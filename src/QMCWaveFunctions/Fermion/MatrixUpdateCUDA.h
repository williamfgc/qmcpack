//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2020 QMCPACK developers.
//
// File developed by: Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//
// File created by: Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//////////////////////////////////////////////////////////////////////////////////////

#ifndef QMCPLUSPLUS_MATRIX_UPDATE_CUDA_H
#define QMCPLUSPLUS_MATRIX_UPDATE_CUDA_H

#include "simd/allocator.hpp"
#include "Platforms/PinnedAllocator.h"
#include "OpenMP/OMPallocator.hpp"
#include "OhmmsPETE/OhmmsVector.h"
#include "OhmmsPETE/OhmmsMatrix.h"
#include "QMCWaveFunctions/Fermion/DiracMatrix.h"
#include "Platforms/OpenMP/ompBLAS.hpp"
#include "Platforms/OpenMP/ompReduction.hpp"
#include <cuda_runtime_api.h>
#include "Numerics/CUDA/cuBLAS.hpp"
#include "Platforms/CUDA/cuBLAS_inhouse.hpp"
#include "QMCWaveFunctions/Fermion/matrix_update_helper.hpp"

namespace qmcplusplus
{
/** implements dirac matrix update using OpenMP
 * @tparam T base precision for most computation
 * @tparam T_FP high precision for matrix inversion, T_FP >= T
 */
template<typename T, typename T_FP>
class MatrixUpdateCUDA
{
  template<typename DT>
  using OffloadAllocator = OMPallocator<DT, aligned_allocator<DT>>;
  template<typename DT>
  using OffloadPinnedAllocator     = OMPallocator<DT, PinnedAlignedAllocator<DT>>;
  using OffloadValueVector_t       = Vector<T, OffloadAllocator<T>>;
  using OffloadPinnedValueVector_t = Vector<T, OffloadPinnedAllocator<T>>;
  using OffloadPinnedValueMatrix_t = Matrix<T, OffloadPinnedAllocator<T>>;

  /// matrix inversion engine
  DiracMatrix<T_FP> detEng;
  /// scratch space for rank-1 update
  OffloadValueVector_t temp;
  /// device pointer of temp
  T* temp_dev_ptr;
  // scratch space for keeping one row of Ainv
  OffloadValueVector_t rcopy;
  /// device pointer of rcopy
  T* rcopy_dev_ptr;
  // constant array value T(1)
  OffloadValueVector_t cone_vec;
  // constant array value T(0)
  OffloadValueVector_t czero_vec;
  // multi walker of grads for transfer needs.
  OffloadPinnedValueMatrix_t grads_value_v;
  // pointer buffer
  Vector<char, OffloadPinnedAllocator<char>> buffer_H2D;

  // CUDA specific variables
  cudaStream_t hstream;
  cublasHandle_t h_cublas;

  inline void waitStream() { cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!"); }

  void resize_fill_constant_arrays(size_t nw)
  {
    if (cone_vec.size() < nw)
    {
      cone_vec.resize(nw);
      czero_vec.resize(nw);
      std::fill_n(cone_vec.data(), nw, T(1));
      std::fill_n(czero_vec.data(), nw, T(0));
      T* cone_ptr = cone_vec.data();
      PRAGMA_OFFLOAD("omp target update to(cone_ptr[:nw])")
      T* czero_ptr = czero_vec.data();
      PRAGMA_OFFLOAD("omp target update to(czero_ptr[:nw])")
    }
  }

  void resize_scratch_arrays(int norb, size_t nw)
  {
    size_t total_size = norb * nw;
    if (temp.size() < total_size)
    {
      temp.resize(total_size);
      rcopy.resize(total_size);
      temp_dev_ptr  = getOffloadDevicePtr(temp.data());
      rcopy_dev_ptr = getOffloadDevicePtr(rcopy.data());
    }
  }

public:
  /// default constructor
  MatrixUpdateCUDA()
  {
    cudaErrorCheck(cudaStreamCreate(&hstream), "cudaStreamCreate failed!");
    //cublasErrorCheck(cublasCreate(&h_cublas), "cublasCreate failed!");
    //cublasErrorCheck(cublasSetStream(h_cublas, hstream), "cublasSetStream failed!");
  }

  ~MatrixUpdateCUDA()
  {
    //cublasErrorCheck(cublasDestroy(h_cublas), "cublasDestroy failed!");
    cudaErrorCheck(cudaStreamDestroy(hstream), "cudaStreamDestroy failed!");
  }

  /** compute the inverse of the transpose of matrix A
   * @param logdetT orbital value matrix
   * @param Ainv inverse matrix
   */
  template<typename TREAL, typename OMPALLOC>
  inline void invert_transpose(const Matrix<T>& logdetT, Matrix<T, OMPALLOC>& Ainv, std::complex<TREAL>& LogValue)
  {
    Matrix<T> Ainv_host_view(Ainv.data(), Ainv.rows(), Ainv.cols());
    detEng.invert_transpose(logdetT, Ainv_host_view, LogValue);
    T* Ainv_ptr = Ainv.data();
    PRAGMA_OFFLOAD("omp target update to(Ainv_ptr[:Ainv.size()])")
  }

  template<typename GT>
  inline void mw_evalGrad(const std::vector<const T*>& invRow_list,
                          const std::vector<const T*>& dpsiM_row_list,
                          int norb,
                          std::vector<GT>& grad_now)
  {
    const int nw = invRow_list.size();
    buffer_H2D.resize(sizeof(T*) * 2 * nw);
    Matrix<const T*> ptr_buffer(reinterpret_cast<const T**>(buffer_H2D.data()), 2, nw);
    for (int iw = 0; iw < nw; iw++)
    {
      ptr_buffer[0][iw] = invRow_list[iw];
      ptr_buffer[1][iw] = dpsiM_row_list[iw];
    }

    constexpr unsigned DIM = GT::Size;
    grads_value_v.resize(nw, DIM);
    auto* __restrict__ grads_value_v_ptr = grads_value_v.data();
    auto* buffer_H2D_ptr                 = buffer_H2D.data();

    PRAGMA_OFFLOAD("omp target teams distribute num_teams(nw) \
                    map(always, to: buffer_H2D_ptr[:buffer_H2D.size()]) \
                    map(always, from: grads_value_v_ptr[:grads_value_v.size()])")
    for (int iw = 0; iw < nw; iw++)
    {
      const T* __restrict__ invRow_ptr    = reinterpret_cast<const T**>(buffer_H2D_ptr)[iw];
      const T* __restrict__ dpsiM_row_ptr = reinterpret_cast<const T**>(buffer_H2D_ptr)[nw + iw];
      T grad_x(0), grad_y(0), grad_z(0);
      PRAGMA_OFFLOAD("omp parallel for reduction(+: grad_x, grad_y, grad_z)")
      for (int iorb = 0; iorb < norb; iorb++)
      {
        grad_x += invRow_ptr[iorb] * dpsiM_row_ptr[iorb * DIM];
        grad_y += invRow_ptr[iorb] * dpsiM_row_ptr[iorb * DIM + 1];
        grad_z += invRow_ptr[iorb] * dpsiM_row_ptr[iorb * DIM + 2];
      }
      grads_value_v_ptr[iw * DIM]     = grad_x;
      grads_value_v_ptr[iw * DIM + 1] = grad_y;
      grads_value_v_ptr[iw * DIM + 2] = grad_z;
    }

    for (int iw = 0; iw < nw; iw++)
      grad_now[iw] = {grads_value_v[iw][0], grads_value_v[iw][1], grads_value_v[iw][2]};
  }

  template<typename VVT, typename RATIOT, typename OMPALLOC>
  inline void updateRow(Matrix<T, OMPALLOC>& Ainv, int rowchanged, const VVT& phiV, RATIOT c_ratio_in)
  {
    // update the inverse matrix
    constexpr T cone(1);
    constexpr T czero(0);
    const int norb = Ainv.rows();
    resize_scratch_arrays(norb, 1);
    // invoke the Fahy's variant of Sherman-Morrison update.
    int dummy_handle  = 0;
    int success       = 0;
    const T* phiV_ptr = phiV.data();
    T* Ainv_ptr       = Ainv.data();
    T* temp_ptr       = temp.data();
    T* rcopy_ptr      = rcopy.data();
    PRAGMA_OFFLOAD("omp target data map(always, to: phiV_ptr[:norb]) \
                    map(always, from: Ainv_ptr[:Ainv.rows()*Ainv.cols()]) \
                    use_device_ptr(phiV_ptr, Ainv_ptr, temp_ptr, rcopy_ptr)")
    {
      success = ompBLAS::gemv(dummy_handle, 'T', norb, norb, cone, Ainv_ptr, norb, phiV_ptr, 1, czero, temp_ptr, 1);
      PRAGMA_OFFLOAD("omp target is_device_ptr(Ainv_ptr, temp_ptr, rcopy_ptr)")
      {
        temp_ptr[rowchanged] -= cone;
        PRAGMA_OFFLOAD("omp parallel for simd")
        for (int i = 0; i < norb; i++)
          rcopy_ptr[i] = Ainv_ptr[rowchanged * norb + i];
      }
      success = ompBLAS::ger(dummy_handle, norb, norb, static_cast<T>(RATIOT(-1) / c_ratio_in), rcopy_ptr, 1, temp_ptr,
                             1, Ainv_ptr, norb);
    }
  }

  inline void mw_updateRow(const std::vector<T*>& Ainv_list,
                           const std::vector<T*>& psiM_g_list,
                           const std::vector<T*>& psiM_l_list,
                           int norb,
                           int rowchanged,
                           const std::vector<bool>& isAccepted,
                           const T* phi_vgl_v_dev_ptr,
                           const size_t phi_vgl_stride,
                           const std::vector<T>& ratios)
  {
    const size_t n_accepted = Ainv_list.size();
    if (n_accepted == 0) return;

    resize_scratch_arrays(norb, n_accepted);

    // to handle T** of Ainv, psi_v, temp, rcopy
    buffer_H2D.resize((sizeof(T*) * 8 + sizeof(T)) * n_accepted);
    Matrix<T*> ptr_buffer(reinterpret_cast<T**>(buffer_H2D.data()), 8, n_accepted);
    T* c_ratio_inv = reinterpret_cast<T*>(buffer_H2D.data() + sizeof(T*) * 8 * n_accepted);
    for (int iw = 0, count = 0; iw < isAccepted.size(); iw++)
      if (isAccepted[iw])
      {
        ptr_buffer[0][count] = Ainv_list[count];
        ptr_buffer[1][count] = const_cast<T*>(phi_vgl_v_dev_ptr + norb * iw);
        ptr_buffer[2][count] = temp_dev_ptr + norb * count;
        ptr_buffer[3][count] = rcopy_dev_ptr + norb * count;
        ptr_buffer[4][count] = psiM_g_list[count];
        ptr_buffer[5][count] = psiM_l_list[count];
        ptr_buffer[6][count] = const_cast<T*>(phi_vgl_v_dev_ptr + phi_vgl_stride + norb * 3 * iw);
        ptr_buffer[7][count] = const_cast<T*>(phi_vgl_v_dev_ptr + phi_vgl_stride * 4 + norb * iw);

        c_ratio_inv[count] = T(-1) / ratios[iw];
        count++;
      }

    // update the inverse matrix
    constexpr T cone(1);
    constexpr T czero(0);
    int dummy_handle     = 0;
    int success          = 0;
    auto* buffer_H2D_ptr = buffer_H2D.data();
    resize_fill_constant_arrays(n_accepted);
    T* cone_ptr  = cone_vec.data();
    T* czero_ptr = czero_vec.data();
    PRAGMA_OFFLOAD("omp target data \
                    map(always, to: buffer_H2D_ptr[:buffer_H2D.size()]) \
                    use_device_ptr(buffer_H2D_ptr, cone_ptr, czero_ptr)")
    {
      T** Ainv_mw_ptr   = reinterpret_cast<T**>(buffer_H2D_ptr);
      T** phiV_mw_ptr   = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted);
      T** temp_mw_ptr   = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 2);
      T** rcopy_mw_ptr  = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 3);
      T** dpsiM_mw_out  = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 4);
      T** d2psiM_mw_out = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 5);
      T** dpsiM_mw_in   = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 6);
      T** d2psiM_mw_in  = reinterpret_cast<T**>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 7);
      T* ratio_inv_mw   = reinterpret_cast<T*>(buffer_H2D_ptr + sizeof(T*) * n_accepted * 8);

      // invoke the Fahy's variant of Sherman-Morrison update.
      cudaErrorCheck(cuBLAS_inhouse::gemv_batched(hstream, 'T', norb, norb, cone_ptr, Ainv_mw_ptr, norb, phiV_mw_ptr, 1,
                                      czero_ptr, temp_mw_ptr, 1, n_accepted), "cuBLAS_inhouse::gemv_batched failed!");

      cudaErrorCheck(CUDA::copyAinvRow_saveGL_cuda(hstream, rowchanged, norb, Ainv_mw_ptr, norb, temp_mw_ptr,
                                    rcopy_mw_ptr, dpsiM_mw_in, d2psiM_mw_in, dpsiM_mw_out, d2psiM_mw_out,
                                    n_accepted), "CUDA::copyAinvRow_saveGL_cud failed!");


      cudaErrorCheck(cuBLAS_inhouse::ger_batched(hstream, norb, norb, ratio_inv_mw, rcopy_mw_ptr, 1, temp_mw_ptr, 1,
                                     Ainv_mw_ptr, norb, n_accepted), "cuBLAS_inhouse::ger_batched failed!");
      waitStream();
    }
  }
};
} // namespace qmcplusplus

#endif // QMCPLUSPLUS_MATRIX_UPDATE_H

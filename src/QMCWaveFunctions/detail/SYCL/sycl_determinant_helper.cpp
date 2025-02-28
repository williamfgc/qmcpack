//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2022 QMCPACK developers.
//
// File developed by: Jeongnim Kim, jeongnim.kim@intel.com, Intel Corp.
//                    Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//
// File created by: Jeongnim Kim, jeongnim.kim@intel.com, Intel Corp.
//////////////////////////////////////////////////////////////////////////////////////


#include "sycl_determinant_helper.hpp"

namespace qmcplusplus
{
template<typename T>
sycl::event applyW_stageV_sycl(sycl::queue& aq,
                               const std::vector<cl::sycl::event>& dependencies,
                               const int* restrict delay_list_gpu,
                               const int delay_count,
                               T* restrict temp_gpu,
                               const int numorbs,
                               const int ndelay,
                               T* restrict V_gpu,
                               const T* restrict Ainv)
{
  const size_t BS = 128;
  const size_t NB = (numorbs + BS - 1) / BS;

  return aq.parallel_for(sycl::nd_range<1>{{BS * NB}, {BS}}, dependencies, [=](sycl::nd_item<1> item) {
    int col = item.get_global_id(0);

    // move rows of Ainv to V
    for (int row = 0; row < delay_count; row++)
    {
      const T* Ainv_row = Ainv + numorbs * delay_list_gpu[row];
      T* V_row          = V_gpu + numorbs * row;
      if (col < numorbs)
        V_row[col] = Ainv_row[col];
    }

    // apply W to temp
    if (col < delay_count)
      temp_gpu[ndelay * delay_list_gpu[col] + col] -= T(1);
  });
}

template sycl::event applyW_stageV_sycl(sycl::queue& aq,
                                        const std::vector<cl::sycl::event>& dependencies,
                                        const int* restrict delay_list_gpu,
                                        const int delay_count,
                                        float* restrict temp_gpu,
                                        const int numorbs,
                                        const int ndelay,
                                        float* restrict V_gpu,
                                        const float* restrict Ainv);

template sycl::event applyW_stageV_sycl(sycl::queue& aq,
                                        const std::vector<cl::sycl::event>& dependencies,
                                        const int* restrict delay_list_gpu,
                                        const int delay_count,
                                        double* restrict temp_gpu,
                                        const int numorbs,
                                        const int ndelay,
                                        double* restrict V_gpu,
                                        const double* restrict Ainv);

template sycl::event applyW_stageV_sycl(sycl::queue& aq,
                                        const std::vector<cl::sycl::event>& dependencies,
                                        const int* restrict delay_list_gpu,
                                        const int delay_count,
                                        std::complex<float>* restrict temp_gpu,
                                        const int numorbs,
                                        const int ndelay,
                                        std::complex<float>* restrict V_gpu,
                                        const std::complex<float>* restrict Ainv);

template sycl::event applyW_stageV_sycl(sycl::queue& aq,
                                        const std::vector<cl::sycl::event>& dependencies,
                                        const int* restrict delay_list_gpu,
                                        const int delay_count,
                                        std::complex<double>* restrict temp_gpu,
                                        const int numorbs,
                                        const int ndelay,
                                        std::complex<double>* restrict V_gpu,
                                        const std::complex<double>* restrict Ainv);
} // namespace qmcplusplus

//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License. See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Ken Esler, kpesler@gmail.com, University of Illinois at
// Urbana-Champaign
//                    Miguel Morales, moralessilva2@llnl.gov, Lawrence Livermore
//                    National Laboratory Jeongnim Kim, jeongnim.kim@gmail.com,
//                    University of Illinois at Urbana-Champaign Jeremy
//                    McMinnis, jmcminis@gmail.com, University of Illinois at
//                    Urbana-Champaign Raymond Clay III, j.k.rofling@gmail.com,
//                    Lawrence Livermore National Laboratory Mark A. Berrill,
//                    berrillma@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Ken Esler, kpesler@gmail.com, University of Illinois at
// Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////

#include "EinsplineSetT.h"

#include "CPU/e2iphi.h"
#include "CPU/math.hpp"
#include "einspline/multi_bspline.h"
#include "type_traits/ConvertToReal.h"

namespace qmcplusplus
{
template<typename T>
EinsplineSetT<T>::EinsplineSetT(const std::string& my_name) : SPOSetT<T>(my_name), TwistNum(0), NumValenceOrbs(0)
{}

template<typename T>
typename EinsplineSetT<T>::UnitCellType EinsplineSetT<T>::GetLattice()
{
  return SuperLattice;
}

template<typename T>
void EinsplineSetT<T>::resetSourceParticleSet(ParticleSet& ions)
{}

template<typename T>
void EinsplineSetT<T>::setOrbitalSetSize(int norbs)
{
  this->OrbitalSetSize = norbs;
}

// Class concrete types from ValueType
template class EinsplineSetT<double>;
template class EinsplineSetT<float>;
template class EinsplineSetT<std::complex<double>>;
template class EinsplineSetT<std::complex<float>>;

} // namespace qmcplusplus

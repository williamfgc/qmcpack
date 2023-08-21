//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License. See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Ken Esler, kpesler@gmail.com, University of Illinois at
// Urbana-Champaign
//                    Miguel Morales, moralessilva2@llnl.gov, Lawrence Livermore
//                    National Laboratory Jeremy McMinnis, jmcminis@gmail.com,
//                    University of Illinois at Urbana-Champaign Jeongnim Kim,
//                    jeongnim.kim@gmail.com, University of Illinois at
//                    Urbana-Champaign Ye Luo, yeluo@anl.gov, Argonne National
//                    Laboratory Mark A. Berrill, berrillma@ornl.gov, Oak Ridge
//                    National Laboratory
//
// File created by: Ken Esler, kpesler@gmail.com, University of Illinois at
// Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////

#ifndef QMCPLUSPLUS_EINSPLINE_SETT_H
#define QMCPLUSPLUS_EINSPLINE_SETT_H

#include "Configuration.h"
#include "QMCWaveFunctions/AtomicOrbital.h"
#include "QMCWaveFunctions/BasisSetBase.h"
#include "QMCWaveFunctions/SPOSetT.h"
#include "Utilities/TimerManager.h"
#include "spline/einspline_engine.hpp"

namespace qmcplusplus
{
template<typename T>
class EinsplineSetT : public SPOSetT<T>
{
public:
  //////////////////////
  // Type definitions //
  //////////////////////
  using PosType      = typename SPOSetT<T>::PosType;
  using RealType     = typename SPOSetT<T>::RealType;
  using FullType     = typename OrbitalSetTraits<double>::RealType;
  using UnitCellType = CrystalLattice<FullType, OHMMS_DIM>;

  EinsplineSetT(const std::string& my_name);

  UnitCellType GetLattice();

  void resetSourceParticleSet(ParticleSet& ions);

  void setOrbitalSetSize(int norbs) override;

  inline std::string Type() { return "EinsplineSetT"; }

  virtual std::string getClassName() const override { return "EinsplineSetT"; }

protected:
  ///////////
  // Flags //
  ///////////
  /// True if all Lattice is diagonal, i.e. 90 degree angles
  bool Orthorhombic;
  /// True if we are using localize orbitals
  bool Localized;
  /// True if we are tiling the primitive cell
  bool Tiling;

  //////////////////////////
  // Lattice and geometry //
  //////////////////////////
  TinyVector<int, 3> TileFactor;
  Tensor<int, OHMMS_DIM> TileMatrix;
  UnitCellType SuperLattice, PrimLattice;
  /// The "Twist" variables are in reduced coords, i.e. from 0 to1.
  /// The "k" variables are in Cartesian coordinates.
  PosType TwistVector, kVector;
  /// This stores which "true" twist vector this clone is using.
  /// "True" indicates the physical twist angle after untiling
  int TwistNum;
  /// metric tensor to handle generic unitcell
  Tensor<RealType, OHMMS_DIM> GGt;

  int NumValenceOrbs;
};

} // namespace qmcplusplus
#endif

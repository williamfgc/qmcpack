//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Ken Esler, kpesler@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeremy McMinnis, jmcminis@gmail.com, University of Illinois at Urbana-Champaign
//                    Mark A. Berrill, berrillma@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Ken Esler, kpesler@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////


#include "ChiesaCorrection.h"
#include "QMCDrivers/WalkerProperties.h"

namespace qmcplusplus
{
void ChiesaCorrection::resetTargetParticleSet(ParticleSet& P) {}

bool ChiesaCorrection::put(xmlNodePtr cur) { return true; }


std::unique_ptr<OperatorBase> ChiesaCorrection::makeClone(ParticleSet& qp, TrialWaveFunction& psi)
{
  return std::make_unique<ChiesaCorrection>(qp, psi);
}

ChiesaCorrection::Return_t ChiesaCorrection::evaluate(ParticleSet& P) { return value_ = psi_ref.KECorrection(); }
#ifdef QMC_CUDA
void ChiesaCorrection::addEnergy(MCWalkerConfiguration& W, std::vector<RealType>& LocalEnergy)
{
  using WP      = WalkerProperties::Indexes;
  RealType corr = psi_ref.KECorrection();
  auto& walkers = W.WalkerList;
  for (int iw = 0; iw < LocalEnergy.size(); iw++)
  {
    walkers[iw]->getPropertyBase()[WP::NUMPROPERTIES + my_index_] = corr;
    LocalEnergy[iw] += corr;
  }
}
#endif
} // namespace qmcplusplus

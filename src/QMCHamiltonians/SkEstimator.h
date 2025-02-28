//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//                    Ken Esler, kpesler@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeremy McMinnis, jmcminis@gmail.com, University of Illinois at Urbana-Champaign
//                    Mark A. Berrill, berrillma@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////


/** @file SkEstimator.h
 * @brief Declare SkEstimator
 */
#ifndef QMCPLUSPLUS_SK_ESTIMATOR_H
#define QMCPLUSPLUS_SK_ESTIMATOR_H
#include "QMCHamiltonians/OperatorBase.h"
namespace qmcplusplus
{
/** SkEstimator evaluate the structure factor of the target particleset
 *
 * <estimator name="sk" type="sk" debug="no"/>
 */
class SkEstimator : public OperatorBase
{
public:
  SkEstimator(ParticleSet& elns);

  void resetTargetParticleSet(ParticleSet& P) override;

  Return_t evaluate(ParticleSet& P) override;

  void addObservables(PropertySetType& plist);
  void addObservables(PropertySetType& plist, BufferType& collectables) override;
  void registerCollectables(std::vector<ObservableHelper>& h5desc, hid_t gid) const override;
  void setObservables(PropertySetType& plist) override;
  void setParticlePropertyList(PropertySetType& plist, int offset) override;
  bool put(xmlNodePtr cur) override;
  bool get(std::ostream& os) const override;
  std::unique_ptr<OperatorBase> makeClone(ParticleSet& qp, TrialWaveFunction& psi) override;

protected:
  ParticleSet* sourcePtcl;
  /** number of species */
  int NumSpecies;
  /** number of kpoints */
  int NumK;
  /** number of kshells */
  int MaxKshell;
  /** normalization factor */
  RealType OneOverN;
  /** kshell counters */
  std::vector<int> Kshell;
  /** instantaneous structure factor  */
  std::vector<RealType> Kmag;
  /** 1.0/degenracy for a ksell */
  std::vector<RealType> OneOverDnk;
  /** \f$rho_k = \sum_{\alpha} \rho_k^{\alpha} \f$ for species index \f$\alpha\f$ */
  Vector<RealType> RhokTot_r, RhokTot_i;
  Vector<RealType> values;
  /** resize the internal data
   *
   * The argument list is not completed
   */
  void resize();

  bool hdf5_out;
};

} // namespace qmcplusplus
#endif

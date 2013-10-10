//////////////////////////////////////////////////////////////////
// (c) Copyright 2003-  by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: jnkim@ncsa.uiuc.edu
//
// Supported by
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
#include "Utilities/OhmmsInfo.h"
#include "QMCWaveFunctions/ElectronGas/ElectronGasComplexOrbitalBuilder.h"
#include "QMCWaveFunctions/Fermion/SlaterDet.h"
#if QMC_BUILD_LEVEL>2
#include "QMCWaveFunctions/Fermion/BackflowTransformation.h"
#endif
#include "OhmmsData/AttributeSet.h"

namespace qmcplusplus
{

/** constructor for EGOSet
 * @param norb number of orbitals for the EGOSet
 * @param k list of unique k points in Cartesian coordinate excluding gamma
 * @param k2 k2[i]=dot(k[i],k[i])
 */
EGOSet::EGOSet(const vector<PosType>& k, const vector<RealType>& k2): K(k), mK2(k2)
{
  KptMax=k.size();
  Identity=true;
  OrbitalSetSize=k.size();
  BasisSetSize=k.size();
  className="EGOSet";
  assign_energies();
}

EGOSet::EGOSet(const vector<PosType>& k, const vector<RealType>& k2, const vector<int>& d)
  : K(k), mK2(k2)
{
  KptMax=k.size();
  Identity=true;
  OrbitalSetSize=k.size();
  BasisSetSize=k.size();
  className="EGOSet";
  assign_energies();
  assign_degeneracies(d);
}

ElectronGasComplexOrbitalBuilder::ElectronGasComplexOrbitalBuilder(ParticleSet& els,
    TrialWaveFunction& psi):
  OrbitalBuilderBase(els,psi)
{
}


bool ElectronGasComplexOrbitalBuilder::put(xmlNodePtr cur)
{
  int nc=0;
  PosType twist(0.0);
  OhmmsAttributeSet aAttrib;
  aAttrib.add(nc,"shell");
  aAttrib.add(twist,"twist");
  aAttrib.put(cur);
  //typedef DiracDeterminant<EGOSet>  Det_t;
  //typedef SlaterDeterminant<EGOSet> SlaterDeterminant_t;
  typedef DiracDeterminantBase  Det_t;
  typedef SlaterDet SlaterDeterminant_t;
  int nat=targetPtcl.getTotalNum();
  int nup=nat/2;
  HEGGrid<RealType,OHMMS_DIM> egGrid(targetPtcl.Lattice);
  if(nc == 0)
    nc = egGrid.getShellIndex(nup);
  egGrid.createGrid(nc,nup,twist);
  targetPtcl.setTwist(twist);
  //create a E(lectron)G(as)O(rbital)Set
  EGOSet* psiu=new EGOSet(egGrid.kpt,egGrid.mk2);
  EGOSet* psid=new EGOSet(egGrid.kpt,egGrid.mk2);
  //create up determinant
  Det_t *updet = new Det_t(psiu);
  updet->set(0,nup);
  //create down determinant
  Det_t *downdet = new Det_t(psid);
  downdet->set(nup,nup);
  //create a Slater determinant
  //SlaterDeterminant_t *sdet  = new SlaterDeterminant_t;
  SlaterDet *sdet  = new SlaterDet(targetPtcl);
  sdet->add(psiu,"u");
  sdet->add(psid,"d");
  sdet->add(updet,0);
  sdet->add(downdet,1);
  //add Slater determinant to targetPsi
  targetPsi.addOrbital(sdet,"SlaterDet",true);
  return true;
}

ElectronGasBasisBuilder::ElectronGasBasisBuilder(ParticleSet& p, xmlNodePtr cur)
  :egGrid(p.Lattice)
{
}

bool ElectronGasBasisBuilder::put(xmlNodePtr cur)
{
  return true;
}

SPOSetBase* ElectronGasBasisBuilder::createSPOSetFromXML(xmlNodePtr cur)
{
  app_log() << "ElectronGasBasisBuilder::createSPOSet " << endl;
  int nc=0;
  int ns=0;
  PosType twist(0.0);
  string spo_name("heg");
  OhmmsAttributeSet aAttrib;
  aAttrib.add(ns,"size");
  aAttrib.add(twist,"twist");
  aAttrib.add(spo_name,"name");
  aAttrib.add(spo_name,"id");
  aAttrib.put(cur);
  if(ns>0)
    nc = egGrid.getShellIndex(ns);
  if (nc<0)
  {
    app_error() << "  HEG Invalid Shell." << endl;
    APP_ABORT("ElectronGasBasisBuilder::put");
  }
  egGrid.createGrid(nc,ns,twist);
  EGOSet* spo = new EGOSet(egGrid.kpt,egGrid.mk2,egGrid.deg);
  vector<int> states;
  states.resize(egGrid.kpt.size());
  for(int s=0;s<egGrid.kpt.size();++s)
    states[s] = s;
  spo->assign_states(states);
  return spo;
}


SPOSetBase* ElectronGasBasisBuilder::createSPOSetFromStates(states_t& states)
{
  egGrid.createGrid(states);
  EGOSet* spo = new EGOSet(egGrid.kpt,egGrid.mk2,egGrid.deg);
  spo->assign_states(states);
  return spo;
}


ElectronGasBasisBuilder::energies_t& ElectronGasBasisBuilder::get_energies()
{
  if(egGrid.kpoints_grid==0)
    APP_ABORT("ElectronGasBasisBuilder::get_energies  kpoint grid does not exist");

  HEGGrid<RealType,OHMMS_DIM>::kpoints_t& kpoints = *egGrid.kpoints_grid;
  energy_list.resize(kpoints.size());

  for(int i=0;i<kpoints.size();++i)
    energy_list[i] = kpoints[i].k2;

  return energy_list;
}


ElectronGasBasisBuilder::degeneracies_t& ElectronGasBasisBuilder::get_degeneracies()
{
  if(egGrid.kpoints_grid==0)
    APP_ABORT("ElectronGasBasisBuilder::get_degeneracies  kpoint grid does not exist");

  HEGGrid<RealType,OHMMS_DIM>::kpoints_t& kpoints = *egGrid.kpoints_grid;
  degeneracy_list.resize(kpoints.size());

  for(int i=0;i<kpoints.size();++i)
    degeneracy_list[i] = kpoints[i].g;

  return degeneracy_list;
}





}
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$
 ***************************************************************************/

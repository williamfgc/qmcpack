//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2022 QMCPACK developers.
//
// File developed by: Miguel Morales, moralessilva2@llnl.gov, Lawrence Livermore National Laboratory
//                    Jeremy McMinnis, jmcminis@gmail.com, University of Illinois at Urbana-Champaign
//                    Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//                    Jaron T. Krogel, krogeljt@ornl.gov, Oak Ridge National Laboratory
//                    Mark A. Berrill, berrillma@ornl.gov, Oak Ridge National Laboratory
//                    Yubo "Paul" Yang, yubo.paul.yang@gmail.com, CCQ @ Flatiron
//
// File created by: Jeongnim Kim, jeongnim.kim@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////

#include "OhmmsData/AttributeSet.h"
#include "LongRange/StructFact.h"
#include "LongRange/KContainerT.h"
#include "QMCWaveFunctions/ElectronGas/FreeOrbitalT.h"
#include "QMCWaveFunctions/ElectronGas/FreeOrbitalBuilderT.h"

namespace qmcplusplus
{
template<typename T>
FreeOrbitalBuilderT<T>::FreeOrbitalBuilderT(ParticleSetT<T>& els, Communicate* comm, xmlNodePtr cur)
    : SPOSetBuilderT<T>("PW", comm), targetPtcl(els)
{}

template<typename T>
std::unique_ptr<SPOSetT<T>> FreeOrbitalBuilderT<T>::createSPOSetFromXML(xmlNodePtr cur)
{
  int norb = -1;
  std::string spo_object_name;
  PosType twist(0.0);
  OhmmsAttributeSet attrib;
  attrib.add(norb, "size");
  attrib.add(twist, "twist");
  attrib.add(spo_object_name, "name");
  attrib.put(cur);

  if (norb < 0)
    throw std::runtime_error("free orbital SPO set require the \"size\" input");

  auto lattice = targetPtcl.getLattice();

  PosType tvec = lattice.k_cart(twist);
#ifdef QMC_COMPLEX
  const int npw = norb;
  targetPtcl.setTwist(twist);
  app_log() << "twist fraction = " << twist << std::endl;
  app_log() << "twist cartesian = " << tvec << std::endl;
#else
  const int npw = std::ceil((norb + 1.0) / 2);
  if (2 * npw - 1 != norb)
  {
    std::ostringstream msg;
    msg << "norb = " << norb << " npw = " << npw;
    msg << " cannot be ran in real PWs (sin, cos)" << std::endl;
    msg << "either use complex build or change the size of SPO set" << std::endl;
    msg << "ideally, set size to a closed shell of PWs." << std::endl;
    throw std::runtime_error(msg.str());
  }
  for (int ldim = 0; ldim < twist.size(); ldim++)
  {
    if (std::abs(twist[ldim]) > 1e-16)
      throw std::runtime_error("no twist for real orbitals");
  }
#endif

  // extract npw k-points from container
  // kpts_cart is sorted by magnitude
  std::vector<PosType> kpts(npw);
  KContainerT<T> klists;
  RealType kcut = lattice.LR_kc; // to-do: reduce kcut to >~ kf
  klists.updateKLists(lattice, kcut, lattice.ndim, twist);

  // k0 is not in kpts_cart
  kpts[0] = tvec;
#ifdef QMC_COMPLEX
  for (int ik = 1; ik < npw; ik++)
  {
    kpts[ik] = klists.kpts_cart[ik - 1];
  }
#else
  const int nktot = klists.kpts.size();
  std::vector<int> mkidx(npw, 0);
  int ik = 1;
  for (int jk = 0; jk < nktot; jk++)
  {
    // check if -k is already chosen
    const int jmk = klists.minusk[jk];
    if (in_list(jk, mkidx))
      continue;
    // if not, then add this kpoint
    kpts[ik]  = klists.kpts_cart[jk];
    mkidx[ik] = jmk; // keep track of its minus
    ik++;
    if (ik >= npw)
      break;
  }
#endif
  auto sposet = std::make_unique<FreeOrbitalT<T>>(spo_object_name, kpts);
  sposet->report("  ");
  return sposet;
}

template<typename T>
bool FreeOrbitalBuilderT<T>::in_list(const int j, const std::vector<int> l)
{
  for (int i = 0; i < l.size(); i++)
  {
    if (j == l[i])
      return true;
  }
  return false;
}

template class FreeOrbitalBuilderT<double>;
template class FreeOrbitalBuilderT<float>;
template class FreeOrbitalBuilderT<std::complex<double>>;
template class FreeOrbitalBuilderT<std::complex<float>>;

} // namespace qmcplusplus

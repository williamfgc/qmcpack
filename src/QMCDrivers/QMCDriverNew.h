//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2020 QMCPACK developers.
//
// File developed by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Laboratory
//
// File refactored from: QMCDriver.h
//////////////////////////////////////////////////////////////////////////////////////


/**
 * @file
 * Declaration of QMCDriverNew
 *
 * This will replace QMCDriver once unified drivers are finished
 * the general documentation from QMCDriver.h must be moved before then
 *  
 * This driver base class should be generic with respect to precision,
 * value type, device execution, and ...
 * It should contain no typdefs not related to compiler bugs or platform workarounds
 *
 */

#ifndef QMCPLUSPLUS_QMCDRIVERNEW_H
#define QMCPLUSPLUS_QMCDRIVERNEW_H

#include <type_traits>

#include "Configuration.h"
#include "Particle/HDFWalkerIO.h"
#include "Pools/PooledData.h"
#include "Utilities/TimerManager.h"
#include "Utilities/ScopedProfiler.h"
#include "QMCDrivers/MCPopulation.h"
#include "QMCDrivers/QMCDriverInterface.h"
#include "QMCDrivers/GreenFunctionModifiers/DriftModifierBase.h"
#include "QMCDrivers/QMCDriverInput.h"
#include "QMCDrivers/ContextForSteps.h"
#include "ProjectData.h"
#include "DriverWalkerTypes.h"
#include "TauParams.hpp"
#include "Particle/MCCoords.hpp"
#include "WalkerLogInput.h"
#include <algorithm>

class Communicate;

namespace qmcplusplus
{
//forward declarations: Do not include headers if not needed
class TraceManager;
class EstimatorManagerNew;
class TrialWaveFunction;
class QMCHamiltonian;

namespace testing
{
class DMCBatchedTest;
class VMCBatchedTest;
class QMCDriverNewTestWrapper;
} // namespace testing

/** @ingroup QMCDrivers
 * @{
 * @brief QMCDriverNew Base class for Unified Drivers
 *
 * # General Principals
 * * Parameters used unchanged from input object are not copied into class state
 * * The driver state machine should be as minimal as possible.
 * * In non performance critical areas favor clarity over clever optimizations.
 */
class QMCDriverNew : public QMCDriverInterface, public MPIObjectBase
{
public:
  using RealType         = QMCTraits::RealType;
  using IndexType        = QMCTraits::IndexType;
  using FullPrecRealType = QMCTraits::FullPrecRealType;
  /** separate but similar to QMCModeEnum
   *  
   *  a code smell
   */
  enum
  {
    QMC_UPDATE_MODE,
    QMC_MULTIPLE,
    QMC_OPTIMIZE,
    QMC_WARMUP
  };

  using MCPWalker = MCPopulation::MCPWalker;
  using WFBuffer  = MCPopulation::WFBuffer;

  using SetNonLocalMoveHandler = std::function<void(QMCHamiltonian&)>;
  /** bits to classify QMCDriver
   *
   * - qmc_driver_mode[QMC_UPDATE_MODE]? particle-by-particle: walker-by-walker
   * - qmc_driver_mode[QMC_MULTIPLE]? multiple H/Psi : single H/Psi
   * - qmc_driver_mode[QMC_OPTIMIZE]? optimization : vmc/dmc/rmc
   */
  std::bitset<QMC_MODE_MAX> qmc_driver_mode_;

  /// whether to allow walker logs
  bool allow_walker_logs;
  /// walker logs input
  WalkerLogInput walker_logs_input;
  //xmlNodePtr walker_logs_xml;

protected:
  /** This is a data structure strictly for QMCDriver and its derived classes
   *
   *  i.e. its nested in scope for a reason
   */
  struct AdjustedWalkerCounts
  {
    IndexType global_walkers;
    std::vector<IndexType> walkers_per_rank;
    std::vector<IndexType> walkers_per_crowd;
    RealType reserve_walkers;
  };
  /** Do common section starting tasks for VMC and DMC
   *
   * set up population_, crowds_, rngs and step_contexts_
   */
  void initializeQMC(const AdjustedWalkerCounts& awc);

  /// inject additional barrier and measure load imbalance.
  void measureImbalance(const std::string& tag) const;
  /// end of a block operations. Aggregates statistics across all MPI ranks and write to disk.
  void endBlock();

public:
  /// Constructor.
  QMCDriverNew(const ProjectData& project_data,
               QMCDriverInput&& input,
               const std::optional<EstimatorManagerInput>& global_emi,
               WalkerConfigurations& wc,
               MCPopulation&& population,
               const std::string timer_prefix,
               Communicate* comm,
               const std::string& QMC_driver_type);

  ///Move Constructor
  QMCDriverNew(QMCDriverNew&&) = default;
  ///Copy Constructor (disabled).
  QMCDriverNew(const QMCDriverNew&) = delete;
  ///Copy operator (disabled).
  QMCDriverNew& operator=(const QMCDriverNew&) = delete;

  ~QMCDriverNew() override;

  bool putQMCInfo(xmlNodePtr cur);

  /** Adjust populations local walkers to this number
  * @param nwalkers number of walkers to add
  *
  */
  void makeLocalWalkers(int nwalkers, RealType reserve);

  DriftModifierBase& get_drift_modifier() const { return *drift_modifier_; }

  /** record the state of the block
   * @param block current block
   *
   * virtual function with a default implementation
   */
  void recordBlock(int block) override;

  /** finalize a qmc section
   * @param block current block
   * @param dumpwalkers if true, dump walkers
   *
   * Accumulate energy and weight is written to a hdf5 file.
   * Finialize the estimators
   */
  bool finalize(int block, bool dumpwalkers = true);

  ///return current step
  inline IndexType current() const { return current_step_; }

  /** Set the status of the QMCDriver
   * @param aname the root file name, ignored
   * @param h5name root name of the master hdf5 file containing previous qmcrun
   * @param append if true, the run is a continuation of the previous qmc
   *
   * All output files will be of
   * the form "aname.s00X.suffix", where "X" is number
   * of previous QMC runs for the simulation and "suffix"
   * is the suffix for the output file.
   */
  void setStatus(const std::string& aname, const std::string& h5name, bool append) override;

  void add_H_and_Psi(QMCHamiltonian* h, TrialWaveFunction* psi) override {};

  void createRngsStepContexts(int num_crowds);

  void putWalkers(std::vector<xmlNodePtr>& wset) override;

  inline RefVector<RandomBase<FullPrecRealType>> getRngRefs() const
  {
    RefVector<RandomBase<FullPrecRealType>> RngRefs;
    for (int i = 0; i < Rng.size(); ++i)
      RngRefs.push_back(*Rng[i]);
    return RngRefs;
  }

  ///return the i-th random generator
  inline RandomBase<FullPrecRealType>& getRng(int i) override { return (*Rng[i]); }

  /** intended for logging output and debugging
   *  you should base behavior on type preferably at compile time or if
   *  necessary at runtime using and protected by dynamic cast.
   *  QMCType is primarily for use in the debugger.
   */
  std::string getEngineName() override { return QMCType; }
  unsigned long getDriverMode() override { return qmc_driver_mode_.to_ulong(); }

  IndexType get_num_living_walkers() const { return population_.get_walkers().size(); }
  IndexType get_num_dead_walkers() const { return population_.get_dead_walkers().size(); }

  const QMCDriverInput& getQMCDriverInput() const { return qmcdriver_input_; }

  /** @ingroup Legacy interface to be dropped
   *  @{
   */
  bool put(xmlNodePtr cur) override { return false; };

  /** QMCDriverNew driver second (3rd, 4th...) stage of constructing a valid driver
   *
   *  This is the shared entry point with legacy,
   *  from QMCMain so the API cannot be updated yet
   *
   *  \todo remove cur, the driver and all its child nodes should be completely processed before
   *        this stage of driver initialization is hit.
   */
  void process(xmlNodePtr cur) override = 0;

  static void initialLogEvaluation(int crowd_id,
                                   UPtrVector<Crowd>& crowds,
                                   UPtrVector<ContextForSteps>& step_context,
                                   const bool serializing_crowd_walkers);


  /** should be set in input don't see a reason to set individually
   * @param pbyp if true, use particle-by-particle update
   */
  inline void setUpdateMode(bool pbyp) override { qmc_driver_mode_[QMC_UPDATE_MODE] = pbyp; }

  void putTraces(xmlNodePtr txml) override {}
  void requestTraces(bool allow_traces) override {}

  void putWalkerLogs(xmlNodePtr wlxml) override;

  void requestWalkerLogs(bool allow_walker_logs_) override { allow_walker_logs = allow_walker_logs_; }

  // scales a MCCoords by sqrtTau. Chooses appropriate taus by CT
  template<typename RT, CoordsType CT>
  static void scaleBySqrtTau(const TauParams<RT, CT>& taus, MCCoords<CT>& coords)
  {
    for (auto& pos : coords.positions)
      pos *= taus.sqrttau;
    if constexpr (CT == CoordsType::POS_SPIN)
      for (auto& spin : coords.spins)
        spin *= taus.spin_sqrttau;
  }

  /** calculates Green Function from displacements stored in MCCoords
     * [param, out] log_g
     */
  template<typename RT, CoordsType CT>
  static void computeLogGreensFunction(const MCCoords<CT>& coords,
                                       const TauParams<RT, CT>& taus,
                                       std::vector<QMCTraits::RealType>& log_gb)
  {
    assert(coords.positions.size() == log_gb.size());
    std::transform(coords.positions.begin(), coords.positions.end(), log_gb.begin(),
                   [halfovertau = taus.oneover2tau](const QMCTraits::PosType& pos) {
                     return -halfovertau * dot(pos, pos);
                   });
    if constexpr (CT == CoordsType::POS_SPIN)
      std::transform(coords.spins.begin(), coords.spins.end(), log_gb.begin(), log_gb.begin(),
                     [halfovertau = taus.spin_oneover2tau](const QMCTraits::FullPrecRealType& spin,
                                                           const QMCTraits::RealType& loggb) {
                       return loggb - halfovertau * spin * spin;
                     });
  }

  /** }@ */

protected:
  /** pure function returning AdjustedWalkerCounts data structure 
   *
   *  The logic is now walker counts is fairly simple.
   *  TotalWalkers trumps all other walker parameters
   *  If TotalWalkers is absent walkers_per_rank is used.
   *  if they are both absent then the default is one walker per crowd,
   *  each rank has crowds walkers.
   *  if crowds aren't specified you get one per main level thread.
   *
   *  You can have crowds or ranks with no walkers.
   *  You cannot have more crowds than threads.
   *
   *  passing num_ranks instead of internally querying comm->size()
   *  makes unit testing much quicker.
   *
   */
  static QMCDriverNew::AdjustedWalkerCounts adjustGlobalWalkerCount(Communicate& comm,
                                                                    const IndexType current_configs,
                                                                    const IndexType requested_total_walkers,
                                                                    const IndexType requested_walkers_per_rank,
                                                                    const RealType reserve_walkers,
                                                                    int num_crowds);

  /** pure function calculating the actual number of steps per block
   *
   * @param global_walkers the total number of walkers over all the MPI ranks
   * @param requested_samples the number of samples from user input "samples". <=0 treated as no input
   * @param requested_steps the number steps per block from user input "steps". <=0 treated as no input
   * @param blocks the number of blocks. Must be positive.
   * @return calculated optimal number of steps per block
   */
  static size_t determineStepsPerBlock(IndexType global_walkers,
                                       IndexType requested_samples,
                                       IndexType requested_steps,
                                       IndexType blocks);

  static void checkNumCrowdsLTNumThreads(const int num_crowds);

  /// check logpsi and grad and lap against values computed from scratch
  static void checkLogAndGL(Crowd& crowd, const std::string_view location, const bool serializing_crowd_walkers);

  const std::string& get_root_name() const override { return project_data_.currentMainRoot(); }

  /** The timers for the driver.
   *
   * This cleans up the driver constructor, and a reference to this structure 
   * Takes the timers into thread scope. We assume the timers are threadsafe.
   */
  struct DriverTimers
  {
    NewTimer& checkpoint_timer;
    NewTimer& run_steps_timer;
    NewTimer& create_walkers_timer;
    NewTimer& init_walkers_timer;
    NewTimer& buffer_timer;
    NewTimer& movepbyp_timer;
    NewTimer& hamiltonian_timer;
    NewTimer& collectables_timer;
    NewTimer& estimators_timer;
    NewTimer& imbalance_timer;
    NewTimer& endblock_timer;
    NewTimer& startup_timer;
    NewTimer& production_timer;
    NewTimer& resource_timer;
    DriverTimers(const std::string& prefix)
        : checkpoint_timer(createGlobalTimer(prefix + "CheckPoint", timer_level_medium)),
          run_steps_timer(createGlobalTimer(prefix + "RunSteps", timer_level_medium)),
          create_walkers_timer(createGlobalTimer(prefix + "CreateWalkers", timer_level_medium)),
          init_walkers_timer(createGlobalTimer(prefix + "InitWalkers", timer_level_medium)),
          buffer_timer(createGlobalTimer(prefix + "Buffer", timer_level_medium)),
          movepbyp_timer(createGlobalTimer(prefix + "MovePbyP", timer_level_medium)),
          hamiltonian_timer(createGlobalTimer(prefix + "Hamiltonian", timer_level_medium)),
          collectables_timer(createGlobalTimer(prefix + "Collectables", timer_level_medium)),
          estimators_timer(createGlobalTimer(prefix + "Estimators", timer_level_medium)),
          imbalance_timer(createGlobalTimer(prefix + "Imbalance", timer_level_medium)),
          endblock_timer(createGlobalTimer(prefix + "BlockEndDataAggregation", timer_level_medium)),
          startup_timer(createGlobalTimer(prefix + "Startup", timer_level_medium)),
          production_timer(createGlobalTimer(prefix + "Production", timer_level_medium)),
          resource_timer(createGlobalTimer(prefix + "Resources", timer_level_medium))
    {}
  };

  QMCDriverInput qmcdriver_input_;

  /** @ingroup Driver mutable input values
   *
   *  they should be limited to values that can be changed from input
   *  or are live state.
   *  @{
   */
  RealType max_disp_sq_;
  ///the number of saved samples
  IndexType target_samples_;

  /// the number of blocks between recomptePsi
  IndexType nBlocksBetweenRecompute;

  /**}@*/

  UPtrVector<Crowd> crowds_;

  std::string h5_file_root_;

  ///drift modifer
  std::unique_ptr<DriftModifierBase> drift_modifier_;

  ///the number to delay updates by
  int k_delay;

  /** period of recording walker configurations
   *
   * Default is 0 indicating that only the last configuration will be saved.
   */
  int walker_dump_period;

  IndexType current_step_;
  /// actual number of steps per block
  size_t steps_per_block_ = 0;

  ///counter for number of moves accepted
  IndexType nAccept;

  ///counter for number of moves /rejected
  IndexType nReject;

  ///Time-step factor \f$ 1/(2\tau)\f$
  RealType m_oneover2tau;
  ///Time-step factor \f$ \sqrt{\tau}\f$
  RealType m_sqrttau;

  ///type of qmc: assigned by subclasses
  const std::string QMCType;

  /** the entire (on node) walker population
   * it serves VMCBatch and DMCBatch right now but will be polymorphic
   */
  MCPopulation population_;

  /** the golden multi walker shared resource
   * serves ParticleSet TrialWaveFunction right now but actually should be based on MCPopulation.
   * per crowd resources are copied from this gold instance
   * it should be activated when dispatchers don't serialize walkers
   */
  struct DriverWalkerResourceCollection golden_resource_;

  /// if true, calculating walker one-by-one within a crowd
  const bool serializing_crowd_walkers_;

  /** Observables manager
   *  Has very problematic owner ship and life cycle.
   *  Can be transferred via branch manager one driver to the next indefinitely
   *  TODO:  Modify Branch manager and others to clear this up.
   */
  std::unique_ptr<EstimatorManagerNew> estimator_manager_;

  ///record engine for walkers
  std::unique_ptr<HDFWalkerOutput> wOut;

  /** Per crowd move contexts, this is where the DistanceTables etc. reside
   */
  UPtrVector<ContextForSteps> step_contexts_;

  ///Random number generators
  UPtrVector<RandomBase<FullPrecRealType>> Rng;

  ///a list of mcwalkerset element
  std::vector<xmlNodePtr> mcwalkerNodePtr;

  // ///alternate method of setting QMC run parameters
  // IndexType nStepsBetweenSamples;
  // ///samples per thread
  // IndexType nSamplesPerThread;

  //  TODO: restart
  //  /** period of dumping walker configurations and everything else for restart
  //  *
  //  * The unit is a block.
  //  */
  // int check_point_period_;

  /** }@ */

  DriverTimers timers_;

  ///profile the driver lifetime
  ScopedProfiler driver_scope_profiler_;

  /// project info for accessing global fileroot and series id
  const ProjectData& project_data_;

  // reference to the captured WalkerConfigurations
  WalkerConfigurations& walker_configs_ref_;

  /// update the global offsets of walker configurations after active walkers being touched.
  static void setWalkerOffsets(WalkerConfigurations&, Communicate* comm);

private:
  friend std::ostream& operator<<(std::ostream& o_stream, const QMCDriverNew& qmcd);

  friend class qmcplusplus::testing::VMCBatchedTest;
  friend class qmcplusplus::testing::DMCBatchedTest;
  friend class qmcplusplus::testing::QMCDriverNewTestWrapper;
};
} // namespace qmcplusplus

#endif

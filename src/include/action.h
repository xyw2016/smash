/*
 *
 *    Copyright (c) 2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_ACTION_H_
#define SRC_INCLUDE_ACTION_H_

#include <stdexcept>
#include <vector>
#include <memory>

#include "particles.h"
#include "processbranch.h"

namespace Smash {

/**
 * Action is the base class for a generic process that takes a number of
 * incoming particles and transforms them into any number of outgoing particles.
 * Currently such an action can be either a decay or a two-body collision
 * (see derived classes).
 */
class Action {
 public:
  /**
   * Construct an action object.
   *
   * \param[in] in_part list of incoming particles
   * \param[in] time_of_execution time at which the action is supposed to take place
   */
  Action(const ParticleList &in_part, float time_of_execution);
  /** Destructor. */
  virtual ~Action();

  /** For sorting by time of execution. */
  bool operator<(const Action &rhs) const {
    return time_of_execution_ < rhs.time_of_execution_;
  }

  /** Returns the total weight, which is a cross section in case of a ScatterAction
   * and a decay width in case of a DecayAction. */
  float weight() const;

  /** Add a new subprocess.  */
  void add_process(const ProcessBranch &p);
  /** Add several new subprocesses at once.  */
  void add_processes(const ProcessBranchList &pv);

  /**
   * Actually perform the action, e.g. carry out a decay or scattering.
   *
   * This method does not do any sanity checks, but assumes that is_valid has
   * been called to determine if the action is still valid.
   */
  virtual void perform(Particles *particles, size_t &id_process) = 0;

  /**
   * Check whether the action still applies.
   *
   * It can happen that a different action removed the incoming_particles from
   * the set of existing particles in the experiment, or that the particle has
   * scattered elastically in the meantime. In this case the Action doesn't
   * apply anymore and should be discarded.
   */
  bool is_valid(const Particles &) const;

  /**
   * Return the list of particles that go into the interaction.
   */
  ParticleList incoming_particles() const;

  /**
   * Return the list of particles that resulted from the interaction.
   */
  const ParticleList &outgoing_particles() const { return outgoing_particles_; }

  /** Check various conservation laws. */
  void check_conservation(const size_t &id_process) const;

  /**
   * Thrown for example when ScatterAction is called to perform with a wrong
   * number of final-state particles or when the energy is too low to produce
   * the resonance.
   */
  class InvalidResonanceFormation : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
  };

 protected:
  /** List with data of incoming particles.  */
  ParticleList incoming_particles_;
  /** time at which the action is supposed to be performed  */
  float time_of_execution_;
  /** list of possible subprocesses  */
  std::vector<ProcessBranch> subprocesses_;
  /** sum of all subprocess weights  */
  float total_weight_;
  /**
   * Initially this stores only the PDG codes of final-state particles.
   *
   * After perform was called it contains the complete particle data of the
   * outgoing particles.
   */
  ParticleList outgoing_particles_;
  /// determine the total energy in the center-of-mass frame
  virtual double sqrt_s() const = 0;

  /**
   * Decide for a particular final-state channel via Monte-Carlo
   * and return it as a list of particles that are only initialized
   * with their PDG code.
   */
  ParticleList choose_channel();

  /**
   * Sample final state momenta (and masses) in general X->2 process.
   *
   * \throws InvalidResonanceFormation
   */
  void sample_cms_momenta();
};


/**
 * DecayAction is a special action which takes one single particle in the
 * initial state and makes it decay into a number of daughter particles
 * (currently two or three).
 */
class DecayAction : public Action {
 public:
  /**
   * Construct a DecayAction object.
   *
   * \param[in] in_part decaying particle
   * \param[in] time_of_execution time at which the action is supposed to take place
   */
  DecayAction(const ParticleData &in_part, float time_of_execution);
  /** Construct a DecayAction from a particle p.
   *
   * Sets up the full list of possible decay processes.
   *
   * \param[in] p Data of decaying particle. We use its ID and mass.
   */
  DecayAction(const ParticleData &p);

  /** Carry out the action, i.e. do the decay.
   * Performs a decay of one particle to two or three particles.
   *
   * \throws InvalidDecay
   */
  void perform(Particles *particles, size_t &id_process);

  /**
   * Thrown when DecayAction is called to perform with 0 or more than 2
   * entries in outgoing_particles.
   */
  class InvalidDecay : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
  };

 protected:
  /// determine the total energy in the center-of-mass frame
  double sqrt_s() const;

 private:

  /**
   * Kinematics of a 1-to-2 decay process.
   *
   * Sample the masses and momenta of the decay products in the
   * center-of-momentum frame.
   */
  void one_to_two();

  /**
   * Kinematics of a 1-to-3 decay process.
   *
   * Sample the masses and momenta of the decay products in the
   * center-of-momentum frame.
   */
  void one_to_three();
};


/**
 * ScatterAction is a special action which takes two incoming particles
 * and performs a scattering, producing one or more final-state particles.
 */
class ScatterAction : public Action {
 public:
  /**
   * Construct a ScatterAction object.
   *
   * \param[in] in_part1 first scattering partner
   * \param[in] in_part2 second scattering partner
   * \param[in] time_of_execution time at which the action is supposed to take place
   */
  ScatterAction(const ParticleData &in_part1, const ParticleData &in_part2,
                float time_of_execution);

  /**
   * Measure distance between incoming particles in center-of-momentum frame.
   * Returns the squared distance.
   */
  double particle_distance() const;

  /**
   * Carry out the action, i.e. do the scattering.
   * Performs either elastic or inelastic scattering.
   *
   * \throws InvalidResonanceFormation
   */
  void perform(Particles *particles, size_t &id_process);

  /**
   * Determine the elastic cross section for this collision. It is given by a
   * parametrization of exp. data for BB collisions and is constant for
   * BM and MM collisions.
   *
   * \param[in] elast_par Elastic cross section parameter from the input file
   * (currently only used for BM and MM collisions).
   * 
   * \return A ProcessBranch object containing the cross section and
   * final-state IDs.
   */
  ProcessBranch elastic_cross_section(float elast_par);

  /**
  * Find all resonances that can be produced in a collision of the two
  * input particles and the production cross sections of these resonances.
  *
  * Given the data and type information of two colliding particles,
  * create a list of possible resonance production processes
  * and their cross sections.
  * Process can be either 2-to-1 (just a resonance in the final state)
  * or 2-to-2 (resonance and a stable particle in the final state).
  *
  * \return A list of processes with resonance in the final state.
  * Each element in the list contains the type(s)
  * of the final state particle(s)
  * and the cross section for that particular process.
  */
  ProcessBranchList resonance_cross_section();

 protected:
  /// determine the total energy in the center-of-mass frame
  double sqrt_s() const;

 private:
  /// determine the velocity of the center-of-mass frame in the lab
  ThreeVector beta_cm() const;

  /** Check if the scattering is elastic. */
  bool is_elastic() const;

  /** Perform an elastic two-body scattering, i.e. just exchange momentum. */
  void momenta_exchange();

  /**
   * Resonance formation process.
   *
   * Creates one or two new particles, one of which is a resonance.
   */
  void resonance_formation();
};


using ActionPtr = std::unique_ptr<Action>;
using ScatterActionPtr = std::unique_ptr<ScatterAction>;

inline std::vector<ActionPtr> &operator+=(std::vector<ActionPtr> &lhs,
                                          std::vector<ActionPtr> &&rhs) {
  if (lhs.size() == 0) {
    lhs = std::move(rhs);
  } else {
    lhs.insert(lhs.end(), std::make_move_iterator(rhs.begin()),
               std::make_move_iterator(rhs.end()));
  }
  return lhs;
}

}  // namespace Smash

#endif  // SRC_INCLUDE_ACTION_H_

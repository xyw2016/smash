/*
 *
 *    Copyright (c) 2014-2015
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_ACTION_H_
#define SRC_INCLUDE_ACTION_H_

#include <memory>
#include <stdexcept>
#include <vector>

#include "density.h"
#include "experimentparameters.h"
#include "pauliblocking.h"
#include "particles.h"
#include "processbranch.h"
#include "random.h"

namespace Smash {

/**
 * \ingroup action
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

  /** Copying is disabled. Use pointers or create a new Action. */
  Action(const Action &) = delete;

  /** Virtual Destructor.
   * The declaration of the destructor is necessary to make it virtual.
   */
  virtual ~Action();

  /** For sorting by time of execution. */
  bool operator<(const Action &rhs) const {
    return time_of_execution_ < rhs.time_of_execution_;
  }

  /** Return the raw weight value, which is a cross section in case of a
   * ScatterAction and a decay width in case of a DecayAction.
   *
   * Prefer to use a more specific function.
   */
  virtual float raw_weight_value() const = 0;

  /** Return the process type. */
  ProcessType get_type() const {
    return process_type_;
  }

  /** Add a new subprocess.  */
  template<typename Branch>
  void add_process(ProcessBranchPtr<Branch> &p,
                   ProcessBranchList<Branch>& subprocesses,
      float& total_weight) {
    if (p->weight() > really_small) {
      total_weight += p->weight();
      subprocesses.emplace_back(std::move(p));
    }
  }
  /** Add several new subprocesses at once.  */
  template<typename Branch>
  void add_processes(ProcessBranchList<Branch> pv,
      ProcessBranchList<Branch>& subprocesses, float& total_weight) {
    if (subprocesses.empty()) {
      subprocesses = std::move(pv);
      for (auto &proc : subprocesses) {
        total_weight += proc->weight();
      }
    } else {
      subprocesses.reserve(subprocesses.size() + pv.size());
      for (auto &proc : pv) {
        total_weight += proc->weight();
        subprocesses.emplace_back(std::move(proc));
      }
    }
  }

  /**
   * Generate the final state for this action.
   *
   * This function selects a subprocess by Monte-Carlo decision and sets up
   * the final-state particles in phase space.
   */
  virtual void generate_final_state() = 0;

  /**
   * Actually perform the action, e.g. carry out a decay or scattering by
   * updating the particle list.
   *
   * This function removes the initial-state particles from the particle list
   * and then inserts the final-state particles. It does not do any sanity
   * checks, but assumes that is_valid has been called to determine if the
   * action is still valid.
   */
  virtual void perform(Particles *particles, size_t &id_process);

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
   *  Check if the action is pauli-bloked. If there are baryons in the final
   *  state then blocking probability is \f$ 1 - \Pi (1-f_i) \f$, where
   *  product is taken by all fermions in the final state and \f$ f_i \f$
   *  denotes phase-space density at the position of i-th final-state
   *  fermion.
   */
  bool is_pauli_blocked(const Particles &, const PauliBlocker &) const;

  /**
   * Return the list of particles that go into the interaction.
   */
  const ParticleList& incoming_particles() const;

  /**
   * Return the list of particles that resulted from the interaction.
   */
  const ParticleList &outgoing_particles() const { return outgoing_particles_; }

  /** Check various conservation laws. */
  void check_conservation(const size_t &id_process) const;

  /** Get the interaction point */
  FourVector get_interaction_point();

  /**
   * \ingroup exception
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
  /**
   * Initially this stores only the PDG codes of final-state particles.
   *
   * After perform was called it contains the complete particle data of the
   * outgoing particles.
   */
  ParticleList outgoing_particles_;
  /** time at which the action is supposed to be performed  */
  float time_of_execution_;
  /** type of process */
  ProcessType process_type_;

  /// determine the total energy in the center-of-mass frame
  /// \fpPrecision Why \c double?
  virtual double sqrt_s() const = 0;

  /**
   * Decide for a particular final-state channel via Monte-Carlo
   * and return it as a ProcessBranch
   */
  template<typename Branch>
  const Branch* choose_channel(
      const ProcessBranchList<Branch>& subprocesses, float total_weight) {
    const auto &log = logger<LogArea::Action>();
    float random_weight = Random::uniform(0.f, total_weight);
    float weight_sum = 0.;
    /* Loop through all subprocesses and select one by Monte Carlo, based on
     * their weights.  */
    for (const auto &proc : subprocesses) {
      /* All processes apart from strings should have
       * a well-defined final state. */
      if (proc->particle_number() < 1
          && proc->get_type() != ProcessType::String) {
        continue;
      }
      weight_sum += proc->weight();
      if (random_weight <= weight_sum) {
        /* Return the full process information. */
         return proc.get();
      }
    }
    /* Should never get here. */
    log.fatal(source_location, "Problem in choose_channel: ",
              subprocesses.size(), " ", weight_sum, " ", total_weight, " ",
    //          random_weight, "\n", *this);
              random_weight, "\n");
    throw std::runtime_error("problem in choose_channel");
  }

  /**
   * Sample final state momenta (and masses) in general X->2 process.
   *
   * \throws InvalidResonanceFormation
   */
  void sample_cms_momenta();

  /**
   * \ingroup logging
   * Writes information about this action to the \p out stream.
   */
  virtual void format_debug_output(std::ostream &out) const = 0;

  /**
   * \ingroup logging
   * Dispatches formatting to the virtual Action::format_debug_output function.
   */
  friend std::ostream &operator<<(std::ostream &out, const Action &action) {
    action.format_debug_output(out);
    return out;
  }
};


/**
 * \ingroup action
 * DecayAction is a special action which takes one single particle in the
 * initial state and makes it decay into a number of daughter particles
 * (currently two or three).
 */
class DecayAction : public Action {
 public:
  /**
   * Construct a DecayAction from a particle \p p.
   *
   * It does not initialize the list of possible decay processes. You need to
   * call add_processes after construction.
   *
   * \param[in] p The particle that should decay if the action is performed.
   * \param[in] time_of_execution time at which the action is supposed to take place
   */
  DecayAction(const ParticleData &p, float time_of_execution);

  /** Add several new decays at once. */
  void add_decays(DecayBranchList pv);

  /** Generate the final state of the decay process.
   * Performs a decay of one particle to two or three particles.
   *
   * \throws InvalidDecay
   */
  void generate_final_state() override;

  float raw_weight_value() const override;

  float total_width() const {
    return total_width_;
  }

  /**
   * \ingroup exception
   * Thrown when DecayAction is called to perform with 0 or more than 2
   * entries in outgoing_particles.
   */
  class InvalidDecay : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
  };

 protected:
  /// determine the total energy in the center-of-mass frame
  double sqrt_s() const override;

  /**
   * \ingroup logging
   * Writes information about this decay action to the \p out stream.
   */
  void format_debug_output(std::ostream &out) const override;

  /** list of possible decays  */
  DecayBranchList decay_channels_;

  /** total decay width */
  float total_width_;

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
 * \ingroup action
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

  /** Add a new collision channel. */
  void add_collision(CollisionBranchPtr p);
  /** Add several new collision channels at once. */
  void add_collisions(CollisionBranchList pv);

  /**
   * Measure distance between incoming particles in center-of-momentum frame.
   * Returns the squared distance.
   *
   * \fpPrecision Why \c double?
   */
  double particle_distance() const;

  /**
   * Generate the final-state of the scattering process.
   * Performs either elastic or inelastic scattering.
   *
   * \throws InvalidResonanceFormation
   */
  void generate_final_state() override;

  float raw_weight_value() const override;

  /**
   * Determine the (parametrized) total cross section for this collision. This
   * is currently only used for calculating the string excitation cross section.
   */
  virtual float total_cross_section() const {
    return 0.;
  }

  /**
   * Determine the elastic cross section for this collision. This routine
   * by default just gives a constant cross section (corresponding to
   * elast_par) but can be overriden in child classes for a different behavior.
   *
   * \param[in] elast_par Elastic cross section parameter from the input file.
   *
   * \return A ProcessBranch object containing the cross section and
   * final-state IDs.
   */
  virtual CollisionBranchPtr elastic_cross_section(float elast_par);

  /**
   * Determine the cross section for string excitations, which is given by the
   * difference between the parametrized total cross section and all the
   * explicitly implemented channels at low energy (elastic, resonance
   * excitation, etc). This method has to be called after all other processes
   * have been added to the Action object.
   */
  virtual CollisionBranchPtr string_excitation_cross_section();

  /**
  * Find all resonances that can be produced in a 2->1 collision of the two
  * input particles and the production cross sections of these resonances.
  *
  * Given the data and type information of two colliding particles,
  * create a list of possible resonance production processes
  * and their cross sections.
  *
  * \return A list of processes with resonance in the final state.
  * Each element in the list contains the type of the final-state particle
  * and the cross section for that particular process.
  */
  virtual CollisionBranchList resonance_cross_sections();

  /**
   * Return the 2-to-1 resonance production cross section for a given resonance.
   *
   * \param[in] type_resonance Type information for the resonance to be produced.
   * \param[in] s Mandelstam-s of the collision
   * of the two initial particles.
   * \param[in] cm_momentum_squared Square of the center-of-mass momentum of the
   * two initial particles.
   *
   * \return The cross section for the process
   * [initial particle a] + [initial particle b] -> resonance.
   *
   * \fpPrecision Why \c double?
   */
  double two_to_one_formation(const ParticleType &type_resonance,
                              double s, double cm_momentum_sqr);

  /** Find all inelastic 2->2 processes for this reaction. */
  virtual CollisionBranchList two_to_two_cross_sections() {
    return CollisionBranchList();
  }

  /** Determine the total energy in the center-of-mass frame,
   * i.e. sqrt of Mandelstam s.  */
  double sqrt_s() const override;
  /**
   * \ingroup exception
   * Thrown when ScatterAction is called to perform with unknown ProcessType.
   */
  class InvalidScatterAction : public std::invalid_argument {
    using std::invalid_argument::invalid_argument;
  };

  float cross_section() const {
    return total_cross_section_;
  }

 protected:
  /** Determine the Mandelstam s variable,
   * s = (p_a + p_b)^2 = square of CMS energy.
   *
   * \fpPrecision Why \c double?
   */
  double mandelstam_s() const;
  /** Determine the momenta of the incoming particles in the
   * center-of-mass system.
   * \fpPrecision Why \c double?
   */
  double cm_momentum() const;
  /** Determine the squared momenta of the incoming particles in the
   * center-of-mass system.
   * \fpPrecision Why \c double?
   */
  double cm_momentum_squared() const;

  /**
   * \ingroup logging
   * Writes information about this scatter action to the \p out stream.
   */
  void format_debug_output(std::ostream &out) const override;

  /** List of possible collisions  */
  CollisionBranchList collision_channels_;

  /** Total cross section */
  float total_cross_section_;

 private:
  /// determine the velocity of the center-of-mass frame in the lab
  ThreeVector beta_cm() const;

  /** Check if the scattering is elastic. */
  bool is_elastic() const;

  /** Perform an elastic two-body scattering, i.e. just exchange momentum. */
  void momenta_exchange();

  /** Perform a 2->1 resonance-formation process. */
  void resonance_formation();
};


/**
 * \ingroup action
 * ScatterActionBaryonBaryon is a special ScatterAction which represents the
 * scattering of two baryons.
 */
class ScatterActionBaryonBaryon : public ScatterAction {
 public:
  /* Inherit constructor. */
  using ScatterAction::ScatterAction;
  /** Determine the parametrized total cross section
   * for a baryon-baryon collision. */
  virtual float total_cross_section() const override;
  /**
   * Determine the elastic cross section for a baryon-baryon collision.
   * It is given by a parametrization of exp. data for NN collisions and is
   * constant otherwise.
   *
   * \param[in] elast_par Elastic cross section parameter from the input file.
   *
   * \return A ProcessBranch object containing the cross section and
   * final-state IDs.
   */
  CollisionBranchPtr elastic_cross_section(float elast_par) override;
  /* There is no resonance formation out of two baryons: Return empty list. */
  CollisionBranchList resonance_cross_sections() override {
    return CollisionBranchList();
  }
  /** Find all inelastic 2->2 processes for this reaction. */
  CollisionBranchList two_to_two_cross_sections() override;

 private:
  /**
  * Calculate cross sections for single-resonance production from
  * nucleon-nucleon collisions (i.e. NN->NR).
  *
  * Checks are processed in the following order:
  * 1. Charge conservation
  * 3. Isospin factors (Clebsch-Gordan)
  * 4. Enough energy for all decay channels to be available for the resonance
  *
  * \param[in] type_particle1 Type information of the first incoming nucleon.
  * \param[in] type_particle2 Type information of the second incoming nucleon.
  *
  * \return List of resonance production processes possible in the collision
  * of the two nucleons. Each element in the list contains the type(s) of the
  * final state particle(s) and the cross section for that particular process.
  */
  CollisionBranchList nuc_nuc_to_nuc_res(const ParticleType &type_particle1,
                                       const ParticleType &type_particle2);

  /**
  * Calculate cross sections for resonance absorption on a nucleon
  * (i.e. NR->NN).
  *
  * \param[in] type_particle1 Type information of the first incoming nucleon.
  * \param[in] type_particle2 Type information of the second incoming nucleon.
  *
  * \return List of resonance absorption processes possible in the collision
  * with a nucleon. Each element in the list contains the type(s) of the
  * final state particle(s) and the cross section for that particular process.
  */
  CollisionBranchList nuc_res_to_nuc_nuc(const ParticleType &type_particle1,
                                       const ParticleType &type_particle2);

 protected:
  /**
   * \ingroup logging
   * Writes information about this scatter action to the \p out stream.
   */
  void format_debug_output(std::ostream &out) const override;
};

/**
 * \ingroup action
 * ScatterActionBaryonMeson is a special ScatterAction which represents the
 * scattering of a baryon and a meson.
 */
class ScatterActionBaryonMeson : public ScatterAction {
 public:
  /* Inherit constructor. */
  using ScatterAction::ScatterAction;

 protected:
  /**
   * \ingroup logging
   * Writes information about this scatter action to the \p out stream.
   */
  void format_debug_output(std::ostream &out) const override;
};

/**
 * \ingroup action
 * ScatterActionMesonMeson is a special ScatterAction which represents the
 * scattering of two mesons.
 */
class ScatterActionMesonMeson : public ScatterAction {
 public:
  /* Inherit constructor. */
  using ScatterAction::ScatterAction;

 protected:
  /**
   * \ingroup logging
   * Writes information about this scatter action to the \p out stream.
   */
  void format_debug_output(std::ostream &out) const override;
};


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

/**
 * \ingroup logging
 * Convenience: dereferences the ActionPtr to Action.
 */
inline std::ostream &operator<<(std::ostream &out, const ActionPtr &action) {
  return out << *action;
}

/**
 * \ingroup logging
 * Writes multiple actions to the \p out stream.
 */
std::ostream &operator<<(std::ostream &out, const ActionList &actions);

}  // namespace Smash

#endif  // SRC_INCLUDE_ACTION_H_

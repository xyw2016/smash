/*
 *
 *    Copyright (c) 2014-2018
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/binaryoutput.h"

#include <string>

#include <boost/filesystem.hpp>

#include "smash/action.h"
#include "smash/clock.h"
#include "smash/config.h"
#include "smash/particles.h"

namespace smash {

/*!\Userguide
 * \page format_binary_ Binary Format
 * SMASH supports a binary output version similar to the OSCAR 2013 standard.
 * It is faster to read and write and theoretically needs less disk space.
 * However, currently in ASCII OSCAR 2013 only 5 digits after the comma are
 * written for any real number, while the binary saves the whole double
 * (16 digits). By accident, this makes the sizes of the binary output files
 * approximately the same as the OSCAR ASCII files.
 * **The binary format follows the general block structure of the OSCAR
 * format:**
 * \ref oscar_general_. However, for the binary format, the data type
 * specification is stricter. The types used for the output are 4 bytes signed
 * integers, 8 bytes doubles and 1 byte chars.
 *
 * As for OSCAR ASCII output there are two kinds of binary output:
 * particles and collisions.
 * The specifics for both particles and collisions output are the following:\n
 * **Header**
 * \code
 * 4*char        uint16_t        uint16_t        uint32_t  len*char
 * magic_number, format_version, format_variant, len,      smash_version
 * \endcode
 * \li magic_number - 4 bytes that in ASCII read as "SMSH".
 * \li Format version is an integer number, currently it is 4.
 * \li Format variant is an integer number: 0 for default, 1 for extended.
 * \li len is the length of smash version string
 * \li smash_version is len chars that give information about the SMASH version.
 *
 * **Output block header**\n
 * At start of event, end of event or any other particle output:
 * \code
 * char uint32_t
 * 'p'  n_part_lines
 * \endcode
 * \li \c n_part_lines is the number of particle lines in the block that follows
 *
 * At interaction:
 * \code
 * char uint32_t uint32_t double  double   uint32_t
 * 'i'  nin      nout     density xsection process_type
 * \endcode
 * \li \c nin, \c nout are numbers of incoming and outgoing particles
 *
 * Block header is followed by \c nin + \c nout particle lines.
 *
 * **Particle line**
 * \code
 *     9*double             int int int
 * t x y z mass p0 px py pz pdg ID charge
 * \endcode
 *
 * **Extended Particle line**
 * <div class="fragment">
 * <div class="line">
 *   9*double        int int int int     double
 *     double                    uint32_t            uint32_t
 *     double        int         int
 * </div>
 * <div class="line">
 * t x y z mass p0 px py pz pdg ID charge ncoll form_time xsecfac
 * proc_id_origin proc_type_origin time_last_coll pdg_mother1 pdg_mother2
 * </div></div>
 *
 * **Event end line**
 * \code
 * char    int            int
 * 'f' event_number impact_parameter
 * \endcode
 *
 * Particles output
 * ----------------
 * The particles output is Written to the \c particles_binary.bin file.
 * It contains the current particle list at specific moments of time. Every
 * moment of time is written as a 'p' block. For options of this output see
 * \ref output_content_specific_options_ "content-specific output options".
 *
 * Collisions output
 * -----------------
 * The collisions output is Written to the \c collisions_binary.bin file.
 * It contains interactions (collisions, decays, box wall crossings) and
 * optionally the initial and final configuration. The interactions are written
 * in computational frame time-ordered fashion, in 'i' blocks, which contains
 * the information of the incoming and the outgoing particles of each reaction
 * written in the 'incoming' and 'outgoing' blocks respectively.
 * Initial and final states are written as 'p' blocks. The process IDs
 * indicating the types of the reaction, such as resonance decay,
 * elastic scattering, soft string process, hard string process, etc.,
 * are written in the 'process_type' blocks. For options of this output see
 * \ref output_content_specific_options_ "content-specific output options".
 *
 * See also \ref collisions_output_in_box_modus_.
 **/

BinaryOutputBase::BinaryOutputBase(const bf::path &path,
                                   const std::string &mode,
                                   const std::string &name,
                                   bool extended_format)
    : OutputInterface(name), file_{path, mode}, extended_(extended_format) {
  std::fwrite("SMSH", 4, 1, file_.get());  // magic number
  write(format_version_);                  // file format version number
  std::uint16_t format_variant = static_cast<uint16_t>(extended_);
  write(format_variant);
  write(VERSION_MAJOR);  // SMASH version
}

// write functions:
void BinaryOutputBase::write(const std::string &s) {
  const auto size = boost::numeric_cast<uint32_t>(s.size());
  std::fwrite(&size, sizeof(std::uint32_t), 1, file_.get());
  std::fwrite(s.c_str(), s.size(), 1, file_.get());
}

void BinaryOutputBase::write(const double x) {
  std::fwrite(&x, sizeof(x), 1, file_.get());
}

void BinaryOutputBase::write(const FourVector &v) {
  std::fwrite(v.begin(), sizeof(*v.begin()), 4, file_.get());
}

void BinaryOutputBase::write(const Particles &particles) {
  for (const auto &p : particles) {
    write_particledata(p);
  }
}

void BinaryOutputBase::write(const ParticleList &particles) {
  for (const auto &p : particles) {
    write_particledata(p);
  }
}

void BinaryOutputBase::write_particledata(const ParticleData &p) {
  write(p.position());
  double mass = p.effective_mass();
  std::fwrite(&mass, sizeof(mass), 1, file_.get());
  write(p.momentum());
  write(p.pdgcode().get_decimal());
  write(p.id());
  write(p.type().charge());
  if (extended_) {
    const auto history = p.get_history();
    write(history.collisions_per_particle);
    write(p.formation_time());
    write(p.xsec_scaling_factor());
    write(history.id_process);
    write(static_cast<uint32_t>(history.process_type));
    write(history.time_last_collision);
    write(history.p1.get_decimal());
    write(history.p2.get_decimal());
  }
}

BinaryOutputCollisions::BinaryOutputCollisions(const bf::path &path,
                                               std::string name,
                                               const OutputParameters &out_par)
    : BinaryOutputBase(
          path / ((name == "Collisions" ? "collisions_binary" : name) + ".bin"),
          "wb", name, out_par.get_coll_extended(name)),
      print_start_end_(out_par.coll_printstartend) {}

void BinaryOutputCollisions::at_eventstart(const Particles &particles,
                                           const int) {
  char pchar = 'p';
  if (print_start_end_) {
    std::fwrite(&pchar, sizeof(char), 1, file_.get());
    write(particles.size());
    write(particles);
  }
}

void BinaryOutputCollisions::at_eventend(const Particles &particles,
                                         const int event_number,
                                         double impact_parameter) {
  char pchar = 'p';
  if (print_start_end_) {
    std::fwrite(&pchar, sizeof(char), 1, file_.get());
    write(particles.size());
    write(particles);
  }

  // Event end line
  char fchar = 'f';
  std::fwrite(&fchar, sizeof(char), 1, file_.get());
  write(event_number);
  write(impact_parameter);

  // Flush to disk
  std::fflush(file_.get());
}

void BinaryOutputCollisions::at_interaction(const Action &action,
                                            const double density) {
  char ichar = 'i';
  std::fwrite(&ichar, sizeof(char), 1, file_.get());
  write(action.incoming_particles().size());
  write(action.outgoing_particles().size());
  std::fwrite(&density, sizeof(double), 1, file_.get());
  const double weight = action.get_total_weight();
  std::fwrite(&weight, sizeof(double), 1, file_.get());
  const double partial_weight = action.get_partial_weight();
  std::fwrite(&partial_weight, sizeof(double), 1, file_.get());
  const auto type = static_cast<uint32_t>(action.get_type());
  std::fwrite(&type, sizeof(uint32_t), 1, file_.get());
  write(action.incoming_particles());
  write(action.outgoing_particles());
}

BinaryOutputParticles::BinaryOutputParticles(const bf::path &path,
                                             std::string name,
                                             const OutputParameters &out_par)
    : BinaryOutputBase(path / "particles_binary.bin", "wb", name,
                       out_par.part_extended),
      only_final_(out_par.part_only_final) {}

void BinaryOutputParticles::at_eventstart(const Particles &particles,
                                          const int) {
  char pchar = 'p';
  if (!only_final_) {
    std::fwrite(&pchar, sizeof(char), 1, file_.get());
    write(particles.size());
    write(particles);
  }
}

void BinaryOutputParticles::at_eventend(const Particles &particles,
                                        const int event_number,
                                        double impact_parameter) {
  char pchar = 'p';
  std::fwrite(&pchar, sizeof(char), 1, file_.get());
  write(particles.size());
  write(particles);

  // Event end line
  char fchar = 'f';
  std::fwrite(&fchar, sizeof(char), 1, file_.get());
  write(event_number);
  write(impact_parameter);

  // Flush to disk
  std::fflush(file_.get());
}

void BinaryOutputParticles::at_intermediate_time(const Particles &particles,
                                                 const Clock &,
                                                 const DensityParameters &) {
  char pchar = 'p';
  if (!only_final_) {
    std::fwrite(&pchar, sizeof(char), 1, file_.get());
    write(particles.size());
    write(particles);
  }
}

}  // namespace smash

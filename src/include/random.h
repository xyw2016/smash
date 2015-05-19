/*
 *
 *    Copyright (c) 2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_RANDOM_H_
#define SRC_INCLUDE_RANDOM_H_

#include <random>

namespace Smash {

/** Namespace Random provides functions for Random Number Generation.
 */

namespace Random {

/// The random number engine used is std::randlux48
using Engine = std::ranlux48;

/// The engine that is used commonly by all distributions.
extern /*thread_local*/ Engine engine;

/** Provides uniform random numbers on a fixed interval.
 *
 * objects of uniform_dist can be used to provide a large number of
 * random numbers in the same interval. Example:
 *
 * \code
 *   using namespace Random;
 *   double sum = 0.0;
 *   auto uniform_0_to_3 = uniform_dist(0., 3.);
 *   for (MANY_TIMES) {
 *     sum += uniform_0_to_3();
 *   }
 * \endcode
 *
 * The random number engine is completely hidden inside the object.
 */
template <typename T> class uniform_dist {
 public:
  /** creates the object and fixes the interval */
  uniform_dist(T min, T max)
    : distribution(min, max) {
  }
  /** returns a random number in the interval */
  T operator()() {
    return distribution(engine);
  }
  /** the distribution object that is being used. */
 private:
  std::uniform_real_distribution<T> distribution;
};

/** sets the seed of the random number engine. */
template <typename T> void set_seed(T &&seed) {
  engine.seed(std::forward<T>(seed));
}
/** returns a uniformly distributed random number \f$\chi \in [{\rm
 * min}, {\rm max})\f$ */
template <typename T> T uniform(T min, T max) {
  return std::uniform_real_distribution<T>(min, max)(engine);
}
/** returns a uniformly distributed random number \f$\chi \in [0,1)\f$
 */
template <typename T = double> T canonical() {
  return std::generate_canonical<T, std::numeric_limits<double>::digits>(
      engine);
}
/** returns a uniform_dist object */
template <typename T>
uniform_dist<T> make_uniform_distribution(T min, T max) {
  return uniform_dist<T>(min, max);
}
/** returns an exponentially distributed random number
 *
 * Probability for a given return value \f$\chi\f$ is \f$p(\chi) =
 * \Theta(\chi) \cdot \exp(-t)\f$
 */
template <typename T = double> T exponential() {
  return std::exponential_distribution<T>(1)(engine);
}

/** Evaluates a random number x according to an exponential distribution exp(A*x).
 *  x is restricted to lie between x1 and x2 (it doesn't matter which one of
 * the two is larger). */
template <typename T = double> T expo(T A, T x1, T x2) {
  T a1 = A*x1, a2 = A*x2;
  T a_min = std::log(std::numeric_limits<T>::min());
  T r1 = a1 > a_min ? std::exp(a1) : 0.;  // prevent underflow
  T r2 = a2 > a_min ? std::exp(a2) : 0.;  // prevent underflow
  T x;
  do {
    x = std::log(uniform(r1,r2))/A;
  } while ((x<x1) + (x<x2) != 1);  // make sure that x is between x1 and x2
  return x;
}

}  // namespace Random
}  // namespace Smash

#endif  // SRC_INCLUDE_RANDOM_H_

/**
 * @file RandomGenerator.h
 * @author seckler
 * @date 22.05.18
 */

#pragma once

#include <array>
#include "autopas/utils/inBox.h"

/**
 * Generator class for uniform distributions
 */
class RandomGenerator {
 private:
  /**
   * Simple random function
   * @tparam floatType
   * @param fMin
   * @param fMax
   * @return double between fMin and fMax
   */
  template <typename floatType>
  static floatType fRand(floatType fMin, floatType fMax);

 public:
  /**
   * Generate a random position within a given box.
   * @tparam floatType
   * @param boxMin
   * @param boxMax
   * @return 3D array with random values
   */
  template <typename floatType>
  static std::array<floatType, 3> randomPosition(const std::array<floatType, 3>& boxMin,
                                                 const std::array<floatType, 3>& boxMax);

  /**
   * Fills the given container with randomly distributed particles between boxMin and boxMax.
   * @tparam Container
   * @tparam Particle Type of particle to be generated
   * @param container
   * @param defaultParticle inserted particle
   * @param boxMin min. position
   * @param boxMax max. position
   * @param numParticles number of particles
   */
  template <class Container, class Particle>
  static void fillWithParticles(Container& container, const Particle& defaultParticle,
                                const std::array<typename Particle::ParticleFloatingPointType, 3>& boxMin,
                                const std::array<typename Particle::ParticleFloatingPointType, 3>& boxMax,
                                unsigned long numParticles = 100ul);

  /**
   * Fills only a given part of a container (also AutoPas object) with randomly uniformly distributed particles.
   * @tparam Container Arbitrary container class that needs to support getBoxMax() and addParticle().
   * @tparam Particle Type of the default particle.
   * @param container
   * @param defaultParticle
   * @param haloWidth
   * @param numParticles
   */
  template <class Container, class Particle>
  static void fillWithHaloParticles(Container& container, const Particle& defaultParticle,
                                    typename Particle::ParticleFloatingPointType haloWidth,
                                    unsigned long numParticles = 100ul);

  /**
   * Fills the given container with randomly distributed particles
   * @tparam Container Arbitrary container class that needs to support getBoxMax() and addParticle().
   * @tparam Particle Type of the default particle.
   * @param container
   * @param defaultParticle
   * @param numParticles
   */
  template <class Container, class Particle>
  static void fillWithParticles(Container& container, const Particle& defaultParticle,
                                unsigned long numParticles = 100) {
    RandomGenerator::fillWithParticles(container, defaultParticle, container.getBoxMin(), container.getBoxMax(),
                                       numParticles);
  }
};

template <class Container, class Particle>
void RandomGenerator::fillWithParticles(Container& container, const Particle& defaultParticle,
                                        const std::array<typename Particle::ParticleFloatingPointType, 3>& boxMin,
                                        const std::array<typename Particle::ParticleFloatingPointType, 3>& boxMax,
                                        unsigned long numParticles) {
  srand(42);  // fixed seedpoint

  for (unsigned long i = 0; i < numParticles; ++i) {
    Particle particle(defaultParticle);
    particle.setR(randomPosition(boxMin, boxMax));
    particle.setID(i);
    container.addParticle(particle);
  }
}

template <class Container, class Particle>
void RandomGenerator::fillWithHaloParticles(Container& container, const Particle& defaultParticle,
                                            typename Particle::ParticleFloatingPointType haloWidth,
                                            unsigned long numParticles) {
  srand(42);  // fixed seedpoint

  auto haloBoxMin = container.getBoxMin();
  auto haloBoxMax = container.getBoxMax();

  // increase the box size not exactly by the width to make it exclusive
  for (unsigned int i = 0; i < 3; ++i) {
    haloBoxMin[i] -= haloWidth * .99;
    haloBoxMax[i] += haloWidth * .99;
  }

  for (unsigned long i = 0; i < numParticles; ++i) {
    const auto pos = randomPosition<typename Particle::ParticleFloatingPointType>(haloBoxMax, haloBoxMax);
    // we only want  to add particles not in the actual box
    if (autopas::utils::inBox(pos, container.getBoxMin(), container.getBoxMax())) continue;
    Particle particle(defaultParticle);
    particle.setR(pos);
    particle.setID(i);
    container.addHaloParticle(particle);
  }
}

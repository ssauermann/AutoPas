/**
 * @file BoundaryConditions.h
 * @author N. Fottner
 * @date 2/8/19
 */

#pragma once
#include <array>
#include <vector>

#include "autopas/AutoPas.h"

/**
 * This Class implements the Periodic Boudaries for an AutoPas Object
 * The implementation was taken from "AutoPasInterfaceTest.cpp"
 */
template <typename ParticleCell>
class BoundaryConditions {
 public:
  /**
   * Type of the Particle
   */
  using ParticleType = typename ParticleCell::ParticleType;

  /**
   * Default constructor.
   */
  BoundaryConditions() = default;

  virtual ~BoundaryConditions() = default;
  /**
   * Convert the leaving particle to entering particles.
   * Hereby the periodic boundary position change is done.
   * @param leavingParticles
   * @return vector of particles that will enter the container.
   */
  static std::vector<ParticleType> convertToEnteringParticles(autopas::AutoPas<ParticleType, ParticleCell> &autopas,
                                                              const std::vector<ParticleType> &leavingParticles);

  /**
   * Identifies and sends particles that are in the halo of neighboring AutoPas instances or the same instance (periodic
   * boundaries).
   * @param autoPas
   * @return vector of particles that are already shifted for the next process.
   */
  static std::vector<ParticleType> identifyAndSendHaloParticles(autopas::AutoPas<ParticleType, ParticleCell> &autoPas);

  /**
   * Adds entering Particles to the Autopas Object
   * @param autoPas
   * @param enteringParticles
   * @return number of Particles Added
   */
  static size_t addEnteringParticles(autopas::AutoPas<ParticleType, ParticleCell> &autoPas,
                                     std::vector<ParticleType> &enteringParticles);

  /**
   * Adds Halo Particles to the AutoPas Object
   * @param autoPas
   * @param haloParticles
   */
  static void addHaloParticles(autopas::AutoPas<ParticleType, ParticleCell> &autoPas,
                               std::vector<ParticleType> &haloParticles);

  /**
   * Realizes periodic Boundaries for the simulation
   * By handling halo particles and updating the container
   */
  static void applyPeriodic(autopas::AutoPas<ParticleType, ParticleCell> &autoPas);
};
template <typename ParticleCell>
std::vector<typename ParticleCell::ParticleType> BoundaryConditions<ParticleCell>::convertToEnteringParticles(
    autopas::AutoPas<ParticleType, ParticleCell> &autopas, const std::vector<ParticleType> &leavingParticles) {
  std::vector<ParticleType> enteringParticles{leavingParticles};
  for (auto &p : enteringParticles) {
    auto pos = p.getR();
    for (auto dim = 0; dim < 3; dim++) {
      if (pos[dim] < autopas.getBoxMin()[dim]) {
        // has to be smaller than boxMax
        pos[dim] = std::min(std::nextafter(autopas.getBoxMax()[dim], -1),
                            pos[dim] + (autopas.getBoxMax()[dim] - autopas.getBoxMin()[dim]));
      } else if (pos[dim] >= autopas.getBoxMax()[dim]) {
        // should at least be boxMin
        pos[dim] = std::max(autopas.getBoxMin()[dim], pos[dim] - (autopas.getBoxMax()[dim] - autopas.getBoxMin()[dim]));
      }
    }
    p.setR(pos);
  }
  return enteringParticles;
}
template <typename ParticleCell>
std::vector<typename ParticleCell::ParticleType> BoundaryConditions<ParticleCell>::identifyAndSendHaloParticles(
    autopas::AutoPas<ParticleType, ParticleCell> &autoPas) {
  std::vector<ParticleType> haloParticles;

  for (short x : {-1, 0, 1}) {
    for (short y : {-1, 0, 1}) {
      for (short z : {-1, 0, 1}) {
        if (x == 0 and y == 0 and z == 0) continue;
        std::array<short, 3> direction{x, y, z};
        std::array<double, 3> min{}, max{}, shiftVec{};
        for (size_t dim = 0; dim < 3; ++dim) {
          // The search domain has to be enlarged as the position of the particles is not certain.
          bool needsShift = false;
          if (direction[dim] == -1) {
            min[dim] = autoPas.getBoxMin()[dim] - autoPas.getVerletSkin();
            max[dim] = autoPas.getBoxMin()[dim] + autoPas.getCutoff() + autoPas.getVerletSkin();
            needsShift = true;
          } else if (direction[dim] == 1) {
            min[dim] = autoPas.getBoxMax()[dim] - autoPas.getCutoff() - autoPas.getVerletSkin();
            max[dim] = autoPas.getBoxMax()[dim] + autoPas.getVerletSkin();
            needsShift = true;
          } else {  // 0
            min[dim] = autoPas.getBoxMin()[dim] - autoPas.getVerletSkin();
            max[dim] = autoPas.getBoxMax()[dim] + autoPas.getVerletSkin();
          }
          if (needsShift) {
            shiftVec[dim] = -(autoPas.getBoxMax()[dim] - autoPas.getBoxMin()[dim]) * direction[dim];
          } else {
            shiftVec[dim] = 0;
          }
        }
        // here it is important to only iterate over the owned particles!
        for (auto iter = autoPas.getRegionIterator(min, max, autopas::IteratorBehavior::ownedOnly); iter.isValid();
             ++iter) {
          auto particleCopy = *iter;
          particleCopy.addR(shiftVec);
          haloParticles.push_back(particleCopy);
        }
      }
    }
  }
  return haloParticles;
}

template <typename ParticleCell>
size_t BoundaryConditions<ParticleCell>::addEnteringParticles(autopas::AutoPas<ParticleType, ParticleCell> &autoPas,
                                                              std::vector<ParticleType> &enteringParticles) {
  size_t numAdded = 0;
  for (auto &p : enteringParticles) {
    if (autopas::utils::inBox(p.getR(), autoPas.getBoxMin(), autoPas.getBoxMax())) {
      autoPas.addParticle(p);
      ++numAdded;
    }
  }
  return numAdded;
}
template <typename ParticleCell>
void BoundaryConditions<ParticleCell>::addHaloParticles(autopas::AutoPas<ParticleType, ParticleCell> &autoPas,
                                                        std::vector<ParticleType> &haloParticles) {
  for (auto &p : haloParticles) {
    autoPas.addOrUpdateHaloParticle(p);
  }
}

template <typename ParticleCell>
void BoundaryConditions<ParticleCell>::applyPeriodic(autopas::AutoPas<ParticleType, ParticleCell> &autoPas) {
  // 1. update Container; return value is vector of invalid == leaving particles!
  auto [invalidParticles, updated] = autoPas.updateContainer();
  if (updated) {
    // 2. leaving and entering particles
    const auto &sendLeavingParticles = invalidParticles;
    // 2b. get+add entering particles (addParticle)
    std::vector<ParticleType> enteringParticles = convertToEnteringParticles(autoPas, sendLeavingParticles);
    addEnteringParticles(autoPas, enteringParticles);
  }
  // 3. halo particles
  // 3a. identify and send inner particles that are in the halo of other autopas instances or itself.
  auto sendHaloParticles = identifyAndSendHaloParticles(autoPas);
  // 3b. get halo particles
  auto &recvHaloParticles = sendHaloParticles;
  addHaloParticles(autoPas, recvHaloParticles);
  // after this, pairwise force calculation
}

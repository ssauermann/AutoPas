/**
 * @file LogicHandler.h
 * @author seckler
 * @date 31.05.19
 */

#pragma once
#include "autopas/selectors/AutoTuner.h"
#include "autopas/utils/Logger.h"

namespace autopas {

/**
 * The LogicHandler takes care of the containers s.t. they are all in the same valid state.
 * This is mainly done by incorporating a global container rebuild frequency, which defines when containers and their
 * neighbor lists will be rebuild.
 */
template <typename Particle, typename ParticleCell>
class LogicHandler {
 public:
  /**
   * Constructor of the LogicHandler.
   * @param autoTuner
   * @param rebuildFrequency
   * @param tuningInterval
   * @param numSamples
   */
  LogicHandler(autopas::AutoTuner<Particle, ParticleCell> &autoTuner, unsigned int rebuildFrequency,
               unsigned int tuningInterval, unsigned int numSamples)
      : _containerRebuildFrequency{rebuildFrequency},
        _tuningInterval{tuningInterval},
        _numSamples{numSamples},
        _autoTuner(autoTuner) {
    doAssertions();
  }

  /**
   * @copydoc AutoPas::updateContainer()
   */
  std::vector<Particle> AUTOPAS_WARN_UNUSED_RESULT updateContainer() {
    if (not isContainerValid()) {
      AutoPasLog(debug, "Initiating container update.");
      _containerIsValid = false;
      return std::move(_autoTuner.getContainer()->updateContainer());
    } else {
      AutoPasLog(debug, "Skipping container update.");
      return std::vector<Particle>{};
    }
  }

  /**
   * @copydoc AutoPas::addParticle()
   */
  void addParticle(Particle &p) {
    if (not isContainerValid()) {
      _autoTuner.getContainer()->addParticle(p);
    } else {
      autopas::utils::ExceptionHandler::exception(
          "Adding of particles not allowed while neighborlists are still valid. Please invalidate the neighborlists "
          "by calling AutoPas::invalidateLists(). Do this on EVERY AutoPas instance, i.e., on all mpi processes!");
    }
  }

  /**
   * @copydoc AutoPas::addOrUpdateHaloParticle()
   */
  void addOrUpdateHaloParticle(Particle &haloParticle) {
    auto container = _autoTuner.getContainer();
    if (not isContainerValid()) {
      container->addHaloParticle(haloParticle);
    } else {
      if (not utils::inBox(haloParticle.getR(), ArrayMath::addScalar(container->getBoxMin(), container->getSkin() / 2),
                           ArrayMath::subScalar(container->getBoxMax(), container->getSkin() / 2))) {
        bool updated = _autoTuner.getContainer()->updateHaloParticle(haloParticle);
        if (not updated) {
          // a particle has to be updated if it is within cutoff + skin/2 of the bounding box
          double dangerousDistance = container->getCutoff() + container->getSkin() / 2;

          bool dangerous =
              utils::inBox(haloParticle.getR(), ArrayMath::subScalar(container->getBoxMin(), dangerousDistance),
                           ArrayMath::addScalar(container->getBoxMax(), dangerousDistance));
          if (dangerous) {
            // throw exception, rebuild frequency not high enough / skin too small!
            utils::ExceptionHandler::exception(
                "VerletListsLinkedBase::addHaloParticle: wasn't able to update halo particle that is too close to "
                "domain (more than cutoff + skin/2). Rebuild frequency not high enough / skin too small!");
          }
        }
      } else {
        // throw exception, rebuild frequency not high enough / skin too small!
        utils::ExceptionHandler::exception(
            "VerletListsLinkedBase::addHaloParticle: trying to update halo particle that is too far inside domain "
            "(more than skin/2). Rebuild frequency not high enough / skin too small!");
      }
    }
  }

  /**
   * @copydoc AutoPas::deleteHaloParticles()
   */
  void deleteHaloParticles() {
    _containerIsValid = false;
    _autoTuner.getContainer()->deleteHaloParticles();
  }

  /**
   * @copydoc AutoPas::deleteAllParticles()
   */
  void deleteAllParticles() {
    _containerIsValid = false;
    _autoTuner.getContainer()->deleteAllParticles();
  }

  /**
   * @copydoc AutoPas::iteratePairwise()
   */
  template <class Functor>
  void iteratePairwise(Functor *f) {
    const bool doRebuild = not isContainerValid();
    _autoTuner.iteratePairwise(f, doRebuild);
    if (doRebuild /*we have done a rebuild now*/) {
      // list is now valid
      _containerIsValid = true;
      _stepsSinceLastContainerRebuild = 0;
    }
    ++_stepsSinceLastContainerRebuild;
  }

  /**
   * @copydoc AutoPas::begin()
   */
  autopas::ParticleIteratorWrapper<Particle> begin(IteratorBehavior behavior = IteratorBehavior::haloAndOwned) {
    return _autoTuner.getContainer()->begin(behavior);
  }

  /**
   * @copydoc AutoPas::getRegionIterator()
   */
  autopas::ParticleIteratorWrapper<Particle> getRegionIterator(
      std::array<double, 3> lowerCorner, std::array<double, 3> higherCorner,
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) {
    return _autoTuner.getContainer()->getRegionIterator(lowerCorner, higherCorner, behavior);
  }

 private:
  void doAssertions() {
    auto container = _autoTuner.getContainer();
    // check boxSize at least cutoff + skin
    for (unsigned int dim = 0; dim < 3; ++dim) {
      if (container->getBoxMax()[dim] - container->getBoxMin()[dim] < container->getCutoff() + container->getSkin()) {
        AutoPasLog(error, "Box (boxMin[{}]={} and boxMax[{}]={}) is too small.", dim, container->getBoxMin()[dim], dim,
                   container->getBoxMax()[dim]);
        AutoPasLog(error, "Has to be at least cutoff({}) + skin({}) = {}.", container->getCutoff(),
                   container->getSkin(), container->getCutoff() + container->getSkin());
        autopas::utils::ExceptionHandler::exception("Box too small.");
      }
    }
  }

  bool isContainerValid() {
    return _containerIsValid and _stepsSinceLastContainerRebuild < _containerRebuildFrequency and
           not _autoTuner.willRebuild();
  }

  /**
   * Specifies after how many pair-wise traversals the container and their neighbor lists (if they exist) are to be
   * rebuild.
   */
  unsigned int _containerRebuildFrequency;

  /**
   * Number of timesteps after which the auto-tuner shall reevaluate all selections.
   */
  unsigned int _tuningInterval;

  /**
   * Number of samples the tuner should collect for each combination.
   */
  unsigned int _numSamples;

  /**
   * Reference to the AutoTuner that owns the container, ...
   */
  autopas::AutoTuner<Particle, ParticleCell> &_autoTuner;

  /**
   * Specifies if the neighbor list is valid.
   */
  bool _containerIsValid{false};

  /**
   * Steps since last rebuild
   */
  unsigned int _stepsSinceLastContainerRebuild{UINT32_MAX};
};
}  // namespace autopas

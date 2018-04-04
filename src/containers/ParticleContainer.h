/*
 * ParticleContainer.h
 *
 *  Created on: 17 Jan 2018
 *      Author: tchipevn
 */

#ifndef DEPENDENCIES_EXTERNAL_AUTOPAS_SRC_PARTICLECONTAINER_H_
#define DEPENDENCIES_EXTERNAL_AUTOPAS_SRC_PARTICLECONTAINER_H_

#include <array>
#include "iterators/ParticleIterator.h"
#include "iterators/RegionParticleIterator.h"
#include "pairwiseFunctors/Functor.h"

namespace autopas {

// consider multiple inheritance or delegation vor avoidane of virtual call to
// Functor

template <class Particle, class ParticleCell>
class ParticleContainer {
 public:
  ParticleContainer(const std::array<double, 3> boxMin,
                    const std::array<double, 3> boxMax, double cutoff)
      : _data(), _boxMin(boxMin), _boxMax(boxMax), _cutoff(cutoff) {}
  virtual ~ParticleContainer() = default;

  /**
   * delete the copy constructor to prevent unwanted copies.
   * No particle container should ever be copied.
   * @param obj
   */
  ParticleContainer(const ParticleContainer &obj) = delete;

  /**
   * delete the copy assignment operator to prevent unwanted copies
   * No particle container should ever be copied.
   * @param other
   * @return
   */
  ParticleContainer &operator=(const ParticleContainer &other) = delete;

  virtual void init() {}

  virtual void addParticle(Particle &p) = 0;

  virtual void addHaloParticle(Particle &haloParticle) = 0;

  virtual void deleteHaloParticles() = 0;

  virtual void iteratePairwiseAoS(Functor<Particle, ParticleCell> *f) = 0;

  virtual void iteratePairwiseSoA(Functor<Particle, ParticleCell> *f) = 0;

  typedef ParticleIterator<Particle, ParticleCell> iterator;
  iterator begin() { return iterator(&_data); }

  typedef RegionParticleIterator<Particle, ParticleCell> regionIterator;
  regionIterator getRegionIterator(std::array<double, 3> lowerCorner,
                                   std::array<double, 3> higherCorner) {
    return regionIterator(&_data, lowerCorner, higherCorner);
  }

  const std::array<double, 3> &getBoxMax() const { return _boxMax; }

  void setBoxMax(const std::array<double, 3> &boxMax) { _boxMax = boxMax; }

  const std::array<double, 3> &getBoxMin() const { return _boxMin; }

  void setBoxMin(const std::array<double, 3> &boxMin) { _boxMin = boxMin; }

  double getCutoff() const { return _cutoff; }

  void setCutoff(double cutoff) { _cutoff = cutoff; }

  virtual void updateContainer() = 0;

 protected:
  std::vector<ParticleCell> _data;

 private:
  std::array<double, 3> _boxMin;
  std::array<double, 3> _boxMax;
  double _cutoff;
};

} /* namespace autopas */

#endif /* DEPENDENCIES_EXTERNAL_AUTOPAS_SRC_PARTICLECONTAINER_H_ */

/**
 * @file RMMParticleCell3T.h
 * @date 17.01.2018
 * @author tchipevn
 */

#pragma once

#include <array>

#include "autopas/cells/ParticleCell.h"
#include "autopas/iterators/ParticleIteratorInterface.h"
#include "autopas/utils/SoA.h"

namespace autopas {

/**
 * Reduced Memory Mode ParticleCell.
 * This cell type does not store particles explicitly. Instead, the particles
 * are stored directly in a structure of array.
 * @todo This currently does not store all information of the particles. Only
 * position and forces and thus does not work.
 * @tparam Particle Type of particle to be stored.
 * @tparam iterator_t Iterator type when iterating over normal particles.
 * @tparam const_iterator_t Iterator type when iterating over constant particles.
 */
template <class Particle, class iterator_t, class const_iterator_t>
class RMMParticleCell3T : public ParticleCell<Particle> {
 public:
  /**
   * Constructor of RMMParticleCell
   */
  RMMParticleCell3T() = default;

  /**
   * @copydoc ParticleCell::addParticle(const Particle&)
   */
  void addParticle(const Particle &p) override {
    _particleSoABuffer.template push<Particle::AttributeNames::id>(p.getID());
    _particleSoABuffer.template push<Particle::AttributeNames::posX>(p.getR()[0]);
    _particleSoABuffer.template push<Particle::AttributeNames::posY>(p.getR()[1]);
    _particleSoABuffer.template push<Particle::AttributeNames::posZ>(p.getR()[2]);
    _particleSoABuffer.template push<Particle::AttributeNames::forceX>(p.getF()[0]);
    _particleSoABuffer.template push<Particle::AttributeNames::forceY>(p.getF()[1]);
    _particleSoABuffer.template push<Particle::AttributeNames::forceZ>(p.getF()[2]);
    _particleSoABuffer.template push<Particle::AttributeNames::owned>(p.isOwned());
  }

  SingleCellIteratorWrapper<Particle, true> begin() override {
    return SingleCellIteratorWrapper<Particle, true>(new iterator_t(this));
  }

  SingleCellIteratorWrapper<Particle, false> begin() const override {
    return SingleCellIteratorWrapper<Particle, false>(new const_iterator_t(this));
  }

  unsigned long numParticles() const override { return _particleSoABuffer.getNumParticles(); }
  bool isNotEmpty() const override { return numParticles() > 0; }

  void clear() override { _particleSoABuffer.clear(); }

  void deleteByIndex(size_t index) override {
    if (index >= numParticles()) {
      utils::ExceptionHandler::exception("Index out of range (range: [0, {}[, index: {})", numParticles(), index);
    }
    if (index < numParticles() - 1) {
      _particleSoABuffer.swap(index, numParticles() - 1);
    }
    _particleSoABuffer.pop_back();
  }

  void setCellLength(std::array<double, 3> &cellLength) override {}

  std::array<double, 3> getCellLength() const override { return std::array<double, 3>{0., 0., 0.}; }

  /**
   * The soa buffer of the particle, all information is stored here.
   */
  SoA<typename Particle::SoAArraysType> _particleSoABuffer;

 private:
  void buildParticleFromSoA(size_t i, Particle *&rmm_or_not_pointer) const {
    rmm_or_not_pointer->setR(
        _particleSoABuffer.template readMultiple<Particle::AttributeNames::posX, Particle::AttributeNames::posY,
                                                 Particle::AttributeNames::posZ>(i));
    rmm_or_not_pointer->setF(
        _particleSoABuffer.template readMultiple<Particle::AttributeNames::forceX, Particle::AttributeNames::forceY,
                                                 Particle::AttributeNames::forceZ>(i));
    rmm_or_not_pointer->setOwned(_particleSoABuffer.template read<Particle::AttributeNames::owned>(i));
  }

  void writeParticleToSoA(size_t index, Particle &particle) {
    _particleSoABuffer.template writeMultiple<Particle::AttributeNames::posX, Particle::AttributeNames::posY,
                                              Particle::AttributeNames::posZ>(index, particle.getR());
    _particleSoABuffer.template writeMultiple<Particle::AttributeNames::forceX, Particle::AttributeNames::forceY,
                                              Particle::AttributeNames::forceZ>(index, particle.getF());
    _particleSoABuffer.template write<Particle::AttributeNames::owned>(index, particle.isOwned());
  }

  /**
   * Iterator friend class.
   * @tparam ParticleType
   */
  template <class ParticleType, bool modifiable>
  friend class RMMParticleCellIterator;
};

/**
 * SingleCellIterator for the RMMParticleCell.
 * @tparam Particle
 */
template <class Particle, bool modifiable>
class RMMParticleCellIterator : public internal::SingleCellIteratorInterfaceImpl<Particle, modifiable> {
  using CellType = std::conditional_t<
      modifiable,
      RMMParticleCell3T<Particle, RMMParticleCellIterator<Particle, true>, RMMParticleCellIterator<Particle, false>>,
      const RMMParticleCell3T<Particle, RMMParticleCellIterator<Particle, true>,
                              RMMParticleCellIterator<Particle, false>>>;
  using ParticleType = std::conditional_t<modifiable, Particle, const Particle>;

 public:
  /**
   * Default constructor of SingleCellIterator.
   */
  RMMParticleCellIterator() : _cell(nullptr), _index(0), _deleted(false) {}

  /**
   * Constructor of SingleCellIterator.
   * @param cell_arg pointer to the cell of particles.
   * @param ind index of the first particle.
   */
  explicit RMMParticleCellIterator(CellType *cell_arg, size_t ind = 0)
      : _cell(cell_arg), _index(ind), _deleted(false) {}

  //  SingleCellIterator(const SingleCellIterator &cellIterator) {
  //    _cell = cellIterator._cell;
  //    _AoSReservoir = cellIterator._AoSReservoir;
  //    _index = cellIterator._index;
  //  }

  /**
   * @copydoc ParticleIteratorInterface::operator*()
   */
  inline ParticleType &operator*() const override {
    // Particle * ptr = nullptr;
    // ptr = const_cast<Particle *>(& _AoSReservoir);
    Particle *ptr = &_AoSReservoir;
    _cell->buildParticleFromSoA(_index, ptr);
    //_cell->particleAt(_index, ptr);
    return *ptr;
  }

  /**
   * Equality operator.
   * If both iterators are invalid or if they point to the same particle, this returns true.
   * @param rhs
   * @return
   */
  bool operator==(const SingleCellIteratorInterface<Particle, modifiable> &rhs) const override {
    if (auto other = dynamic_cast<const RMMParticleCellIterator *>(&rhs)) {
      return (not this->isValid() and not rhs.isValid()) or (_cell == other->_cell && _index == other->_index);
    } else {
      return false;
    }
  }

  /**
   * Inequality operator.
   * Descrition see operator==
   * @param rhs
   * @return
   */
  bool operator!=(const SingleCellIteratorInterface<Particle, modifiable> &rhs) const override {
    return !(rhs == *this);
  }

  /**
   * Increment operator to get the next particle.
   * @return the next particle, usually ignored.
   */
  inline RMMParticleCellIterator &operator++() override {
    if (not _deleted) {
      if constexpr (modifiable) {
        // only write it if we could actually modify it!
        _cell->writeParticleToSoA(_index, _AoSReservoir);
      }
      ++_index;
    }
    _deleted = false;
    return *this;
  }

  /**
   * Check whether the iterator is valid.
   * @return returns whether the iterator is valid.
   */
  bool isValid() const override { return _cell != nullptr and _index < _cell->numParticles(); }

  /**
   * Get the index of the particle in the cell.
   * @return index of the current particle.
   */
  size_t getIndex() const override { return _index; }

  internal::SingleCellIteratorInterfaceImpl<Particle, modifiable> *clone() const override {
    return new RMMParticleCellIterator<Particle, modifiable>(*this);
  }

 protected:
  /**
   * Deletes the current particle.
   */
  void deleteCurrentParticleImpl() override {
    if constexpr (modifiable) {
      _cell->deleteByIndex(_index);
      _deleted = true;
    } else {
      utils::ExceptionHandler::exception("Error: Trying to delete a particle through a const iterator.");
    }
  }

 private:
  CellType *_cell;
  mutable Particle _AoSReservoir;
  size_t _index;
  bool _deleted;
};

// provide a simpler template for RMMParticleCell, i.e.
// RMMParticleCell<Particle>
/**
 * using declaration for simpler access to RMMParticleCell.
 */
template <class Particle>
using RMMParticleCell =
    RMMParticleCell3T<Particle, RMMParticleCellIterator<Particle, true>, RMMParticleCellIterator<Particle, false>>;

}  // namespace autopas

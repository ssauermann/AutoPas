/**
 * @file VerletClusterCells.h
 * @author jspahl
 * @date 25.3.19
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/CellBorderAndFlagManager.h"
#include "autopas/containers/ParticleContainer.h"
#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/containers/verletClusterLists/VerletClusterCellsParticleIterator.h"
#include "autopas/containers/verletClusterLists/traversals/VerletClusterTraversalInterface.h"
#include "autopas/iterators/ParticleIterator.h"
#include "autopas/iterators/RegionParticleIterator.h"
#include "autopas/utils/ArrayMath.h"
#include "autopas/utils/CudaDeviceVector.h"

namespace autopas {

/**
 * Particles are divided into clusters.
 * The VerletClusterCells class uses neighborhood lists for each cluster pair
 * to calculate pairwise interactions.
 * It is optimized for a constant, i.e. particle independent, cutoff radius of
 * the interaction.
 * @tparam Particle
 */
template <class Particle>
class VerletClusterCells : public ParticleContainer<FullParticleCell<Particle>> {
 public:
  /**
   * Constructor of the VerletClusterCells class.
   * The neighbor lists are build using an estimated density.
   * The box is divided into cuboids with roughly the
   * same side length. The rebuildFrequency should be chosen, s.t. the particles do
   * not move more than a distance of skin/2 between two rebuilds of the lists.
   * @param boxMin the lower corner of the domain
   * @param boxMax the upper corner of the domain
   * @param cutoff the cutoff radius of the interaction
   * @param skin the skin radius
   * @param clusterSize size of clusters
   */
  VerletClusterCells(const std::array<double, 3> boxMin, const std::array<double, 3> boxMax, double cutoff,
                     double skin = 0, int clusterSize = 32)
      : ParticleContainer<FullParticleCell<Particle>>(boxMin, boxMax, cutoff, skin),
        _boxMinWithHalo(utils::ArrayMath::subScalar(boxMin, cutoff + skin)),
        _boxMaxWithHalo(utils::ArrayMath::addScalar(boxMax, cutoff + skin)),
        _clusterSize(clusterSize),
        _isValid(false) {
    this->_cells.resize(1);
    _dummyStarts = {0};
  }

  ContainerOption getContainerType() const override { return ContainerOption::verletClusterCells; }

  /**
   * Function to iterate over all pairs of particles.
   * This function only handles short-range interactions.
   * @param traversal to be used used
   */
  void iteratePairwise(TraversalInterface *traversal) override {
    auto *traversalInterface = dynamic_cast<VerletClusterTraversalInterface<FullParticleCell<Particle>> *>(traversal);
    auto *cellPairTraversal = dynamic_cast<CellPairTraversal<FullParticleCell<Particle>> *>(traversal);

    if ((!traversalInterface) or (!cellPairTraversal)) {
      autopas::utils::ExceptionHandler::exception(
          "trying to use a traversal of wrong type in VerletClusterCells::iteratePairwise");
    }

    traversalInterface->setVerletListPointer(&_neighborCellIds, &_neighborMatrixDim, &_neighborMatrix);

    if (traversalInterface->getSignature() != _lastTraversalSig or (not _isValid)) {
      if (!_isValid) {
        rebuild();
      }
      traversalInterface->rebuildVerlet(_cellsPerDim, this->_cells, _boundingBoxes,
                                        std::ceil(this->getInteractionLength() * _gridSideLengthReciprocal),
                                        this->getInteractionLength());
      _lastTraversalSig = traversalInterface->getSignature();
    }

    cellPairTraversal->setCellsToTraverse(this->_cells);
    traversal->initTraversal();
    traversal->traverseParticlePairs();
    traversal->endTraversal();
  }

  /**
   * @copydoc VerletLists::addParticle()
   */
  void addParticle(const Particle &p) override {
    if (autopas::utils::inBox(p.getR(), this->getBoxMin(), this->getBoxMax())) {
      _isValid = false;
      // removes dummy particles in first cell
      this->_cells[0].resize(_dummyStarts[0]);
      // add particle somewhere, because lists will be rebuild anyways
      this->_cells[0].addParticle(p);
      ++_dummyStarts[0];
    } else {
      utils::ExceptionHandler::exception(
          "VerletCluster: trying to add particle that is not inside the bounding box.\n" + p.toString());
    }
  }

  /**
   * @copydoc VerletLists::addHaloParticle()
   */
  void addHaloParticle(const Particle &haloParticle) override {
    Particle p_copy = haloParticle;
    if (autopas::utils::notInBox(p_copy.getR(), this->getBoxMin(), this->getBoxMax())) {
      _isValid = false;
      // removes dummy particles in first cell
      this->_cells[0].resize(_dummyStarts[0]);
      p_copy.setOwned(false);
      // add particle somewhere, because lists will be rebuild anyways
      this->_cells[0].addParticle(p_copy);
      ++_dummyStarts[0];
    } else {
      utils::ExceptionHandler::exception(
          "VerletCluster: trying to add halo particle that is inside the bounding box.\n" + haloParticle.toString());
    }
  }

  /**
   * Update a halo particle of the container with the given haloParticle.
   * @param haloParticle Particle to be updated.
   * @return Returns true if the particle was updated, false if no particle could be found.
   */
  bool updateHaloParticle(const Particle &haloParticle) override {
    Particle pCopy = haloParticle;
    pCopy.setOwned(false);

    for (auto it = getRegionIterator(utils::ArrayMath::subScalar(pCopy.getR(), this->getSkin() / 2),
                                     utils::ArrayMath::addScalar(pCopy.getR(), this->getSkin() / 2),
                                     IteratorBehavior::haloOnly);
         it.isValid(); ++it) {
      if (pCopy.getID() == it->getID()) {
        *it = pCopy;
        return true;
      }
    }
    return false;
  }

  /**
   * Rebuilds the neighbor lists.
   * @param traversal The used traversal.
   */
  void rebuildNeighborLists(TraversalInterface *traversal) override {
    auto *traversalInterface = dynamic_cast<VerletClusterTraversalInterface<FullParticleCell<Particle>> *>(traversal);
    if (!traversalInterface) {
      autopas::utils::ExceptionHandler::exception(
          "trying to use a traversal of wrong type in VerletClusterCells::iteratePairwise");
    }
    if (not _isValid) {
      rebuild();
    }

    traversalInterface->setVerletListPointer(&_neighborCellIds, &_neighborMatrixDim, &_neighborMatrix);

    traversalInterface->rebuildVerlet(_cellsPerDim, this->_cells, _boundingBoxes,
                                      std::ceil(this->getInteractionLength() * _gridSideLengthReciprocal),
                                      this->getInteractionLength());
    _lastTraversalSig = traversalInterface->getSignature();
  }

  /**
   * @copydoc VerletLists::deleteHaloParticles
   */
  void deleteHaloParticles() override {
    _isValid = false;
    for (size_t i = 0; i < this->_cells.size(); ++i) {
      for (size_t j = 0; j < _dummyStarts[i];) {
        if (not this->_cells[i]._particles[j].isOwned()) {
          // set position outside the domain with other dummy particles
          auto pos = this->_cells[i]._particles[j].getR();
          pos[0] += _boxMaxWithHalo[2] + 8 * this->getInteractionLength();
          this->_cells[i]._particles[j].setR(pos);
          // one more dummy particle
          --_dummyStarts[i];
          // swap last non dummy particle with the halo particle to remove
          std::swap(this->_cells[i]._particles[j], this->_cells[i]._particles[_dummyStarts[i]]);
        } else {
          // move on if no halo particle was removed
          ++j;
        }
      }
    }
  }

  /**
   * @copydoc VerletLists::updateContainer()
   */
  std::vector<Particle> updateContainer() override {
    AutoPasLog(debug, "updating container");

    deleteHaloParticles();

    std::vector<Particle> outsideParticles;
    for (auto iter = begin(autopas::IteratorBehavior::ownedOnly); iter.isValid(); ++iter) {
      if (utils::notInBox(iter->getR(), this->getBoxMin(), this->getBoxMax())) {
        outsideParticles.push_back(*iter);
        internal::deleteParticle(iter);
      }
    }

    return outsideParticles;
  }

  bool isContainerUpdateNeeded() const override {
    if (not _isValid) {
      return true;
    }
    for (size_t i = 0; i < this->_cells.size(); ++i) {
      size_t pid = 0;
      const size_t end = (_boundingBoxes[i].size() > 0) ? _boundingBoxes[i].size() - 1 : 0;

      for (size_t cid = 0; cid < end; ++cid) {
        for (unsigned int pic = 0; pic < _clusterSize; ++pic) {
          if (not particleInSkinOfBox(_boundingBoxes[i][cid], this->_cells[i][pid])) {
            return true;
          }
          ++pid;
        }
      }
      for (unsigned int pic = 0; pic < _clusterSize && pid < _dummyStarts[i]; ++pic) {
        if (not particleInSkinOfBox(_boundingBoxes[i][_boundingBoxes[i].size() - 1], this->_cells[i][pid])) {
          return true;
        }
        ++pid;
      }
    }

    return false;
  }

  TraversalSelectorInfo getTraversalSelectorInfo() const override {
    return TraversalSelectorInfo(_cellsPerDim, this->getInteractionLength(),
                                 {_gridSideLength, _gridSideLength, this->getBoxMax()[2] - this->getBoxMin()[2]},
                                 _clusterSize);
  }

  ParticleIteratorWrapper<Particle, true> begin(IteratorBehavior behavior = IteratorBehavior::haloAndOwned) override {
    return ParticleIteratorWrapper<Particle, true>(
        new internal::VerletClusterCellsParticleIterator<Particle, FullParticleCell<Particle>, true>(
            &this->_cells, _dummyStarts, _boxMaxWithHalo[0] + 8 * this->getInteractionLength(), behavior));
  }

  ParticleIteratorWrapper<Particle, false> begin(
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) const override {
    return ParticleIteratorWrapper<Particle, false>(
        new internal::VerletClusterCellsParticleIterator<Particle, FullParticleCell<Particle>, false>(
            &this->_cells, _dummyStarts, _boxMaxWithHalo[0] + 8 * this->getInteractionLength(), behavior));
  }

  ParticleIteratorWrapper<Particle, true> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner,
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) override {
    // Special iterator requires sorted cells
#ifdef AUTOPAS_OPENMP
#pragma omp single
#endif
    if (not _isValid) {
      rebuild();
    }
    // there is an implicit barrier at end of single!

    // restrict search area to the region where particles are
    const auto lowerCornerInBounds = utils::ArrayMath::max(lowerCorner, _boxMinWithHalo);
    const auto upperCornerInBounds = utils::ArrayMath::min(higherCorner, _boxMaxWithHalo);

    // Find cells intersecting the search region
    size_t xmin = (size_t)((lowerCornerInBounds[0] - _boxMinWithHalo[0] - this->getSkin()) * _gridSideLengthReciprocal);
    size_t ymin = (size_t)((lowerCornerInBounds[1] - _boxMinWithHalo[1] - this->getSkin()) * _gridSideLengthReciprocal);

    size_t xlength =
        ((size_t)((upperCornerInBounds[0] - _boxMinWithHalo[0] + this->getSkin()) * _gridSideLengthReciprocal) - xmin) +
        1;
    size_t ylength =
        ((size_t)((upperCornerInBounds[1] - _boxMinWithHalo[1] + this->getSkin()) * _gridSideLengthReciprocal) - ymin) +
        1;

    std::vector<size_t> cellsOfInterest(xlength * ylength);

    auto cellsOfInterestIterator = cellsOfInterest.begin();
    int start = xmin + ymin * _cellsPerDim[0];
    for (size_t i = 0; i < ylength; ++i) {
      std::iota(cellsOfInterestIterator, cellsOfInterestIterator + xlength, start + i * _cellsPerDim[0]);
      cellsOfInterestIterator += xlength;
    }

    return ParticleIteratorWrapper<Particle, true>(
        new internal::VerletClusterCellsRegionParticleIterator<Particle, FullParticleCell<Particle>, true>(
            &this->_cells, _dummyStarts, lowerCornerInBounds, upperCornerInBounds, cellsOfInterest,
            _boxMaxWithHalo[0] + 8 * this->getInteractionLength(), behavior, this->getSkin()));
  }

  ParticleIteratorWrapper<Particle, false> getRegionIterator(
      const std::array<double, 3> &lowerCorner, const std::array<double, 3> &higherCorner,
      IteratorBehavior behavior = IteratorBehavior::haloAndOwned) const override {
    // restrict search area to the region where particles are
    const auto lowerCornerInBounds = utils::ArrayMath::max(lowerCorner, _boxMinWithHalo);
    const auto upperCornerInBounds = utils::ArrayMath::min(higherCorner, _boxMaxWithHalo);

    // Special iterator requires sorted cells.
    // Otherwise all cells are traversed with the general Iterator.
    if (_isValid) {
      // Find cells intersecting the search region
      size_t xmin =
          (size_t)((lowerCornerInBounds[0] - _boxMinWithHalo[0] - this->getSkin()) * _gridSideLengthReciprocal);
      size_t ymin =
          (size_t)((lowerCornerInBounds[1] - _boxMinWithHalo[1] - this->getSkin()) * _gridSideLengthReciprocal);

      size_t xlength =
          (((upperCornerInBounds[0] - _boxMinWithHalo[0] + this->getSkin()) * _gridSideLengthReciprocal) - xmin) + 1;
      size_t ylength =
          (((upperCornerInBounds[1] - _boxMinWithHalo[1] + this->getSkin()) * _gridSideLengthReciprocal) - ymin) + 1;

      std::vector<size_t> cellsOfInterest(xlength * ylength);

      auto cellsOfInterestIterator = cellsOfInterest.begin();
      int start = xmin + ymin * _cellsPerDim[0];
      for (size_t i = 0; i < ylength; ++i) {
        std::iota(cellsOfInterestIterator, cellsOfInterestIterator + xlength, start + i * _cellsPerDim[0]);
        cellsOfInterestIterator += xlength;
      }
      return ParticleIteratorWrapper<Particle, false>(
          new internal::VerletClusterCellsRegionParticleIterator<Particle, FullParticleCell<Particle>, false>(
              &this->_cells, _dummyStarts, lowerCornerInBounds, upperCornerInBounds, cellsOfInterest,
              _boxMaxWithHalo[0] + 8 * this->getInteractionLength(), behavior, this->getSkin()));
    } else {
      // check all cells
      // As dummy particles are outside the domain they are only found if the search region is outside the domain.
      std::vector<size_t> cellsOfInterest(this->_cells.size());
      std::iota(cellsOfInterest.begin(), cellsOfInterest.end(), 0);

      return ParticleIteratorWrapper<Particle, false>(
          new internal::RegionParticleIterator<Particle, FullParticleCell<Particle>, false>(
              &this->_cells, lowerCornerInBounds, upperCornerInBounds, cellsOfInterest, nullptr, behavior));
    }
  }

  /**
   * Get the number of particles excluding dummy Particles saved in the container.
   * @return Number of particles in the container.
   */
  unsigned long getNumParticles() const override {
    size_t numParticles = 0ul;
#ifdef AUTOPAS_OPENMP
    // @todo: find a sensible value for magic number
    // numThreads should be at least 1 and maximal max_threads
    int numThreads = std::max(1, std::min(omp_get_max_threads(), (int)(this->_cells.size() / 1000)));
    AutoPasLog(trace, "Using {} threads", numThreads);
#pragma omp parallel for num_threads(numThreads) reduction(+ : numParticles)
#endif
    for (size_t index = 0; index < _dummyStarts.size(); ++index) {
      numParticles += _dummyStarts[index];
    }
    return numParticles;
  }

  /**
   * Deletes all particles from the container.
   */
  void deleteAllParticles() override {
    _isValid = false;
    std::fill(_dummyStarts.begin(), _dummyStarts.end(), 0);
    ParticleContainer<FullParticleCell<Particle>>::deleteAllParticles();
  }

  /**
   * Deletes all Dummy Particles in the container
   */
  void deleteDummyParticles() {
    for (size_t i = 0; i < this->_cells.size(); ++i) {
      this->_cells[i].resize(_dummyStarts[i]);
    }
    _isValid = false;
  }

 protected:
  /**
   * Recalculate grids and clusters,
   * build verlet lists and pad clusters.
   * @return Vector of particles containing particles no longer in the box
   */
  std::vector<Particle> rebuild() {
    deleteDummyParticles();
    _boundingBoxes.clear();
    // get the dimensions and volumes of the box
    std::array<double, 3> boxSize{};
    double volume = 1.0;

    for (int d = 0; d < 3; ++d) {
      boxSize[d] = _boxMaxWithHalo[d] - _boxMinWithHalo[d];
      volume *= boxSize[d];
    }

    // get all particles and clear clusters
    std::vector<Particle> invalidParticles;
    std::vector<Particle> outsideParticles;

    for (size_t i = 0; i < this->_cells.size(); ++i) {
      for (auto &p : this->_cells[i]._particles) {
        if (utils::inBox(p.getR(), this->getBoxMin(), this->getBoxMax())) {
          invalidParticles.push_back(p);
        } else {
          if (p.isOwned()) {
            outsideParticles.push_back(p);
          } else {
            invalidParticles.push_back(p);
          }
        }
      }
      this->_cells[i].clear();
    }

    // estimate particle density
    double density = (std::max(1.0, (double)invalidParticles.size())) / volume;

    // guess optimal grid side length
    _gridSideLength = std::cbrt(((double)_clusterSize) / density);
    _gridSideLengthReciprocal = 1 / _gridSideLength;

    // get cells per dimension
    size_t sizeGrid = 1;
    for (int d = 0; d < 2; d++) {
      _cellsPerDim[d] = static_cast<size_t>(std::ceil(boxSize[d] * _gridSideLengthReciprocal));
      sizeGrid *= _cellsPerDim[d];
    }
    _cellsPerDim[2] = static_cast<size_t>(1);

    // resize to number of grids
    this->_cells.resize(sizeGrid);

    _dummyStarts.clear();
    _dummyStarts.resize(sizeGrid);
    _boundingBoxes.resize(sizeGrid);

    // put particles into grid cells
    for (size_t i = 0; i < invalidParticles.size(); ++i) {
      size_t index =
          (size_t)((invalidParticles[i].getR()[0] - _boxMinWithHalo[0]) * _gridSideLengthReciprocal) +
          (size_t)((invalidParticles[i].getR()[1] - _boxMinWithHalo[1]) * _gridSideLengthReciprocal) * _cellsPerDim[0];
      this->_cells[index].addParticle(invalidParticles[i]);
    }

    // sort by last dimension and add dummy particles
#if defined(AUTOPAS_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
    for (size_t i = 0; i < sizeGrid; ++i) {
      this->_cells[i].sortByDim(2);
      const auto numParticles = this->_cells[i].numParticles();

      _dummyStarts[i] = numParticles;
      unsigned int numDummys = _clusterSize;
      if (numParticles > 0) {
        numDummys -= (numParticles % (size_t)_clusterSize);
      }

      Particle dummyParticle = Particle();
      for (unsigned int j = 0; j < numDummys; ++j) {
        dummyParticle.setR({_boxMaxWithHalo[0] + 8 * this->getInteractionLength() + static_cast<double>(i),
                            _boxMaxWithHalo[1] + 8 * this->getInteractionLength() + static_cast<double>(j),
                            _boxMaxWithHalo[2] + 8 * this->getInteractionLength()});
        dummyParticle.setID(std::numeric_limits<size_t>::max());
        dummyParticle.setOwned(false);
        this->_cells[i].addParticle(dummyParticle);
      }
    }

    // make bounding boxes
#if defined(AUTOPAS_OPENMP)
#pragma omp parallel for schedule(guided)
#endif
    for (size_t i = 0; i < sizeGrid; ++i) {
      const size_t nClusters = this->_cells[i].numParticles() / _clusterSize;

      _boundingBoxes[i].resize(nClusters, {_boxMaxWithHalo[0], _boxMaxWithHalo[1], _boxMaxWithHalo[2],
                                           _boxMinWithHalo[0], _boxMinWithHalo[1], _boxMinWithHalo[2]});

      for (size_t cid = 0; cid < nClusters; ++cid)
        for (size_t pid = cid * _clusterSize; pid < _dummyStarts[i]; ++pid) {
          expandBoundingBox(_boundingBoxes[i][cid], this->_cells[i][pid]);
        }
    }

    _isValid = true;
    return outsideParticles;
  }

 private:
  /**
   * Expands a bounding Box such the Particle is in it.
   * @param box
   * @param p
   */
  void expandBoundingBox(std::array<double, 6> &box, const Particle &p) {
    for (int i = 0; i < 3; ++i) {
      box[i] = std::min(box[i], p.getR()[i]);
      box[3 + i] = std::max(box[3 + i], p.getR()[i]);
    }
  }

  /**
   * Checks if particle is within skin of bounding box.
   * @param box
   * @param p
   */
  bool particleInSkinOfBox(const std::array<double, 6> &box, const Particle &p) const {
    for (int i = 0; i < 3; ++i) {
      if (box[0 + i] - this->getSkin() > p.getR()[i] or box[3 + i] + this->getSkin() < p.getR()[i]) return false;
    }
    return true;
  }
  std::array<double, 3> _boxMinWithHalo, _boxMaxWithHalo;

  /// indices where dummy particles in the cells start
  std::vector<size_t> _dummyStarts;

  // number of particles in a cluster
  unsigned int _clusterSize;

  // id of neighbor clusters of a clusters in the form [mycell][mycluster] pair(othercell, othercluster)
  std::vector<std::vector<std::vector<std::pair<size_t, size_t>>>> _neighborCellIds;

  size_t _neighborMatrixDim;
  utils::CudaDeviceVector<unsigned int> _neighborMatrix;

  // bounding boxes of all clusters (xmin,ymin,zmin,xmax,ymax,zmax)
  std::vector<std::vector<std::array<double, 6>>> _boundingBoxes;

  // side length of xy-grid and reciprocal
  double _gridSideLength;
  double _gridSideLengthReciprocal;

  // dimensions of grid
  std::array<size_t, 3> _cellsPerDim;

  // specifies if the neighbor list is currently valid
  bool _isValid;

  /// Signature of the last Traversal to trigger rebuild when a new one is used
  std::tuple<TraversalOption, DataLayoutOption, bool> _lastTraversalSig;
};
}  // namespace autopas

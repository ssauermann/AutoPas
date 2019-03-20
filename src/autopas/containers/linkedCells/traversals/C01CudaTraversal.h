/**
 * @file C01CudaTraversal.h
 * @author jspahl
 * @date 11.03.2019
 */

#pragma once

#include "LinkedCellTraversalInterface.h"
#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/pairwiseFunctors/CellFunctor.h"
#include "autopas/utils/CudaDeviceVector.h"
#include "autopas/utils/ThreeDimensionalMapping.h"
#include "autopas/utils/WrapOpenMP.h"
#if defined(AUTOPAS_CUDA)
#include "autopas/pairwiseFunctors/LJFunctor.h"
#include "autopas/pairwiseFunctors/LJFunctorCuda.cuh"
#include "autopas/utils/CudaExceptionHandler.h"
#include "autopas/utils/CudaStreamHandler.h"
#endif
namespace autopas {

/**
 * This class provides the c01 traversal on the GPU.
 *
 * The traversal calculates all cells in parallel
 *
 * @tparam ParticleCell the type of cells
 * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
 * @tparam DataLayout
 * @tparam useNewton3
 */
template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
class C01CudaTraversal : public CellPairTraversal<ParticleCell>, public LinkedCellTraversalInterface<ParticleCell> {
 public:
  /**
   * Constructor of the c01 traversal.
   * @param dims The dimensions of the cellblock, i.e. the number of cells in x,
   * y and z direction.
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   */
  explicit C01CudaTraversal(const std::array<unsigned long, 3> &dims, PairwiseFunctor *pairwiseFunctor)
      : CellPairTraversal<ParticleCell>(dims), _functor(pairwiseFunctor) {
    computeOffsets();
  }

  /**
   * Computes pairs used in processBaseCell()
   */
  void computeOffsets();

  /**
   * @copydoc LinkedCellTraversalInterface::traverseCellPairs()
   */
  void traverseCellPairs(std::vector<ParticleCell> &cells) override;

  TraversalOption getTraversalType() override { return TraversalOption::c01Cuda; }

  /**
   * Cuda traversal is only usable if using a GPU.
   * @return
   */
  bool isApplicable() override {
#if defined(AUTOPAS_CUDA)
    int nDevices;
    cudaGetDeviceCount(&nDevices);
    return (DataLayout == DataLayoutOption::cuda) && (nDevices > 0);
#else
    return false;
#endif
  }

  DataLayoutOption requiredDataLayout() override { return DataLayoutOption::aos; }

 private:
  /**
   * Pairs for processBaseCell().
   */
  std::vector<int> _cellOffsets;

  /**
   * Pairwise Functor to be used
   */
  PairwiseFunctor *_functor;

  /**
   * SoA Storage cell for all cells and device Memory
   */
  ParticleCell _storageCell;

  /**
   * Non Halo Cell Ids
   */
  utils::CudaDeviceVector<unsigned int> _nonHaloCells;

  /**
   * device cell offsets
   */
  utils::CudaDeviceVector<int> _deviceCellOffsets;

  /**
   * device cell sizes storage
   */
  utils::CudaDeviceVector<size_t> _deviceCellSizes;
};

template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
inline void C01CudaTraversal<ParticleCell, PairwiseFunctor, DataLayout, useNewton3>::computeOffsets() {
  for (int z = -1; z <= 1; ++z) {
    for (int y = -1; y <= 1; ++y) {
      for (int x = -1; x <= 1; ++x) {
        int offset = (z * this->_cellsPerDimension[1] + y) * this->_cellsPerDimension[0] + x;
        if (not useNewton3) {
          _cellOffsets.push_back(offset);
        } else {
          if (offset > 0) {
            _cellOffsets.push_back(offset);
          }
        }
      }
    }
  }
#if defined(AUTOPAS_CUDA)
  _deviceCellOffsets.copyHostToDevice(_cellOffsets.size(), _cellOffsets.data());
#endif

  std::vector<unsigned int> nonHaloCells;
  const unsigned long end_x = this->_cellsPerDimension[0] - 1;
  const unsigned long end_y = this->_cellsPerDimension[1] - 1;
  const unsigned long end_z = this->_cellsPerDimension[2] - 1;

  for (unsigned long z = 1; z < end_z; ++z) {
    for (unsigned long y = 1; y < end_y; ++y) {
      for (unsigned long x = 1; x < end_x; ++x) {
        nonHaloCells.push_back(utils::ThreeDimensionalMapping::threeToOneD(x, y, z, this->_cellsPerDimension));
      }
    }
  }
#if defined(AUTOPAS_CUDA)
  _nonHaloCells.copyHostToDevice(nonHaloCells.size(), nonHaloCells.data());
#endif
}  // namespace autopas

template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
inline void C01CudaTraversal<ParticleCell, PairwiseFunctor, DataLayout, useNewton3>::traverseCellPairs(
    std::vector<ParticleCell> &cells) {
  if (not this->isApplicable()) {
    utils::ExceptionHandler::exception(
        "The Cuda traversal cannot work with Data Layouts other than DataLayoutOption::cuda!");
  }
#if defined(AUTOPAS_CUDA)
  // load CUDA SOA
  std::vector<size_t> cellSizePartialSum = {0};
  size_t maxParticlesInCell = 0;

  for (size_t i = 0; i < cells.size(); ++i) {
    _functor->SoALoader(cells[i], _storageCell._particleSoABuffer, cellSizePartialSum.back());
    const size_t size = cells[i].numParticles();

    maxParticlesInCell = std::max(maxParticlesInCell, size);
    cellSizePartialSum.push_back(cellSizePartialSum.back() + size);
  }

  _functor->getCudaWrapper().setNumThreads(((maxParticlesInCell - 1) / 32 + 1) * 32);
  _deviceCellSizes.copyHostToDevice(cellSizePartialSum.size(), cellSizePartialSum.data());
  _functor->deviceSoALoader(_storageCell._particleSoABuffer, _storageCell._particleSoABufferDevice);

  // wait for copies to be done
  utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());

  if (useNewton3) {
    _functor->getCudaWrapper().LinkedCellsTraversalN3Wrapper(
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posX>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posY>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posZ>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceX>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceY>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceZ>().get(),
        _nonHaloCells.size(), _nonHaloCells.get(), _deviceCellSizes.size(), _deviceCellSizes.get(),
        _deviceCellOffsets.size(), _deviceCellOffsets.get(), 0);
  } else {
    _functor->getCudaWrapper().LinkedCellsTraversalNoN3Wrapper(
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posX>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posY>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::posZ>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceX>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceY>().get(),
        _storageCell._particleSoABufferDevice.template get<ParticleCell::ParticleType::AttributeNames::forceZ>().get(),
        _nonHaloCells.size(), _nonHaloCells.get(), _deviceCellSizes.size(), _deviceCellSizes.get(),
        _deviceCellOffsets.size(), _deviceCellOffsets.get(), 0);
  }
  utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());

  // Extract
  _functor->deviceSoAExtractor(_storageCell._particleSoABuffer, _storageCell._particleSoABufferDevice);

  for (size_t i = 0; i < cells.size(); ++i) {
    _functor->SoAExtractor(cells[i], _storageCell._particleSoABuffer, cellSizePartialSum[i]);
  }

  utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());
#endif
}

}  // namespace autopas

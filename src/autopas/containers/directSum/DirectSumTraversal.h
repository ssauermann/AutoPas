/**
 * @file DirectSumTraversal.h
 * @author F. Gratl
 * @date 11/23/18
 */

#pragma once

#include <vector>
#include "DirectSumTraversalInterface.h"
#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/pairwiseFunctors/CellFunctor.h"
#include "autopas/utils/DataLayoutConverter.h"
#if defined(AUTOPAS_CUDA)
#include "cuda_runtime.h"
#endif

namespace autopas {

/**
 * This sum defines the traversal typically used by the DirectSum container.
 *
 * @tparam ParticleCell the type of cells
 * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
 * @tparam DataLayout
 * @tparam useNewton3
 */
template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
class DirectSumTraversal : public CellPairTraversal<ParticleCell>, public DirectSumTraversalInterface<ParticleCell> {
 public:
  /**
   * Constructor for the DirectSum traversal.
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   */
  DirectSumTraversal(PairwiseFunctor *pairwiseFunctor)
      : CellPairTraversal<ParticleCell>({2, 1, 1}),
        _cellFunctor(CellFunctor<typename ParticleCell::ParticleType, ParticleCell, PairwiseFunctor, DataLayout,
                                 useNewton3, true>(pairwiseFunctor)),
        _dataLayoutConverter(pairwiseFunctor) {}

  TraversalOption getTraversalType() override { return TraversalOption::directSumTraversal; }

  bool isApplicable() override {
    int nDevices = 0;
#if defined(AUTOPAS_CUDA)
    cudaGetDeviceCount(&nDevices);
#endif
    if (DataLayout == DataLayoutOption::cuda)
      return nDevices > 0;
    else
      return true;
  }

  void initTraversal(std::vector<ParticleCell> &cells) override {
    for (auto &cell : cells) {
      _dataLayoutConverter.loadDataLayout(cell);
    }
  }

  void endTraversal(std::vector<ParticleCell> &cells) override {
    for (auto &cell : cells) {
      _dataLayoutConverter.storeDataLayout(cell);
    }
  }

  /**
   * @copydoc LinkedCellTraversalInterface::traverseCellPairs()
   * @note This function expects a vector of exactly two cells. First cell is the main region, second is halo.
   */
  void traverseCellPairs(std::vector<ParticleCell> &cells) override;

 private:
  /**
   * CellFunctor to be used for the traversal defining the interaction between two cells.
   */
  CellFunctor<typename ParticleCell::ParticleType, ParticleCell, PairwiseFunctor, DataLayout, useNewton3, true>
      _cellFunctor;

  /**
   * Data Layout Converter to be used with this traversal
   */
  utils::DataLayoutConverter<PairwiseFunctor, DataLayout> _dataLayoutConverter;
};

template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
void DirectSumTraversal<ParticleCell, PairwiseFunctor, DataLayout, useNewton3>::traverseCellPairs(
    std::vector<ParticleCell> &cells) {
  // Assume cell[0] is the main domain and cell[1] is the halo
  _cellFunctor.processCell(cells[0]);
  _cellFunctor.processCellPair(cells[0], cells[1]);
}

}  // namespace autopas

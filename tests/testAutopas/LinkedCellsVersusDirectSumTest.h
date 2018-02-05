/*
 * LinkedCellsVersusDirectSumTest.h
 *
 *  Created on: 23 Jan 2018
 *      Author: tchipevn
 */

#ifndef TESTS_TESTAUTOPAS_LINKEDCELLSVERSUSDIRECTSUMTEST_H_
#define TESTS_TESTAUTOPAS_LINKEDCELLSVERSUSDIRECTSUMTEST_H_

#include "autopas.h"
#include "gtest/gtest.h"

class LinkedCellsVersusDirectSumTest : public ::testing::Test {
 public:
  LinkedCellsVersusDirectSumTest();

  ~LinkedCellsVersusDirectSumTest() override = default;

  std::array<double, 3> getBoxMin() const { return {0.0, 0.0, 0.0}; }

  std::array<double, 3> getBoxMax() const { return {3.0, 3.0, 3.0}; }

  double getCutoff() const { return 1.0; }

 protected:
  double fRand(double fMin, double fMax) const;

  std::array<double, 3> randomPosition(
      const std::array<double, 3> &boxMin,
      const std::array<double, 3> &boxMax) const;

  void fillContainerWithMolecules(
      unsigned long numMolecules,
      autopas::ParticleContainer<autopas::MoleculeLJ,
                                 autopas::FullParticleCell<autopas::MoleculeLJ>>
          &cont) const;

  void test(unsigned long numMolecules, double rel_err_tolerance);

  autopas::DirectSum<autopas::MoleculeLJ,
                     autopas::FullParticleCell<autopas::MoleculeLJ>>
      _directSum;
  autopas::LinkedCells<autopas::MoleculeLJ,
                       autopas::FullParticleCell<autopas::MoleculeLJ>>
      _linkedCells;
};

#endif /* TESTS_TESTAUTOPAS_LINKEDCELLSVERSUSDIRECTSUMTEST_H_ */
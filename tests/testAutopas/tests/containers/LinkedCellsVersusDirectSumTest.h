/**
 * @file LinkedCellsVersusDirectSumTest.h
 * @author tchipev
 * @date 23.01.18
 */

#pragma once

#include <gtest/gtest.h>

#include "AutoPasTestBase.h"
#include "autopas/containers/directSum/DirectSum.h"
#include "autopas/containers/linkedCells/LinkedCells.h"
#include "autopas/molecularDynamics/ParticlePropertiesLibrary.h"
#include "testingHelpers/commonTypedefs.h"

class LinkedCellsVersusDirectSumTest : public AutoPasTestBase {
 public:
  LinkedCellsVersusDirectSumTest();

  ~LinkedCellsVersusDirectSumTest() override = default;

  std::array<double, 3> getBoxMin() const { return {0.0, 0.0, 0.0}; }

  std::array<double, 3> getBoxMax() const { return {3.0, 3.0, 3.0}; }

  double getCutoff() const { return 1.0; }

 protected:
  void test(unsigned long numMolecules, double rel_err_tolerance);

  autopas::DirectSum<FMCell> _directSum;
  autopas::LinkedCells<FMCell> _linkedCells;
};

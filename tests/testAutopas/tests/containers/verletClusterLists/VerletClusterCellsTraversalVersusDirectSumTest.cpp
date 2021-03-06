/**
 * @file VerletClusterCellsTraversalVersusDirectSumTest.cpp
 * @author jspahl
 * @date 3.04.19
 */

#include "VerletClusterCellsTraversalVersusDirectSumTest.h"

#include "autopas/containers/directSum/DirectSumTraversal.h"
#include "autopas/containers/verletClusterLists/traversals/VerletClusterCellsTraversal.h"
#include "autopas/molecularDynamics/LJFunctor.h"

VerletClusterCellsTraversalVersusDirectSumTest::VerletClusterCellsTraversalVersusDirectSumTest()
    : _directSum(getBoxMin(), getBoxMax(), getCutoff(), 0.2),
      _verletCluster(getBoxMin(), getBoxMax(), getCutoff(), 0.2, _clusterSize) {}

double VerletClusterCellsTraversalVersusDirectSumTest::fRand(double fMin, double fMax) const {
  double f = static_cast<double>(rand()) / RAND_MAX;
  return fMin + f * (fMax - fMin);
}

std::array<double, 3> VerletClusterCellsTraversalVersusDirectSumTest::randomPosition(
    const std::array<double, 3> &boxMin, const std::array<double, 3> &boxMax) const {
  std::array<double, 3> r{};
  for (int d = 0; d < 3; ++d) {
    r[d] = fRand(boxMin[d], boxMax[d]);
  }
  return r;
}

void VerletClusterCellsTraversalVersusDirectSumTest::fillContainerWithMolecules(
    unsigned long numMolecules, autopas::ParticleContainer<FMCell> &cont) const {
  srand(42);  // fixed seedpoint

  std::array<double, 3> boxMin(cont.getBoxMin()), boxMax(cont.getBoxMax());

  for (unsigned long i = 0; i < numMolecules; ++i) {
    auto id = static_cast<unsigned long>(i);
    Molecule m(randomPosition(boxMin, boxMax), {0., 0., 0.}, id);
    cont.addParticle(m);
  }
}

template <bool useNewton3, autopas::DataLayoutOption::Value dataLayout, bool calculateGlobals>
void VerletClusterCellsTraversalVersusDirectSumTest::test(unsigned long numMolecules, double rel_err_tolerance) {
  fillContainerWithMolecules(numMolecules, _directSum);
  // now fill second container with the molecules from the first one, because
  // otherwise we generate new particles
  for (auto it = _directSum.begin(); it.isValid(); ++it) {
    _verletCluster.addParticle(*it);
  }

  constexpr double eps = 1.0;
  constexpr double sig = 1.0;
  constexpr bool shifting = false;
  constexpr bool mixing = false;
  autopas::LJFunctor<Molecule, FMCell, shifting, mixing, autopas::FunctorN3Modes::Both, calculateGlobals> funcDS(
      getCutoff());
  funcDS.setParticleProperties(eps * 24, sig * sig);
  autopas::LJFunctor<Molecule, FMCell, shifting, mixing, autopas::FunctorN3Modes::Both, calculateGlobals> funcVC(
      getCutoff());
  funcVC.setParticleProperties(eps * 24, sig * sig);

  autopas::DirectSumTraversal<FMCell, decltype(funcDS), autopas::DataLayoutOption::aos, useNewton3> traversalDS(
      &funcDS, getCutoff());
  autopas::VerletClusterCellsTraversal<FMCell, decltype(funcVC), dataLayout, useNewton3> traversalVerletCluster(
      &funcVC, _clusterSize);

  funcDS.initTraversal();
  _directSum.iteratePairwise(&traversalDS);
  funcDS.endTraversal(useNewton3);

  funcVC.initTraversal();
  _verletCluster.iteratePairwise(&traversalVerletCluster);
  funcVC.endTraversal(useNewton3);

  _verletCluster.deleteDummyParticles();

  auto itDirect = _directSum.begin();
  auto itLinked = _verletCluster.begin();

  std::vector<std::array<double, 3>> forcesDirect(numMolecules), forcesVerlet(numMolecules);
  // get and sort by id, the
  for (auto it = _directSum.begin(); it.isValid(); ++it) {
    Molecule &m = *it;
    forcesDirect.at(m.getID()) = m.getF();
  }

  for (auto it = _verletCluster.begin(); it.isValid(); ++it) {
    Molecule &m = *it;
    forcesVerlet.at(m.getID()) = m.getF();
  }

  for (size_t i = 0; i < numMolecules; ++i) {
    for (int d = 0; d < 3; ++d) {
      double f1 = forcesDirect[i][d];
      double f2 = forcesVerlet[i][d];
      double abs_err = std::abs(f1 - f2);
      double rel_err = std::abs(abs_err / f1);
      EXPECT_LT(rel_err, rel_err_tolerance) << " for ParticleID: " << i << " dim:" << d << " " << f1 << " vs " << f2;
    }
  }

  if (calculateGlobals) {
    EXPECT_LT(std::abs((funcDS.getUpot() - funcVC.getUpot()) / funcDS.getUpot()), rel_err_tolerance)
        << funcDS.getUpot() << " vs " << funcVC.getUpot();
    EXPECT_LT(std::abs((funcDS.getVirial() - funcVC.getVirial()) / funcDS.getVirial()), rel_err_tolerance)
        << funcDS.getVirial() << " vs " << funcVC.getVirial();
  }
}
TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_100) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<false, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_500) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<false, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_100) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-13;

  test<true, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_500) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<true, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::aos>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testSoA_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::soa>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3SoA_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::soa>(numMolecules, rel_err_tolerance);
}

#ifdef AUTOPAS_CUDA
TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_100) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-13;

  test<false, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_500) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<false, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_100) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-13;

  test<true, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_500) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<true, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_1000) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::cuda>(numMolecules, rel_err_tolerance);
}
#endif

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_100Globals) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-11;

  test<false, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_500Globals) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<false, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testAoS_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_100Globals) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-11;

  test<true, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_500Globals) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<true, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3AoS_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::aos, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testSoA_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::soa, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testN3SoA_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::soa, true>(numMolecules, rel_err_tolerance);
}

#ifdef AUTOPAS_CUDA
TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_100Globals) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-11;

  test<false, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_500Globals) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<false, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCuda_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<false, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_100Globals) {
  unsigned long numMolecules = 100;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-11;

  test<true, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_500Globals) {
  unsigned long numMolecules = 500;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1e-12;

  test<true, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}

TEST_F(VerletClusterCellsTraversalVersusDirectSumTest, testCudaN3_1000Globals) {
  unsigned long numMolecules = 1000;

  // empirically determined and set near the minimal possible value
  // i.e. if something changes, it may be needed to increase value
  // (and OK to do so)
  double rel_err_tolerance = 1.5e-12;
  test<true, autopas::DataLayoutOption::cuda, true>(numMolecules, rel_err_tolerance);
}
#endif

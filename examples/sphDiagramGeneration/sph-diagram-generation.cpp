/**
 * @file sph-diagram-generation.cpp
 * @date 25.01.2018
 * @author seckler
 */

#include <array>
#include <iostream>
#include "../md/mdutils.h"
#include "autopas/autopasIncludes.h"
#include "autopas/sph/autopassph.h"
#include "autopas/utils/Timer.h"

template <class Container, class Functor>
void measureContainer(Container *cont, Functor *func, int numParticles, int numIterations, bool useNewton3);

void addParticles(
    autopas::LinkedCells<autopas::sph::SPHParticle, autopas::FullParticleCell<autopas::sph::SPHParticle>> &sph_system,
    int numParticles) {
  // Place SPH particles

  srand(42);  // fixed seedpoint

  std::array<double, 3> boxMin(sph_system.getBoxMin()), boxMax(sph_system.getBoxMax());

  for (int i = 0; i < numParticles; ++i) {
    auto id = static_cast<unsigned long>(i);
    autopas::sph::SPHParticle particle(randomPosition(boxMin, boxMax), {0., 0., 0.}, id, 0.75, 0.012, 0.);
    // autopas::sph::SPHParticle ith(randomPosition(boxMin, boxMax), {0, 0, 0},
    // i++, 0.75, 0.012, 0. );
    sph_system.addParticle(particle);
  }

  //	for (auto it = cont->begin(); it.isValid(); ++it) {
  //		it->print();
  //	}

  for (auto part = sph_system.begin(); part.isValid(); ++part) {
    part->setMass(part->getMass() * boxMax[0] * boxMax[1] * boxMax[2] / (double)(numParticles));
  }
  // std::cout << "# of ptcls is... " << numParticles << std::endl;

  // Set the end time
}

int main(int argc, char *argv[]) {
  autopas::Logger::create();
  std::array<double, 3> boxMin({0., 0., 0.}), boxMax{};
  boxMax[0] = 0.15;
  boxMax[1] = boxMax[2] = boxMax[0] / 1.0;
  double cutoff = .03;

  autopas::sph::SPHCalcDensityFunctor densfunc;

  autopas::sph::SPHCalcHydroForceFunctor hydrofunc;

  int numParticles = 16;
  int numIterations = 100000;
  int containerTypeInt = 0;
  enum ContainerType { linkedCells, directSum, verletLists } containerType = linkedCells;
  int functorTypeInt = 0;
  enum FunctorType { densityFunctor, hydroForceFunctor } functorType = densityFunctor;
  double skin = 0.;
  int rebuildFrequency = 10;
  bool useNewton3 = true;
  if (argc == 9) {
    numParticles = atoi(argv[1]);
    numIterations = atoi(argv[2]);
    containerTypeInt = atoi(argv[3]);
    functorTypeInt = atoi(argv[4]);
    skin = atof(argv[5]);
    rebuildFrequency = atof(argv[6]);
    useNewton3 = atoi(argv[7]);
    boxMax[0] = boxMax[1] = boxMax[2] = atof(argv[8]);
  } else if (argc == 8) {
    numParticles = atoi(argv[1]);
    numIterations = atoi(argv[2]);
    containerTypeInt = atoi(argv[3]);
    functorTypeInt = atoi(argv[4]);
    skin = atof(argv[5]);
    rebuildFrequency = atof(argv[6]);
    useNewton3 = atoi(argv[7]);
  } else if (argc == 7) {
    numParticles = atoi(argv[1]);
    numIterations = atoi(argv[2]);
    containerTypeInt = atoi(argv[3]);
    functorTypeInt = atoi(argv[4]);
    skin = atof(argv[5]);
    rebuildFrequency = atof(argv[6]);
  } else if (argc == 5) {
    numParticles = atoi(argv[1]);
    numIterations = atoi(argv[2]);
    containerTypeInt = atoi(argv[3]);
    functorTypeInt = atoi(argv[4]);
  } else if (argc == 4) {
    numParticles = atoi(argv[1]);
    numIterations = atoi(argv[2]);
    containerTypeInt = atoi(argv[3]);
  } else {
    std::cerr
        << "ERROR: wrong number of arguments given. " << std::endl
        << "sph-diagram-generation requires the following arguments:" << std::endl
        << "numParticles numIterations containerType [functorType [skin rebuildFrequency [useNewton3 [boxSize]]]]:"
        << std::endl
        << std::endl
        << "containerType should be either 0 (linked-cells), 1 (direct sum) or 2 (verlet lists)" << std::endl
        << "functorType should be either 0 (density functor) or 1 (hydro force functor)" << std::endl;
    exit(1);
  }

  if (containerTypeInt <= verletLists) {
    containerType = static_cast<ContainerType>(containerTypeInt);
  } else {
    std::cerr << "Error: wrong containerType " << containerTypeInt << std::endl
              << "containerType should be either 0 (linked-cells), 1 (direct sum) or 2 (verlet lists)" << std::endl;
    exit(2);
  }

  if (functorTypeInt <= hydroForceFunctor) {
    functorType = static_cast<FunctorType>(functorTypeInt);
  } else {
    std::cerr << "Error: wrong functorType " << functorTypeInt << std::endl
              << "functorType should be either 0 (density functor) or 1 (hydro force functor)" << std::endl;
    exit(2);
  }

  autopas::LinkedCells<autopas::sph::SPHParticle, autopas::FullParticleCell<autopas::sph::SPHParticle>> lcCont(
      boxMin, boxMax, cutoff);
  autopas::DirectSum<autopas::sph::SPHParticle, autopas::FullParticleCell<autopas::sph::SPHParticle>> dirCont(
      boxMin, boxMax, cutoff);
  autopas::VerletLists<autopas::sph::SPHParticle> verletCont(boxMin, boxMax, cutoff, skin * cutoff, rebuildFrequency);

  addParticles(lcCont, numParticles);

  for (auto it = lcCont.begin(); it.isValid(); ++it) {
    dirCont.addParticle(*it);
    verletCont.addParticle(*it);
  }

  if (containerType == linkedCells) {
    if (functorType == densityFunctor) {
      measureContainer(&lcCont, &densfunc, numParticles, numIterations, useNewton3);
    } else if (functorType == hydroForceFunctor) {
      measureContainer(&lcCont, &hydrofunc, numParticles, numIterations, useNewton3);
    } else {
      std::cout << "wrong functor given" << std::endl;
      exit(2);
    }
  } else if (containerType == directSum) {
    if (functorType == densityFunctor) {
      measureContainer(&dirCont, &densfunc, numParticles, numIterations, useNewton3);
    } else if (functorType == hydroForceFunctor) {
      measureContainer(&dirCont, &hydrofunc, numParticles, numIterations, useNewton3);
    } else {
      std::cout << "wrong functor given" << std::endl;
      exit(2);
    }
  } else if (containerType == verletLists) {
    if (functorType == densityFunctor) {
      measureContainer(&verletCont, &densfunc, numParticles, numIterations, useNewton3);
    } else if (functorType == hydroForceFunctor) {
      measureContainer(&verletCont, &hydrofunc, numParticles, numIterations, useNewton3);
    } else {
      std::cout << "wrong functor given" << std::endl;
      exit(2);
    }
  } else {
    std::cout << "invalid container id" << std::endl;
    exit(3);
  }
}

template <class Container, class Functor>
void measureContainer(Container *cont, Functor *func, int numParticles, int numIterations, bool useNewton3) {
  // autopas::FlopCounterFunctor<autopas::sph::SPHParticle, autopas::FullParticleCell<autopas::sph::SPHParticle>>
  //    flopFunctor(cont->getCutoff());

  autopas::utils::Timer t;

  // cont->iteratePairwiseAoS(&flopFunctor);
  // double flopsPerIteration = flopFunctor.getFlops(func.getNumFlopsPerKernelCall());

  autopas::C08Traversal<autopas::FullParticleCell<autopas::sph::SPHParticle>, Functor, false, false> dummyTraversal(
      {0, 0, 0}, func);
  if (cont->getContainerType() == ContainerOptions::linkedCells) {
    dummyTraversal = C08Traversal<FullParticleCell<autopas::sph::SPHParticle>, Functor, false, false>(
        dynamic_cast<LinkedCells<autopas::sph::SPHParticle, FullParticleCell<autopas::sph::SPHParticle>> *>(cont)
            ->getCellBlock()
            .getCellsPerDimensionWithHalo(),
        func);
  }

  t.start();
  for (int i = 0; i < numIterations; ++i) {
    cont->iteratePairwiseAoS(func, &dummyTraversal, useNewton3);
  }
  double elapsedTime = t.stop();

  // double flops = flopsPerIteration * numIterations;

  double MFUPS_aos = numParticles * numIterations / elapsedTime * 1e-6;

  t.start();
  for (int i = 0; i < numIterations; ++i) {
    cont->iteratePairwiseSoA(func, &dummyTraversal, useNewton3);
  }
  elapsedTime = t.stop();

  // double flops = flopsPerIteration * numIterations;

  double MFUPS_soa = numParticles * numIterations / elapsedTime * 1e-6;

  std::cout << numParticles << "\t" << numIterations << "\t" << MFUPS_aos << "\t" << MFUPS_soa;
  // std::cout << numParticles << "\t" << numIterations << "\t" << elapsedTime / numIterations << "\t" << MFUPS;
  // std::cout << "\t" << flops;
  // std::cout << "\t" << flopFunctor.getHitRate();
  // std::cout << "\t" << flops / elapsedTime * 1e-9 << std::endl;

  // std::cout << "measuring done" << std::endl;

  std::cout << std::endl;
}

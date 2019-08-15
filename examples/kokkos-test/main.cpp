/**
 *
 * @file main.cpp
 * @date 01.07.19
 * @author M. Geitner
 */


#include <chrono>
#include <fstream>
#include <iostream>
#include <autopas/utils/MemoryProfiler.h>
#include "autopas/AutoPas.h"
#include "../../tests/testAutopas/testingHelpers/GridGenerator.h"
#include "../../tests/testAutopas/testingHelpers/GaussianGenerator.h"
//#include "../../tests/testAutopas/testingHelpers/RandomGenerator.h"
#include "autopas/pairwiseFunctors/KokkosFlopCounterFunctor.h"

#include "MDFlexParser.h"

//@TODO build KokkosFlopCounter


using namespace std;
using namespace autopas;



/**
 * @brief Constructs a container and fills it with particles.
 *
 * According to the options passed, a %DirectSum or %'LinkedCells' container is
 * built. It consists of %`FullParticleCells` and is filled with
 * `PrintableMolecules`. The particles are aligned on a cuboid grid.
 *
 * @param autopas AutoPas object that should be initialized
 * @param particlesPerDim Number of desired particles per dimension.
 * @param particelSpacing Space between two particles along each axis of space.
 */
 template< class Particle>
void initContainerGrid(autopas::AutoPas<Particle, FullParticleCell<Particle>> &autopas,
                       size_t particlesPerDim, double particelSpacing) {
  std::array<double, 3> boxMin({0., 0., 0.});
  std::array<double, 3> boxMax(
          {(particlesPerDim)*particelSpacing, (particlesPerDim)*particelSpacing, (particlesPerDim)*particelSpacing});
  autopas.setBoxMin(boxMin);
  autopas.setBoxMax(boxMax);

  autopas.init();

  Particle dummyParticle;
  GridGenerator::fillWithParticles(autopas, {particlesPerDim, particlesPerDim, particlesPerDim}, dummyParticle,
                                   {particelSpacing, particelSpacing, particelSpacing},
                                   {particelSpacing / 2, particelSpacing / 2, particelSpacing / 2});
}

void initContainerGridKokkos(autopas::AutoPas<KokkosParticle, FullParticleCell<KokkosParticle>> &autopas,
                       size_t particlesPerDim, double particelSpacing) {
    std::array<double, 3> boxMin({0., 0., 0.});
    std::array<double, 3> boxMax(
            {(particlesPerDim)*particelSpacing, (particlesPerDim)*particelSpacing, (particlesPerDim)*particelSpacing});
    autopas.setBoxMin(boxMin);
    autopas.setBoxMax(boxMax);

    autopas.init();


    //GridGenerator method, without dummy particle
    std::array<size_t, 3> particlesPerDim2 = {particlesPerDim, particlesPerDim, particlesPerDim};
    std::array<double, 3> spacing2 = {particelSpacing, particelSpacing, particelSpacing};
    std::array<double, 3> offset2 = {particelSpacing / 2, particelSpacing / 2, particelSpacing / 2};

    size_t id = 0;
    for (unsigned int z = 0; z < particlesPerDim2[2]; ++z) {
        for (unsigned int y = 0; y < particlesPerDim2[1]; ++y) {
            for (unsigned int x = 0; x < particlesPerDim2[0]; ++x) {
                KokkosParticle p{};
                p.setR({x * spacing2[0] + offset2[0], y * spacing2[1] + offset2[1], z * spacing2[2] + offset2[2]});
                p.setID(id++);
                autopas.addParticle(p);
            }
        }
    }
}





/*
template <class ParticleBase>
void initContainerGauss(autopas::AutoPas<ParticleBase, FullParticleCell<ParticleBase>> &autopas,
                        double boxLength, size_t numParticles, double distributionMean, double distributionStdDev) {
  std::array<double, 3> boxMin({0., 0., 0.});
  std::array<double, 3> boxMax({boxLength, boxLength, boxLength});

  autopas.setBoxMin(boxMin);
  autopas.setBoxMax(boxMax);

  autopas.init();

  KokkosParticle dummyParticle;
  GaussianGenerator::fillWithParticles(autopas, numParticles, dummyParticle, distributionMean, distributionStdDev);
}
 */

/*
void initContainerUniform(autopas::AutoPas<KokkosParticle, FullParticleCell<KokkosParticle>> &autopas,
                          double boxLength, size_t numParticles) {
  std::array<double, 3> boxMin({0., 0., 0.});
  std::array<double, 3> boxMax({boxLength, boxLength, boxLength});

  autopas.setBoxMin(boxMin);
  autopas.setBoxMax(boxMax);

  autopas.init();

  KokkosParticle dummyParticle;
  RandomGenerator::fillWithParticles(autopas, dummyParticle, numParticles);
}
*/

/**
 * Writes a VTK file for the current state of the AutoPas object
 * @tparam AutoPasTemplate Template for the templetized autopas type.
 * @param filename
 * @param numParticles
 * @param autopas
 */
template <class AutoPasTemplate>
void writeVTKFile(string &filename, size_t numParticles, AutoPasTemplate &autopas) {
  std::ofstream vtkFile;
  vtkFile.open(filename);

  vtkFile << "# vtk DataFile Version 2.0" << endl;
  vtkFile << "Timestep" << endl;
  vtkFile << "ASCII" << endl;
  vtkFile << "DATASET STRUCTURED_GRID" << endl;
  vtkFile << "DIMENSIONS 1 1 1" << endl;
  vtkFile << "POINTS " << numParticles << " double" << endl;

  for (auto iter = autopas.begin(); iter.isValid(); ++iter) {
    auto pos = iter->getR();
    vtkFile << pos[0] << " " << pos[1] << " " << pos[2] << endl;
  }

  vtkFile.close();
}

/**
 * This function is needed to create functors with the actual type through templates.
 * @tparam FunctorChoice
 * @tparam AutoPasTemplate
 * @param autopas
 * @param cutoff
 * @param numIterations
 * @return Time for all calculation iterations in microseconds.
 */
template <class FunctorChoice, class AutoPasTemplate>
long calculate(AutoPasTemplate &autopas, double cutoff, double epsilon, double sigma, size_t numIterations) {
  auto functor = FunctorChoice(cutoff, epsilon, sigma, 0.0);

  std::chrono::high_resolution_clock::time_point startCalc, stopCalc;

  startCalc = std::chrono::high_resolution_clock::now();

  // actual Calculation
  for (unsigned int i = 0; i < numIterations; ++i) {
    if (autopas::Logger::get()->level() <= autopas::Logger::LogLevel::debug) {
      cout << "Iteration " << i << endl;
      cout << "Current Memory usage: " << autopas::memoryProfiler::currentMemoryUsage() << " kB" << endl;
    }
    autopas.iteratePairwise(&functor);
  }
  stopCalc = std::chrono::high_resolution_clock::now();

  auto durationCalc = std::chrono::duration_cast<std::chrono::microseconds>(stopCalc - startCalc).count();
  return durationCalc;
}



int main(int argc, char **argv){



  //parameters
  //double cutoff = 1.0;
  double sigma = 1.0;
  double epsilon = 1.0;
  //bool newton3 = false;

#ifdef AUTOPAS_KOKKOS
  //init
  //int argc1 = 1;
  //char **argx = new char*[1];
  //std::string s = "--kokkos-threads=8";
  //const char* s1 = s.c_str();
 // argx[0] = (char *) alloca(s.size() + 1);
  //memcpy(argx[0], s.c_str() , s.size()+ 1);

    Kokkos::InitArguments args;
    //args.num_threads = autopas::autopas_get_max_threads();
    Kokkos::initialize(args);


// Parsing
  MDFlexParser parser;
  if (not parser.parseInput(argc, argv)) {
    exit(-1);
  }

  auto boxLength(parser.getBoxLength());
  auto containerChoice(parser.getContainerOptions());
  auto selectorStrategy(parser.getSelectorStrategy());
  auto cutoff(parser.getCutoff());
  auto &cellSizeFactors(parser.getCellSizeFactors());
  auto dataLayoutOptions(parser.getDataLayoutOptions());
  auto distributionMean(parser.getDistributionMean());
  auto distributionStdDev(parser.getDistributionStdDev());
  //auto functorChoice(parser.getFunctorOption());

  auto generatorChoice(parser.getGeneratorOption());
  auto logLevel(parser.getLogLevel());
  string logFileName(parser.getLogFileName());
  auto measureFlops(parser.getMeasureFlops());//not used because FlopCounter cannot handle KokkosParticle

  auto newton3Options(parser.getNewton3Options());
  auto numIterations(parser.getIterations());
  auto particleSpacing(parser.getParticleSpacing());
  auto particlesPerDim(parser.getParticlesPerDim());
  auto particlesTotal(parser.getParticlesTotal());
  auto traversalOptions(parser.getTraversalOptions());
  auto tuningInterval(parser.getTuningInterval());
  auto tuningSamples(parser.getTuningSamples());
  auto tuningStrategy(parser.getTuningStrategyOption());
  auto verletRebuildFrequency(parser.getVerletRebuildFrequency());
  auto verletSkinRadius(parser.getVerletSkinRadius());
  auto vtkFilename(parser.getWriteVTK());
  parser.printConfig();

  std::chrono::high_resolution_clock::time_point startTotal, stopTotal;

  startTotal = std::chrono::high_resolution_clock::now();

  // select either std::out or a logfile for autopas log output.
  // This does not affect md-flex output.
  std::ofstream logFile;
  std::streambuf *streamBuf;
  if (logFileName.empty()) {
    streamBuf = std::cout.rdbuf();
  } else {
    logFile.open(logFileName);
    streamBuf = logFile.rdbuf();
  }
  std::ostream outputStream(streamBuf);
  // Initialization

  if(dataLayoutOptions.find(DataLayoutOption::aos)!= dataLayoutOptions.end()){
    //found element aos
      autopas::AutoPas<KokkosParticle, FullParticleCell<KokkosParticle>> autopas(outputStream);
      autopas::Logger::get()->set_level(logLevel);

      autopas.setCutoff(cutoff);
      autopas.setVerletSkin(verletSkinRadius);
      autopas.setVerletRebuildFrequency(verletRebuildFrequency);
      autopas.setTuningInterval(tuningInterval);
      autopas.setTuningStrategyOption(tuningStrategy);
      autopas.setNumSamples(tuningSamples);
      autopas.setSelectorStrategy(selectorStrategy);
      autopas.setAllowedContainers(containerChoice);//directsum, linkedcells
      autopas.setAllowedTraversals(traversalOptions);//c08 kokkos, directsum


      autopas.setAllowedDataLayouts(dataLayoutOptions);
      autopas.setAllowedNewton3Options(newton3Options);
      autopas.setAllowedCellSizeFactors(cellSizeFactors);

      switch (generatorChoice) {
          case MDFlexParser::GeneratorOption::grid: {
              initContainerGridKokkos(autopas, particlesPerDim, particleSpacing);
              particlesTotal = particlesPerDim * particlesPerDim * particlesPerDim;
              break;
          }
              /*case MDFlexParser::GeneratorOption::uniform: {
                initContainerUniform(autopas, boxLength, particlesTotal);
                break;
              }*/
          case MDFlexParser::GeneratorOption::gaussian: {
              //initContainerGauss(autopas, boxLength, particlesTotal, distributionMean, distributionStdDev);
              break;
          }
          default:
              std::cerr << "Unknown generator choice" << std::endl;
              return -1;
      }

      //KokkosParticle::setEpsilon(1.0);
      //KokkosParticle::setSigma(1.0);
      cout << endl;
      //cout << "epsilon: " << PrintableMolecule::getEpsilon() << endl;
      //cout << "sigma  : " << PrintableMolecule::getSigma() << endl << endl;

      if (not vtkFilename.empty()) writeVTKFile(vtkFilename, particlesTotal, autopas);

      // statistics for linked cells
      if (autopas.getContainer()->getContainerType() == autopas::ContainerOption::linkedCells) {
          auto lcContainer = dynamic_cast<autopas::LinkedCells<KokkosParticle, FullParticleCell<KokkosParticle>> *>(
                  autopas.getContainer());
          auto cellsPerDimHalo = lcContainer->getCellBlock().getCellsPerDimensionWithHalo();
          std::array<size_t, 3> cellsPerDim{cellsPerDimHalo[0] - 2, cellsPerDimHalo[1] - 2, cellsPerDimHalo[2] - 2};
          //    auto numCellsHalo = lcContainer->getCells().size();
          auto numCells = cellsPerDim[0] * cellsPerDim[1] * cellsPerDim[2];

          cout << "Cells per dimension with Halo: " << cellsPerDimHalo[0] << " x " << cellsPerDimHalo[1] << " x "
               << cellsPerDimHalo[2] << " (Total: " << numCells << ")" << endl;
          cout << "Average Particles per cell: " << (particlesTotal) / (double)numCells << endl;
          cout << endl;
      }

      cout << "Using " << autopas::autopas_get_max_threads() << " Threads" << endl;

      long durationApply = 0;
      //unsigned long flopsPerKernelCall = 0;
      cout << "Starting force calculation... " << endl;

      //use KokkosFunctor only
      durationApply =
              calculate<KokkosLJFunctor<KokkosParticle, FullParticleCell<KokkosParticle>>>(autopas, cutoff, epsilon, sigma, numIterations);
      //flopsPerKernelCall = KokkosLJFunctor<KokkosParticle, FullParticleCell<KokkosParticle>>::getNumFlopsPerKernelCall();



      stopTotal = std::chrono::high_resolution_clock::now();
      cout << "Force calculation done!" << endl;

      //  printMolecules(autopas);

      auto durationTotal = std::chrono::duration_cast<std::chrono::microseconds>(stopTotal - startTotal).count();
      auto durationTotalSec = durationTotal * 1e-6;
      auto durationApplySec = durationApply * 1e-6;

      // Statistics
      cout << fixed << setprecision(2);
      cout << endl << "Measurements:" << endl;
      cout << "Time total   : " << durationTotal << " \u03bcs (" << durationTotalSec << "s)" << endl;
      if (numIterations > 0) {
          cout << "One iteration: " << durationApply / numIterations << " \u03bcs (" << durationApplySec / numIterations
               << "s)" << endl;
      }
      auto mfups = particlesTotal * numIterations / durationApplySec * 1e-6;
      cout << "MFUPs/sec    : " << mfups << endl;
      /*
      if (measureFlops) {
        FlopCounterFunctor<KokkosParticle, FullParticleCell<KokkosParticle>> flopCounterFunctor(
                autopas.getContainer()->getCutoff());
        autopas.iteratePairwise(&flopCounterFunctor);

        auto flops = flopCounterFunctor.getFlops(flopsPerKernelCall) * numIterations;
        // approximation for flops of verlet list generation
        if (autopas.getContainer()->getContainerType() == autopas::ContainerOption::verletLists)
          flops +=
                  flopCounterFunctor.getDistanceCalculations() *
                  FlopCounterFunctor<KokkosParticle, FullParticleCell<KokkosParticle>>::numFlopsPerDistanceCalculation *
                  floor(numIterations / verletRebuildFrequency);

        cout << "GFLOPs       : " << flops * 1e-9 << endl;
        cout << "GFLOPs/sec   : " << flops * 1e-9 / durationApplySec << endl;
        cout << "Hit rate     : " << flopCounterFunctor.getHitRate() << endl;
      }
       */


      if (not logFileName.empty()) {
          logFile.close();
      }
  }else if(dataLayoutOptions.find(DataLayoutOption::kokkos)!= dataLayoutOptions.end()){
      //-------------------------------- soa -------------------------------------
      autopas::AutoPas<Particle, FullParticleCell<Particle>> autopas(outputStream);

      autopas::Logger::get()->set_level(logLevel);

      autopas.setCutoff(cutoff);
      autopas.setVerletSkin(verletSkinRadius);
      autopas.setVerletRebuildFrequency(verletRebuildFrequency);
      autopas.setTuningInterval(tuningInterval);
      autopas.setTuningStrategyOption(tuningStrategy);
      autopas.setNumSamples(tuningSamples);
      autopas.setSelectorStrategy(selectorStrategy);
      autopas.setAllowedContainers(containerChoice);//directsum, linkedcells
      autopas.setAllowedTraversals(traversalOptions);//c08 kokkos, directsum


      autopas.setAllowedDataLayouts(dataLayoutOptions);
      autopas.setAllowedNewton3Options(newton3Options);
      autopas.setAllowedCellSizeFactors(cellSizeFactors);

      switch (generatorChoice) {
          case MDFlexParser::GeneratorOption::grid: {
              initContainerGrid(autopas, particlesPerDim, particleSpacing);
              particlesTotal = particlesPerDim * particlesPerDim * particlesPerDim;
              break;
          }
              /*case MDFlexParser::GeneratorOption::uniform: {
                initContainerUniform(autopas, boxLength, particlesTotal);
                break;
              }*/
          case MDFlexParser::GeneratorOption::gaussian: {
              //initContainerGauss(autopas, boxLength, particlesTotal, distributionMean, distributionStdDev);
              break;
          }
          default:
              std::cerr << "Unknown generator choice" << std::endl;
              return -1;
      }

      //KokkosParticle::setEpsilon(1.0);
      //KokkosParticle::setSigma(1.0);
      cout << endl;
      //cout << "epsilon: " << PrintableMolecule::getEpsilon() << endl;
      //cout << "sigma  : " << PrintableMolecule::getSigma() << endl << endl;

      if (not vtkFilename.empty()) writeVTKFile(vtkFilename, particlesTotal, autopas);

      // statistics for linked cells
      if (autopas.getContainer()->getContainerType() == autopas::ContainerOption::linkedCells) {
          auto lcContainer = dynamic_cast<autopas::LinkedCells<Particle, FullParticleCell<Particle>> *>(
                  autopas.getContainer());
          auto cellsPerDimHalo = lcContainer->getCellBlock().getCellsPerDimensionWithHalo();
          std::array<size_t, 3> cellsPerDim{cellsPerDimHalo[0] - 2, cellsPerDimHalo[1] - 2, cellsPerDimHalo[2] - 2};
          //    auto numCellsHalo = lcContainer->getCells().size();
          auto numCells = cellsPerDim[0] * cellsPerDim[1] * cellsPerDim[2];

          cout << "Cells per dimension with Halo: " << cellsPerDimHalo[0] << " x " << cellsPerDimHalo[1] << " x "
               << cellsPerDimHalo[2] << " (Total: " << numCells << ")" << endl;
          cout << "Average Particles per cell: " << (particlesTotal) / (double)numCells << endl;
          cout << endl;
      }

      cout << "Using " << autopas::autopas_get_max_threads() << " Threads" << endl;

      long durationApply = 0;
      unsigned long flopsPerKernelCall = 0;
      cout << "Starting force calculation... " << endl;

      //use KokkosFunctor only
      durationApply =
              calculate<KokkosStructLJFunctor<Particle, FullParticleCell<Particle>>>(autopas, cutoff, epsilon, sigma, numIterations);
      flopsPerKernelCall =
              KokkosStructLJFunctor<Particle, FullParticleCell<Particle>>::getNumFlopsPerKernelCall();



      stopTotal = std::chrono::high_resolution_clock::now();
      cout << "Force calculation done!" << endl;

      //  printMolecules(autopas);

      auto durationTotal = std::chrono::duration_cast<std::chrono::microseconds>(stopTotal - startTotal).count();
      auto durationTotalSec = durationTotal * 1e-6;
      auto durationApplySec = durationApply * 1e-6;

      // Statistics
      cout << fixed << setprecision(2);
      cout << endl << "Measurements:" << endl;
      cout << "Time total   : " << durationTotal << " \u03bcs (" << durationTotalSec << "s)" << endl;
      if (numIterations > 0) {
          cout << "One iteration: " << durationApply / numIterations << " \u03bcs (" << durationApplySec / numIterations
               << "s)" << endl;
      }
      auto mfups = particlesTotal * numIterations / durationApplySec * 1e-6;
      cout << "MFUPs/sec    : " << mfups << endl;
/*
      if (measureFlops) {
        FlopCounterFunctor<Particle, FullParticleCell<Particle>> flopCounterFunctor(
                autopas.getContainer()->getCutoff());
        autopas.iteratePairwise(&flopCounterFunctor);

        auto flops = flopCounterFunctor.getFlops(flopsPerKernelCall) * numIterations;
        // approximation for flops of verlet list generation
        if (autopas.getContainer()->getContainerType() == autopas::ContainerOption::verletLists)
          flops +=
                  flopCounterFunctor.getDistanceCalculations() *
                  FlopCounterFunctor<Particle, FullParticleCell<Particle>>::numFlopsPerDistanceCalculation *
                  floor(numIterations / verletRebuildFrequency);

        cout << "GFLOPs       : " << flops * 1e-9 << endl;
        cout << "GFLOPs/sec   : " << flops * 1e-9 / durationApplySec << endl;
        cout << "Hit rate     : " << flopCounterFunctor.getHitRate() << endl;
      }
*/


      if (not logFileName.empty()) {
          logFile.close();
      }
  }







#endif

#ifdef AUTOPAS_KOKKOS
  //Kokkos::finalize();
#endif

  return EXIT_SUCCESS;
}
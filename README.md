# AutoPas
AutoPas is a node-level auto-tuned particle simulation library developed
in the context of the **TaLPas** project. [![Build Status](https://www5.in.tum.de/jenkins/mardyn/buildStatus/icon?job=AutoPas-Multibranch/master)](https://www5.in.tum.de/jenkins/mardyn/job/AutoPas-Multibranch/job/master/)

## Documentation
The documentation can be found at our website:
 <https://www5.in.tum.de/AutoPas/doxygen_doc/master/>

Alternatively you can build the documentation on your own:
* requirements: [Doxygen](http://www.doxygen.nl/)
* `make doc_doxygen`

## Requirements
* cmake 3.14 or newer
* make (build-essentials) or ninja
* a c++17 compiler (gcc7, clang8 and icpc 2019 are tested)

## Building AutoPas
build instructions for make:
```bash
mkdir build
cd build
cmake ..
make
```
if you want to use another compiler, specify it at the first cmake call, e.g.:
```bash
mkdir build
cd build
CC=clang CXX=clang++ cmake ..
make
```
if you would like to use ninja instead of make:
```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```
### Building AutoPas on a Cluster
HPC clusters often use module systems. CMake is sometimes not able to
correctly detect the compiler you wished to use. If a wrong compiler is
found please specify the compiler explicitly, e.g. for gcc:
```bash
mkdir build
cd build
CC=`which gcc` CXX=`which g++` cmake ..
make
```

## Testing
### Running Tests
to run tests:
```bash
make test
# or
ninja test
```
or using the ctest environment:
```bash
ctest
```
to get verbose output:
```bash
ctest --verbose
```
#### How to run specific tests

use the --gtest_filter variable:
```bash
./tests/testAutopas/runTests --gtest_filter=ArrayMathTest.testAdd*
```
or use the GTEST_FILTER environment variable:
```bash
GTEST_FILTER="ArrayMathTest.testAdd*" ctest --verbose
```
or `ctest` arguments like `-R` (run tests matching regex) and `-E` (exclude tests matching regex)
```bash
ctest -R 'Array.*testAdd' -E 'Double'
```

### Debugging Tests
Find out the command to start your desired test with `-N` aka. `--show-only`:
```bash
ctest -R 'Array.*testAdd' -N
```
Start the test with `gdb`
```bash
gdb --args ${TestCommand}
```

## Examples
As AutoPas is only a library for particle simulations it itself is not able to run simulations.
We have, however, included a variety of examples in the **examples** directory. The examples include:
* Molecular dynamics simulations with 1 centered Lennard-Jones particles.
* Smoothed particle hydrodynamics simulations
* Gravity simulations

## Using AutoPas
Steps to using AutoPas in your particle simulation program:

### Defining a Custom Particle Class
First you will need to define a particle class.
For that we provide some basic Particle classes defined
in `src/particles/` that you can use either directly
or you can write your own Particle class by inheriting from
one of the provided classes.
```C++
class SPHParticle : public autopas::Particle {

}
```

### Functors
Once you have defined your particle you can start with functors;
#### Definition
TODO
#### Usage
TODO

### Ownage
An AutoPas container normally saves two different types of particles:
* owned particles: Particles that belong to the AutoPas instance
* halo particles: Particles that do not belong to the current AutoPas instance. These can be ghost particles due to, e.g., periodic boundary conditions, or particles belonging to another neighboring AutoPas object (if you split the entire domain over multiple AutoPas objects). The halo particles are needed for the correct calculation of the pairwise forces. 

Note that not all owned particles necessarily have to lie within the boundaries of an AutoPas
object, see also section Simulation Loop.
 
### Iterating Through Particles
Iterators to iterate over particle are provided.
The particle can be accesses using `iter->` (`*iter` is also possible).
When created inside a OpenMP parallel region, work is automatically spread over all iterators.
```C++
#pragma omp parallel
for(auto iter = autoPas.begin(); iter.isValid(); ++iter) {
  // user code:
  auto position = iter->getR();
}
```
For convenience the `end()` method is also implemented for the AutoPas class.
```C++
#pragma omp parallel
for(auto iter = autoPas.begin(); iter != autoPas.end(); ++iter) {
  // user code:
  auto position = iter->getR();
}
```
You might also use range-based for loops:
```C++
#pragma omp parallel
for(auto& particle : autoPas) {
  // user code:
  auto position = particle.getR();
}
```

To iterate over a subset of particles, the `getRegionIterator(lowCorner, highCorner)`
method can be used:
```C++
#pragma omp parallel
for(auto iter = autoPas.getRegionIterator(lowCorner, highCorner); iter != autoPas.end(); ++iter) {
  // user code:
  auto position = iter->getR();
}
```

Both `begin()` and `getRegionIterator()` can also take the additional parameter `IteratorBehavior`,
which indicates over which particles the iteration should be performed.
```C++
enum IteratorBehavior {
  haloOnly,     /// iterate only over halo particles
  ownedOnly,    /// iterate only over owned particles
  haloAndOwned  /// iterate over both halo and owned particles
};
```
The default parameter is `haloAndOwned`, which is also used for range-based for loops.

Analogously to `begin()`, `cbegin()` is also defined, which guarantees to return a
`const_iterator`.

### Simulation Loop
One simulation loop should always consist of the following phases:

1. Updating the Container, which returns a vector of all invalid == leaving particles!
   ```C++
   auto [invalidParticles, updated] = autoPas.updateContainer();
   ```

2. Handling the leaving particles
   * Apply boundary conditions on them
   
   * Potentially send them to other mpi-processes, skip this if MPI is not needed
   
   * Add them to the containers using
      ```C++
      autoPas.addParticle(particle)
      ```

3. Handle halo particles:
   * Identify the halo particles by use of AutoPas' iterators and send them in a similar way as the leaving particles.

   * Add the particles as haloParticles using 
      ```C++
      autoPas.addOrUpdateHaloParticle(haloParticle)
      ```

4. Perform an iteratePairwise step.
   ```C++
   autoPas.iteratePairwise(functor);
   ```

In some iterations step 1. will return a pair of an empty list of invalid particles and false.
In this case the container was not rebuild to benefit of not rebuilding the containers and the associated neighbor lists.

### Using multiple functors

AutoPas is able to work with simulation setups using multiple functors that describe different forces.
A good demonstration for that is the sph example found under examples/sph or examples/sph-mpi.
There exist some things you have to be careful about when using multiple functors:
* If you use multiple functors it is necessary that all functors support the same newton3 options. If there is one functor not supporting newton3, you have to disable newton3 support for AutoPas by calling
  ```C++
  autoPas.setAllowedNewton3Options({false});
  ```

* If you have `n` functors within one iteration and update the particle position only at the end or start of the iteration, the rebuildFrequency and the samplingRate have to be a multiple of `n`.   

### Inserting additional particles
Before inserting additional particles (e.g. through a grand-canonical thermostat ), 
you always have to enforce a containerUpdate on ALL AutoPas instances, i.e., 
on all mpi processes, by calling  
```C++
autoPas.updateContainerForced();
``` 
This will invalidate the internal neighbor lists and containers.

## Developing AutoPas
Please look at our [contribution guidelines](https://github.com/AutoPas/AutoPas/blob/master/.github/CONTRIBUTING.md).

## Acknowledgements
* TaLPas BMBF

## Papers to cite
* TODO: Add papers

# MD-Flexible 

This demo shows how to easily create and simulate molecular dynamic
scenarios using AutoPas and flexibly configure all of it's options.

## Compiling
To build MD-Flexible execute the following from the AutoPas root folder:
```bash
mkdir build && $_
cmake ..
make md-flexible
```

## Usage 

When running md-flexible without any arguments a default simulation with
all AutoPas options active is run and it's configuration printed. From
there you can restrict any AutoPas options or change the simulation.

For all available option see:
```bash
 examples/md-flexible/md-flexible --help
```

### Input

MD-Flexible accepts input via command line arguments and YAML files.
When given both, any command line options will overwrite their YAML
counterparts.

The keywords for the YAML file are the same as for the command line
input. However, since there are options that can only be defined
through the YAML file there is also the file `input/ALLOptions.yaml`
to be used as a reference.

### Output

* After every execution, a configuration YAML file is generated. It is
possible to use this file as input for a new simulation.
* You can generate vtk output by providing a vtk-filename
(see help for details).

### Checkpoints

MD-Flexible can be initialized through a previously written VTK file.
Please use only VTK files written by MD-Flexible since the parsing is
rather strict. The VTK file only contains Information about all
particles positions, velocities, forces and typeIDs. All other options,
especially the simulation box size and particle properties (still) need
to be set through a YAML file.

# Counter-Example Guided Knowledge Compilation for Boolean Functional Synthesis

This is the source code of a tool based on work reported in the paper "Counterexample Guided Knowledge Compilation for Boolean Functional Synthesis" by S. Akshay, Supratik Chakraborty and Sahil Jain, to appear in Proceedings of the 35th International Conference on Computer Aided Verification, 2023.


## Documentation
- [Counter-Example Guided Knowledge Compilation for Boolean Functional Synthesis](#counter-example-guided-knowledge-compilation-for-boolean-functional-synthesis)
  - [Documentation](#documentation)
  - [Structure](#structure)
  - [Setup](#setup)
    - [Locally](#locally)
    - [Using docker](#using-docker)
  - [Usage](#usage)
    - [Locally](#locally-1)
    - [Using docker](#using-docker-1)
  - [Source code](#source-code)
  - [Benchmarks](#benchmarks)
  - [Scripts](#scripts)
    - [Running the experiments](#running-the-experiments)
    - [Analysing the outputs](#analysing-the-outputs)
      - [Beyond Manthan analysis](#beyond-manthan-analysis)
  - [Resource requirements](#resource-requirements)
  - [Replicating the results](#replicating-the-results)

## Structure
This repository consists of primarily the [source code](#source-code) of the tool, the patched library [abc](dependencies/abc), [benchmarks](#benchmarks) and [scripts](#scripts) to run the tool & analyse the results.

Code from [bfss](https://github.com/BooleanFunctionalSynthesis/bfss), under license GPLv2.0, has been re-used in the creation of this tool. A patched version of [abc](https://github.com/jsahil730/abc) has also been employed and the same resides at [dependencies/abc](dependencies/abc). 


## Setup

### Locally

To setup the tool locally, it is recommended to use [setup.sh](setup.sh) as simply `./script.sh` for setting up all the required dependencies. `sudo` access to the system is required as this script installs several libraries such as `readline`, `boost-program-options` and `termcap`. This also compiles and sets up `abc`.

The source code can then simply be compiled by using `make [BUILD=RELEASE/TEST/DEBUG]`. For performing experiments & benchmarking, it is recommended to build the tool with `BUILD=RELEASE`.

### Using docker

The tool can be directly built as a docker image using the provided [Dockerfile](Dockerfile) using the following command :

```
sudo docker build -t cex_kc:latest .
``` 
This prepares the docker image but the tool itself is only built when running the container. 

> A few warning messages in red may flash by during the docker build process.  These are due to some legacy code in the [abc](https://github.com/jsahil730/abc) library and can be safely ignored.

> Note that building this docker image for the first time can take around 20 to 30 minutes. Rebuilding the docker image should usually not take more than 10 to 15 minutes.
## Usage

### Locally

The built binary(tool) when compiled locally, resides at `./bin/main`. To use the tool, a benchmark, a preliminary ordering and an output folder are required to be specified as follows -
```
bin/main -b examples/example2.v -v examples/OrderFiles/example2_varstoelim.txt --out examples/ 
```

Details about other options can be checked out by using the `-h` help option. The benchmark can be provided in either verilog or aig format. Qdimacs benchmarks need to be converted to verilogs using [readCnf](src/readCnf.cpp).

On invoking the tool as above, logging information is printed to the standard output and results regarding unates, final ordering, and final verilogs are respectively dumped in sub-folders `Unates`, `OrderFiles` and `Verilogs` inside the specified output folder. These sub-folders have to be created before invoking the tool.

### Using docker

Once the docker image has been built as mentioned above, the container can be simply run as :
```
sudo docker run cex_kc:latest [single_test_benchmarks/small_test_benchmarks/test_benchmarks/all_benchmarks] [retain]
```
It is to be noted that the tool itself is compiled when the container is run, and not during building the image. Compilation logs are therefore printed during the same before the experiments begin.

By default, the experiments are run with `small_test_benchmarks`; and to run it with a different file, the filename needs to be provided with the `docker run` command, for example, 
```
sudo docker run cex_kc:latest single_test_benchmarks
```
Few such files containing different set of benchmarks have been provided, the details of which can be found in the [Benchmarks](#benchmarks) section.

Furthermore, by default, the docker container exits immediately after the experiments finish execution, printing the statistics of the result in the format of the table given in Fig. 4 of the paper. To retain the container and be able to inspect the generated logfiles and other output files, an additional option to retain the container must be provided at the end of the docker run command, for example, 
```
sudo docker run cex_kc:latest single_test_benchmarks retain
```
needs to be run to retain the container. In this case, the container must be exited by closing the shell directly.

For the docker run; every configuration - `DO`, `SO`, `CDO` and `CSO` have been assigned distinct output directories inside the `results` directory for the ease of distinguishment. Each of these directories further contain `OrderFiles`, `Unates`, and `Verilogs` sub-directories containing the final generated results. The logfiles, however, continue to be stored under the `outputs` directory directly under `results`. Every run corresponds to a distinct logfile, the path to which is known when performing the run.

The exact sequence of steps to inspect the files is as follows -
```
sudo docker run cex_kc:latest single_test_benchmarks retain

## wait for the experiments & analyses to be complete

## then, run from another shell
sudo docker exec -it --workdir "/home/user/results/" <container_id OR container_name> /bin/bash
$ ls

```

For replicating the results reported in the paper, please skip to [Replicating the results](#replicating-the-results) section.


## Source code

The source code is written completely in C++ and is mainly divided across two files - [main.cpp](src/main.cpp) and [helper.cpp](src/helper.cpp) - with the `main` function being a part of the first one. 

Few of the interesting functions in the file include - [`getConflictFormulaCNF`](src/helper.cpp#L3138) & [`getConflictFormulaCNF2`](src/helper.cpp#L3211) functions, both of which are used to obtain conflict formulae for a given index; [`repair`](src/helper.cpp#L2778) function which performs rectification of the input `Aig` based on a single counter-example; and [`Rectify`](src/helper.cpp#L2434) & [`Rectify3`](src/helper.cpp#L2642) functions where the actual rectification takes place.


## Benchmarks

The benchmarks used for experimentation are the same as those used in [bfss](https://github.com/BooleanFunctionalSynthesis/bfss/tree/master/benchmarks).  These benchmarks can be found in the [benchmarks](benchmarks) directory.  The documentation for these benchmarks, as available in [bfss](https://github.com/BooleanFunctionalSynthesis/bfss/tree/master/benchmarks) has also been replicated here for convenience.
There are 602 such benchmarks on which the experiments have been performed.  

Multiple files containing benchmark paths have been provided for running the complete set of experiments :
- [all_benchmarks](all_benchmarks) contains a list of all 602 benchmarks.
- [test_benchmarks](test_benchmarks) contains a list of some 304 benchmarks which were completely solved within half an hour under a specific choice of parameters and on hardware configuration described in the paper.
- [small_test_benchmarks](small_test_benchmarks) contains a small subset of 13 benchmarks from [test_benchmarks](test_benchmarks) which can be used for verifying the results reasonable quickly.
- [single_test_benchmarks](single_test_benchmarks) contains a single benchmark for running a smoke test to confirm whether anything is broken since this benchmark gets solved within a few seconds.

One can also add new benchmarks by creating them in aiger or verilog format and adding them to the benchmarks folder.  One can also add/delete/modify entries in the four given files (all_benchmarks/test_benchmarks/small_test_benchmarks/single_test_benchmarks) for purposes of experimentation. However, in these cases the docker image has to be rebuilt using 
```
sudo docker build -t cex_kc:latest .
```
before running the tool.


## Scripts

The code has also been provided with few scripts to [run](run_expts.py) it on several benchmarks at once as well as perform post-run [analysis](analysis.py). The complete set of scripts can be invoked in sequence by directly running the [test.sh](test.sh). This would create the required directories for dumping the results; run the experiments as well as perform the final analysis on the same.

### Running the experiments

`run_expts.py` requires a file to read benchmark paths as input. Other options are detailed about in the usage help. 

```
python3 run_expts.py small_test_benchmarks -resultdir "results"
```

This script runs all the benchmarks specified in the file with each possible configuration, based on the options, and the results are eventally reported in `runs.csv` and `results.json` files, both present within the results directory specified. The results directory also includes other files output by the tool as well as an `outputs` directory containing the logfiles for each run of the tool, all named with a different hash.

Hashed directories are also created in the same results folder for each possible configuration, where data regarding benchmarks being part of various benchmark types such as `allUnates`(all variables were unates) or `noConflicts`(no conflicts were existent) etc is stored.

> Note that currently all files output by the tool are overwritten across configurations, thus rendering them meaningless when running multiple configurations together. Thus, it is advised to run each configuration separately in case these files are to be utilized.

### Analysing the outputs

`analysis.py` requires the `json` file emitted by running the experiments and performs various different analyses.

```
python3 analysis_py results/results.json
```

Note that some analyses might be cross-configurations while others might not be. It is therefore advised to appropriately comment or uncomment the analyses required to be run.

#### Beyond Manthan analysis

The textfile [beyond-manthan.txt](beyond-manthan.txt) jots down all the benchmarks which could not be solved by [Manthan](https://github.com/meelgroup/manthan) within a two hour time limit as reported in the paper "Engineering an Efficient Boolean Functional Synthesis Engine by P. Golia, F. Slivovsky, S. Roy and K.S. Meel, ICCAD 2021". We use this list to compute how many benchmarks of the current list of benchmarks could not be solved by Manthan in 2 hours, but could be solved by the current run of our tool.


## Resource requirements

It is possible to effortlessly run the experiments on any personal laptop or desktop as well. Any such device with atleast 4 GB RAM, single core, 1.5 or so GHz cpu frequency and atleast 10 GB of free hard disk space should be able to replicate even the complete results sans the time taken. 

## Replicating the results 

To replicate the results of the paper, depending on whether the experiments are to be run on all of the benchmarks, or just a small subset of them, the `docker run` command needs to be specified accordingly. 

To run the experiments on all benchmarks and replicate results of the paper, the following command can be used -
```
sudo docker run cex_kc:latest all_benchmarks
```

> Please note! On the specified resource requirements, this run might potentially take several days to be completed. 

To run the experiments on just a subset of the benchmarks, the following command can be used -
```
sudo docker run cex_kc:latest small_test_benchmarks
```

> This run should take atmost a couple of hours on the resource requirements stated above.

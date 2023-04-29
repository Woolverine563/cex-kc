# Counter-Example Guided Knowledge Compilation for Boolean Functional Synthesis

This is the source code of a tool based on work reported in the paper "Counterexample Guided Knowledge Compilation for Boolean Functional Synthesis" by S. Akshay, Supratik Chakraborty and Sahil Jain, to appear in Proceedings of the 35th International Conference on Computer Aided Verification, 2023.


## Documentation
- [Counter-Example Guided Knowledge Compilation for Boolean Functional Synthesis](#counter-example-guided-knowledge-compilation-for-boolean-functional-synthesis)
  - [Documentation](#documentation)
  - [Structure](#structure)
  - [Setup](#setup)
    - [Using docker](#using-docker)
    - [Locally](#locally)
  - [Usage](#usage)
    - [Using docker](#using-docker-1)
    - [Locally](#locally-1)
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

### Using docker

The tool can be directly built as a docker image using the provided [Dockerfile](Dockerfile) using the following command :

```
sudo docker build -t cex_kc:latest .
``` 
This prepares the docker image but the tool is actually only built when running the container. 

> Note that building this docker image for the first time can take around 20 to 30 minutes. Rebuilding the docker image should usually not take more than 10 to 15 minutes.

### Locally

If it is required to run the tool locally, it is recommended to use [setup.sh](setup.sh) for setting up [abc](dependencies/abc) appropriately. Additional libraries such as `readline`, `boost-program-options`, `termcap` etc also need to be installed in this case; which are usually either available or installable in any standard linux distribution.

Also it is required to install `matplotlib` and `scipy` python3 packages for being able to use the [scripts](#scripts) & run the complete experiments on multiple benchmarks. 

The source code can be compiled simply by using `make [BUILD=RELEASE/TEST/DEBUG]` after setting up abc.


## Usage

### Using docker

Once the docker image has been built as mentioned above, the container can be simply run as :
```
sudo docker run cex_kc:latest [single_test_benchmarks/small_test_benchmarks/test_benchmarks/all_benchmarks] [retain]
```

By default, the experiments are run with `small_test_benchmarks`; and to run it with a different file, the filename needs to be provided with the `docker run` command, ie, `sudo docker run cex_kc:latest single_test_benchmarks`.
Few such files containing different set of benchmarks have been provided, the details of which can be found in the [Benchmarks](#benchmarks) section.
Furthermore, by default, the docker container quits immediately after the experiments finish execution. To retain the container and be able to inspect the generated results, logfiles and outputs; the filename as well as a second option to retain has to be provided, ie, `sudo docker run cex_kc:latest single_test_benchmarks retain`.

### Locally

The built binary(tool) when compiled locally, resides at `./bin/main` and requires specifying a benchmark along with a preliminary ordering to use and an output folder. Details about other options can be checked out by using the `-h` help option. The benchmark can be provided in either verilog or aig format. Qdimacs benchmarks would need to be converted to verilogs using [readCnf](src/readCnf.cpp).

```
bin/main -b examples/example2.v -v examples/OrderFiles/example2_varstoelim.txt --out examples/ 
```

On invoking the tool as above, logging information is printed to the standard output and data regarding unates, final ordering etc are dumped in their respective directories inside the specified output folder. Subfolders within the output folder under the name of `OrderFiles`, `Unates`, `Verilogs` and `UnatesOnly` have to be created before invoking the tool. 



For replicating the results reported in the paper, please skip to [Replicating the results](#replicating-the-results) section.

## Source code

The source code is written completely in C++ and is mainly divided across two files - [main.cpp](src/main.cpp) and [helper.cpp](src/helper.cpp) - with the `main` function being a part of the first one. 

Few of the interesting functions in the file include - [`getConflictFormulaCNF`](src/helper.cpp#L3138) & [`getConflictFormulaCNF2`](src/helper.cpp#L3211) functions, both of which are used to obtain conflict formulae for a given index; [`repair`](src/helper.cpp#L2778) function which performs rectification of the input `Aig` based on a single counter-example; and [`Rectify`](src/helper.cpp#L2434) & [`Rectify3`](src/helper.cpp#L2642) functions where the actual rectification takes place.


## Benchmarks

The benchmarks used for experimentation have been borrowed from [bfss](https://github.com/BooleanFunctionalSynthesis/bfss/tree/master/benchmarks) as well; and can be found in the [benchmarks](benchmarks) directory.
There are total 602 such benchmarks on which the experiments have been performed.  

Multiple files containing benchmark paths have been provided for running the complete set of experiments :
- [all_benchmarks](all_benchmarks) contains a list of all 602 benchmarks.
- [test_benchmarks](test_benchmarks) contains a list of some 304 benchmarks which were completely solved within half an hour under a specific choice of parameters and on hardware configuration described in the paper.
- [small_test_benchmarks](small_test_benchmarks) contains a random subset of 20 benchmarks from [test_benchmarks](test_benchmarks) which can be used for verifying the results.
- [single_test_benchmarks](single_test_benchmarks) contains a single benchmark path for running a smoke test to confirm whether anything is broken since this benchmark gets solved within a few seconds.

One can also add new benchmarks by creating them in aiger or verilog format and adding them to this benchmarks folder, but note that in this case the docker image will have to be rebuilt before running the tool.


## Scripts

The code has also been provided with few scripts to [run](run_expts.py) it on several benchmarks at once as well as perform post-run [analysis](analysis.py). The complete set of scripts can be invoked in sequence by directly running the [test.sh](test.sh). This would create the required directories for dumping the results; run the experiments as well as perform the final analysis on the same.

### Running the experiments

`run_expts.py` requires a file to read benchmark paths as input. Other options are detailed about in the usage help. 

```
python3 run_expts.py small_test_benchmarks -resultdir "results"
```

This script runs all the benchmarks specified in the file with each possible configuration, based on the options, and the results are eventally reported in `runs.csv` and `results.json` files, both present within the results directory specified. The results directory also includes other files outputted by the tool as well as an `outputs` directory containing the logfiles for each run of the tool, all named with a different hash.

Hashed directories are also created in the same results folder for each possible configuration, where data regarding benchmarks being part of various benchmark types such as `allUnates`(all variables were unates) or `noConflicts`(no conflicts were existent) etc is stored.

> Note that currently all files outputted by the tool are overwritten across configurations, thus rendering them meaningless when running multiple configurations together. Thus, it is advised to run each configuration separately in case these files are to be utilized.

### Analysing the outputs

`analysis.py` requires the `json` file emitted by running the experiments and performs various different analyses.

```
python3 analysis_py results/results.json
```

Note that some analyses might be cross-configurations while others might not be. It is therefore advised to appropriately comment or uncomment the analyses required to be run.

#### Beyond Manthan analysis

The textfile [beyond-manthan.txt](beyond-manthan.txt) jots down all the benchmarks which could not be solved by [Manthan](https://github.com/meelgroup/manthan) within the specified one hour time limit. Beyond Manthan analysis compares the solved benchmarks of any run with this textfile and reports all those which have been solved by the tool.


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

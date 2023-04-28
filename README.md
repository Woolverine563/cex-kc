# Counter-Example Guided Knowledge Compilation for Boolean Functional Synthesis


## Setup

This tool has 2 major dependencies : [ABC](https://github.com/jsahil730/abc) and [BFSS](https://github.com/BooleanFunctionalSynthesis/bfss).

The code from BFSS has been heavily re-used in the creation of this tool. A patched version of ABC has been employed and the same resides at [dependencies](dependencies). The tool can be directly built as a docker image using the provided [Dockerfile](Dockerfile) using the following command `docker build -t cex_kc:latest .` In case it is necessary to run the tool locally, I recommend using [setup.sh](setup.sh) for setting up [ABC](dependencies/abc) appropriately.

## Usage

Once the docker image has been built as mentioned above, the container can be simply run as `docker run cex_kc:latest` and it should start running the tool on the benchmark file specified in [test.sh](test.sh). 
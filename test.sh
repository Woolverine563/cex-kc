#!/bin/bash
folder="."
mkdir -p "${folder}/results/outputs/"

for f in "DO" "SO" "CDO" "CSO"; do
    mkdir -p "${folder}/results/${f}/Unates/"
    mkdir -p "${folder}/results/${f}/UnatesOnly/"
    mkdir -p "${folder}/results/${f}/Unates/"
    mkdir -p "${folder}/results/${f}/UnatesOnly/"
    mkdir -p "${folder}/results/${f}/OrderFiles/"
    mkdir -p "${folder}/results/${f}/Verilogs/"
done

benchmarks=${1:-'small_test_benchmarks'}
python3 run_expts.py $benchmarks -nocompile -noparallel -resultdir "${folder}/results" -timeout 3600 -unatetimeout 1000 -conflict '1,2' -dynamic '0,1' 2>&1
python3 analysis.py "${folder}/results/results.json"
if [[ $2 = "retain" ]]
then
    echo "All experiments & analyses have now finished, the container will not exit unless the shell is killed/closed directly."
    tail -f /dev/null # to ensure the docker container is not exited
fi
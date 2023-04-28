#!/bin/bash
folder=`git rev-parse --short=6 HEAD`
mkdir -p "${folder}/results/outputs/"
mkdir -p "${folder}/results/Unates/"
mkdir -p "${folder}/results/UnatesOnly/"
mkdir -p "${folder}/results/OrderFiles/"
mkdir -p "${folder}/results/Verilogs/"
mkdir -p "${folder}/analysis/"
benchmarks="small_test_benchmarks"
python3 run_expts.py $benchmarks -resultdir "${folder}/results" -timeout 3600 -unatetimeout 3600 2>&1 > $folder/output
python3 analysis.py "${folder}/results/results.json"

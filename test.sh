#!/bin/sh

cd /users/ug18/sahiljain/cex-kc
folder=`git rev-parse --short=6 HEAD`
mkdir -p "${folder}/results/outputs/"
mkdir -p "${folder}/results/Unates"
mkdir -p "${folder}/results/OrderFiles"
mkdir -p "${folder}/results/Verilogs"
mkdir -p "${folder}/analysis/"
python3 run_expts.py run_expts_test_benchmarks -timeout 9 -nocompile 2>&1 > $folder/output
python3 analysis.py "${folder}/results/results.json"

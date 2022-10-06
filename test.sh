folder=`git rev-parse --short=6 HEAD`
mkdir -p "${folder}/results/outputs/"
mkdir -p "${folder}/analysis/"
python3 run_expts.py run_expts_test_benchmarks -timeout 9 -analyse 2>&1 > $folder/output
python3 analysis.py "${folder}/results/results.json"
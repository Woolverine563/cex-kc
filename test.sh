folder=`git rev-parse --short=6 HEAD`
mkdir -p $folder
nohup python3 run_expts.py test_benchmarks -timeout 900 -analyse 2>&1 > $folder/output &
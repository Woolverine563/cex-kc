import argparse, json
from subprocess import run, check_output, STDOUT
from multiprocessing import Pool
from csv import writer
from itertools import product
from hashlib import md5
import os
from util import *

def run_code(boolargs: list, valargs: list, analyse: bool, analysisdir: str):
    args = ["bin/main"]
    D = dict(zip(NON_BOOL_FIELDS, valargs))

    for (k,v) in zip(BOOL_FIELDS, boolargs):
        val = "" if not v else k
        args.append(val)
        D[k] = val

    for k,v in zip(NON_BOOL_FIELDS, valargs):
        args.append(k)
        args.append(str(v))

    args.extend(["--out", arguments.outdir])

    bname = D[BNAME_FIELD]
    # default ordering is being used

    hash = md5(' '.join(args).encode()).hexdigest()
    D[HASH] = hash

    try:
        oup = check_output(args, stderr=STDOUT, timeout=D[TIMEOUT_FIELD] + 300) # 5 min extra timeout!
        # Hard timeout of atmost twice

        os.makedirs(f'{arguments.outdir}/outputs', exist_ok=True)
        with open(f'{arguments.outdir}/outputs/output-{hash}.txt','wb') as _:
            _.write(oup)

        if (analyse):
            os.makedirs(f'{analysisdir}', exist_ok=True)
            gmon = f'{analysisdir}/gmon.{hash}.out'
            run(["mv","gmon.out",gmon])
            with open(f'{analysisdir}/analysis-{hash}.txt','w') as _:
                run(["gprof","bin/main",gmon],stdout=_)

        lines = oup.splitlines()[-LINES_COUNT:]

        initial = lines[0].split(b':')[1].strip().split()[0].decode()
        final = lines[1].split(b':')[1].strip().split()[0].decode()
        init_u = lines[2].split(b':')[1].strip().split()[0].decode()
        tot_u = lines[2].split(b':')[1].strip().split()[4].decode()
        phases = lines[2].split(b':')[1].strip().split()[-2].decode()
        iters = lines[3].split()[1].decode()
        counterexs = lines[3].split(b':')[1].strip().split()[0].decode()
        idx = lines[3].split(b', ')[1].strip().split()[0].decode()
        numY = lines[3].split()[-2].decode()
        time = lines[4].strip().split()[0].decode()

        return D, [initial, final, init_u, tot_u, phases, iters, counterexs, idx, numY, time] + [x.decode() for x in lines[5].strip().split()] + [""], False
        # empty error field

    except Exception as e:
        # assumption that mostly no failures        
        print(e, args)
        return D, [""] * (len(RESULTS)-1) + [str(e)], True

parser = argparse.ArgumentParser()

commit = check_output(["git","rev-parse","--short=6","HEAD"]).strip().decode()

parser.add_argument("-timeout", type=lambda s: [int(item) for item in s.split(',')], default=[3600])
parser.add_argument("-unatetimeout", type=lambda s: [int(item) for item in s.split(',')], default=[])
parser.add_argument("-analyse", action='store_true')
parser.add_argument("file", type=str)
parser.add_argument("-outdir", type=str, default=f'{commit}/results')
parser.add_argument("-analysisdir", type=str, default=f'{commit}/analysis')
parser.add_argument("-nocompile", action='store_true')
arguments = parser.parse_args()

os.makedirs(arguments.outdir, exist_ok=True)

# TODO : we dump all code across config in same place, overwriting other config final results
CSV = open(f'{arguments.outdir}/runs.csv', 'w')
wr = writer(CSV)
wr.writerow(HEADER)
        
if (not arguments.nocompile):
    os.system("make clean")
    if arguments.analyse:
        os.system("make")
    else: 
        os.system("make BUILD=RELEASE")

if arguments.analyse:
    os.makedirs(arguments.analysisdir, exist_ok=True)
    for file in os.listdir(arguments.analysisdir):
        os.remove(f"{arguments.analysisdir}/{file}")

depth = [20]
rectify = [3]
conflict = [2]
unate = [True]
shannon = [False]
dynamic = [False, True]
fastcnf = [False]
timeout = arguments.timeout
unatetimeout = arguments.timeout if (len(arguments.unatetimeout) == 0) else arguments.unatetimeout
benchmarks = []

with open(arguments.file, 'r') as runs:
    l = runs.readlines()
    for i in range(len(l)):
        b_name = l[i].strip()

        benchmarks.append(b_name)

runs = []
json_res = []

v = list(product(rectify, depth)) # + [("1", "0")]

for name, c, (r, d), u, s, dyn, fc, t, ut in product(benchmarks, conflict, v, unate, shannon, dynamic, fastcnf, timeout, unatetimeout):
        
    path, file = name.rsplit('/', 1)
    order = f"{path}/OrderFiles/{file.rsplit('.', 1)[0]}_varstoelim.txt"

    bool_arg = [u,s,dyn,fc]
    arg = [name, order, c, r, d, t, ut]
    runs.append((bool_arg, arg, arguments.analyse, arguments.analysisdir))

pool = Pool() # processes=(os.cpu_count()*3)//4) # 2 cores free, should be fine

pool = pool.starmap_async(run_code, runs)
pool.wait()
res = pool.get()

for (D, outputs, error) in res:
    row, bname, hash, d, d_all = process(D, outputs, error)
    wr.writerow(row)
    json_res.append(d_all)
    
CSV.close()

with open(f'{arguments.outdir}/results.json','w') as file:
    json.dump(json_res, file)
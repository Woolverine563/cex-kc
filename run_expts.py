import argparse, json
from subprocess import run, check_output, STDOUT
from multiprocessing import Pool
from csv import writer, DictReader
from itertools import product
from hashlib import md5
from more_itertools import collapse
import os

def run_code(kwargs: dict, analyse: bool, analysisdir: str):
    args = ["bin/main"]

    if kwargs.pop('-u', False):
        args.append("-u")
    else:
        args.append("")

    if kwargs.pop('-s', False):
        args.append("-s")
    else:
        args.append("")

    if kwargs.pop('-o', False):
        args.append("-o")
    else:
        args.append("")

    for k,v in kwargs.items():
        args.append(k)
        args.append(str(v))

    bname = kwargs["-b"]

    try:
        oup = check_output(args, stderr=STDOUT)
        hash = md5(' '.join(args).encode()).hexdigest()

        print(oup.decode())

        if (analyse):
            with open(f'{analysisdir}/analysis-{hash}.txt','w') as _:
                run(["gprof","bin/main","gmon.out"],stdout=_)

        lines = oup.splitlines()[-6:]

        initial = lines[0].split(b':')[1].strip().split()[0].decode()
        final = lines[1].split(b':')[1].strip().split()[0].decode()
        init_u = lines[2].split(b':')[1].strip().split()[0].decode()
        tot_u = lines[2].split(b':')[1].strip().split()[-1].decode()
        iters = lines[3].split()[1].decode()
        counterexs = lines[3].split(b':')[1].strip().split()[0].decode()
        idx = lines[3].split(b', ')[1].strip().split()[0].decode()
        numY = lines[3].split()[-2].decode()
        time = lines[4].strip().split()[0].decode()

        return [bname, initial, final, init_u, tot_u, iters, counterexs, idx, numY, time] + [x.decode() for x in lines[5].strip().split()] + [hash] + args[1:]

    except Exception as e:
        print(e, args)
        return [bname]

parser = argparse.ArgumentParser()

parser.add_argument("-timeout", type=int, default=3600)
parser.add_argument("-analyse", action='store_true')
parser.add_argument("file", type=str)
parser.add_argument("-outdir", type=str, default='results')
parser.add_argument("-analysisdir", type=str, default='analysis')
parser.add_argument("-nocompile", action='store_true')
arguments = parser.parse_args()

os.makedirs(arguments.outdir, exist_ok=True)

f = open(f'{arguments.outdir}/runs.csv', 'w')
f2 = open(f'{arguments.outdir}/allUnates', 'w')
f3 = open(f'{arguments.outdir}/noConflictsWithUnates', 'w')
f4 = open(f'{arguments.outdir}/noConflicts', 'w')
f5 = open(f'{arguments.outdir}/noUnates', 'w')
f6 = open(f'{arguments.outdir}/others', 'w')
f7 = open(f'{arguments.outdir}/error', 'w')

wr = writer(f)

fields = ["Benchmark", "Initial size", "Final size", "Initial unates", "Final unates", "Number of iterations", "Number of cex", "Outputs fixed", "Total outputs", "Time taken", "repairTime", "conflictCnfTime", "satSolvingTime", "unateTime", "compressTime", "rectifyCnfTime", "rectifyUnsatCoreTime"]

# wr.writerow(["Benchmark", "Input NNF size", "Time", "SDD size"])
wr.writerow(fields)
        
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
unate = [True, False]
shannon = [False]
dynamic = [False]
benchmarks = []

with open(arguments.file, 'r') as f:
    l = f.readlines()
    for i in range(len(l)):
        b_name = l[i].strip()

        benchmarks.append(b_name)

runs = []
json_res = []

v = list(product(rectify, depth)) # + [("1", "0")]

for name, c, (r, d), u, s, dyn in product(benchmarks, conflict, v, unate, shannon, dynamic):
        
    path, file = name.rsplit('/', 1)
    order = f"{path}/OrderFiles/{file.rsplit('.', 1)[0]}_varstoelim.txt"

    arg = {"-b": name, "-v": order, "-c": c, "-r": r, "-d": d, "-t": arguments.timeout, "-u": u, "-s": s, "-o": dyn, "--unateTimeout": arguments.timeout}
    runs.append((arg, arguments.analyse, arguments.analysisdir))

pool = Pool(processes=(os.cpu_count()*3)//4) # 2 cores free, should be fine

pool = pool.starmap_async(run_code, runs)
pool.wait()
res = pool.get()

for row in res:
    if (len(row) > 1): # no error
        bname = row[0]
        wr.writerow(row)

        if (int(row[8]) == 0):
            f2.write(bname+"\n")
        elif (int(row[7]) == int(row[8])) and (int(row[6]) == 0):
            if ("-u" in list(collapse(row))):
                f3.write(bname+"\n")
            else:
                f4.write(bname+"\n")
        elif (int(row[4]) == 0) and ("-u" in row):
            f5.write(bname+"\n")
        else:
            f6.write(bname+"\n")
    else:
        f7.write(row[0]+"\n")

    json_res.append(dict(zip(fields, row)))
    

f.close()
f2.close()
f3.close()
f4.close()
f5.close()
f6.close()
f7.close()

with open(f'{arguments.outdir}/results.json','w') as file:
    json.dump(json_res, file)
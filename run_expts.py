import argparse
from subprocess import run, check_output
from multiprocessing import Pool
from csv import writer
from itertools import product
from hashlib import md5
from more_itertools import collapse
import os

parser = argparse.ArgumentParser()

parser.add_argument("-timeout", type=int, default=3600)
parser.add_argument("-unate", action='store_true')
parser.add_argument("-shannon", action='store_true')
parser.add_argument("-analyse", action='store_true')
parser.add_argument("file", type=str)
parser.add_argument("-outdir", type=str, default='results')
parser.add_argument("-analysisdir", type=str, default='analysis')
arguments = parser.parse_args()


def run_code(kwargs: dict):
    args = ["bin/main"]
    for k,v in kwargs.items():
        args.append(k)
        args.append(v)

    if (arguments.unate):
        args.append("-u")
    
    if (arguments.shannon):
        args.append("-s")

    try:
        oup = check_output(args)
        hash = md5(' '.join(args).encode()).hexdigest()

        print(oup.decode())

        if (arguments.analyse):
            with open(f'{arguments.analysisdir}/analysis-{hash}.txt','w') as _:
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

        return [args[2], initial, final, init_u, tot_u, iters, counterexs, idx, numY, time] + [x.decode() for x in lines[5].strip().split()] + [hash] + args[3:]

    except Exception as e:
        print(args, e)
        return [args[2]]

benchmarks = []

with open(arguments.file, 'r') as f:
    l = f.readlines()
    for i in range(len(l)):
        b_name = l[i].strip()

        path, file = b_name.rsplit('/', 1)
        
        b_order = f"{path}/OrderFiles/{file.rsplit('.', 1)[0]}_varstoelim.txt"
        # b_order = l[2*i+1].strip()
        benchmarks.append((b_name, b_order))

os.system("make clean")
if arguments.analyse:
    os.system("make")
else: 
    os.system("make BUILD=RELEASE")

d = {}

depth = ["20"]
rectify = ["3"]
conflict = ["2"]


if arguments.analyse:
    os.makedirs(arguments.analysisdir, exist_ok=True)
    for file in os.listdir(arguments.analysisdir):
        os.remove(f"{arguments.analysisdir}/{file}")

os.makedirs(arguments.outdir, exist_ok=True)

f = open(f'{arguments.outdir}/runs.csv', 'w')
f2 = open(f'{arguments.outdir}/allUnates', 'w')
f3 = open(f'{arguments.outdir}/noConflictsWithUnates', 'w')
f4 = open(f'{arguments.outdir}/noConflicts', 'w')
f5 = open(f'{arguments.outdir}/noUnates', 'w')
f6 = open(f'{arguments.outdir}/others', 'w')
f7 = open(f'{arguments.outdir}/error', 'w')

wr = writer(f)

# wr.writerow(["Benchmark", "Input NNF size", "Time", "SDD size"])
wr.writerow(["Benchmark", "Initial size", "Final size", "Initial unates", "Final unates", "Number of iterations", "Number of cex", "Outputs fixed", "Total outputs", "Time taken", "repairTime", "conflictCnfTime", "satSolvingTime", "unateTime"])

runs = []

v = list(product(rectify, depth)) # + [("1", "0")]

for (name, order), c, (r, d) in product(benchmarks, conflict, v):
        d2 = {"-b": name, "-v": order, "-c": c, "-r": r, "-d": d, "-t": str(arguments.timeout)}
        runs.append(d2)

# for (name, order) in benchmarks:
#     d2 = {"-b":name, "-v": order}
#     runs.append(d2)

pool = Pool(processes=os.cpu_count()-1)

pool = pool.map_async(run_code, runs)
pool.wait()
res = pool.get()

params = {}

benchmark_names = [x[0] for x in benchmarks]

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
        

f.close()
f2.close()
f3.close()
f4.close()
f5.close()
f6.close()
f7.close()
from subprocess import run, check_output
from multiprocessing import Pool
from csv import writer
from itertools import product
from hashlib import md5
from more_itertools import collapse
import os

TIMEOUT = 1800

def run_code(kwargs: dict):
    args = ["bin/main"]
    for k,v in kwargs.items():
        args.append(k)
        args.append(v)

    args.append("-u")

    try:
        # check_output(args)

        # b_name = args[2] + ".nnf"

        # oup = check_output(["python3", "../nnf2sdd/compiler.py", b_name], timeout=1800)

        # lines = oup.splitlines()

        # return [b_name, lines[0].strip().split()[1][:-1].decode(), f"{float(lines[2].strip().decode()):.4f}", lines[3].strip().split(b':')[1].strip().decode()]
        
        oup = check_output(args)
        hash = md5(' '.join(args).encode()).hexdigest()

        print(oup.decode())

        run(["gprof","bin/main","gmon.out"],stdout=open(f'analysis/analysis-{hash}.txt','w'))

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

        return [args[2], initial, final, init_u, tot_u, iters, counterexs, idx, numY, time] + lines[5].strip().split() + [args[3:], hash]

    except Exception as e:
        print(e)
        return []

benchmarks = []

with open('benchmarks_to_run', 'r') as f:
    l = f.readlines()
    for i in range(len(l)):
        b_name = l[i].strip()

        path, file = b_name.rsplit('/', 1)
        
        b_order = f"{path}/OrderFiles/{file.rsplit('.', 1)[0]}_varstoelim.txt"
        # b_order = l[2*i+1].strip()
        benchmarks.append((b_name, b_order))

# system("make clean")
# system("make BUILD=RELEASE")

d = {}

depth = ["20"]
rectify = ["3"]
conflict = ["2"]


os.makedirs('analysis', exist_ok=True)
os.makedirs('results', exist_ok=True)

for file in os.listdir('analysis'):
    os.remove(f"analysis/{file}")

f = open('results/runs.csv', 'w')
f2 = open('results/allUnates', 'w')
f3 = open('results/noConflictsWithUnates', 'w')
f4 = open('results/noConflicts', 'w')
f5 = open('results/noUnates', 'w')
f6 = open('results/others', 'w')

wr = writer(f)

# wr.writerow(["Benchmark", "Input NNF size", "Time", "SDD size"])
wr.writerow(["Benchmark", "Initial size", "Final size", "Initial unates", "Final unates", "Number of iterations", "Number of cex", "Outputs fixed", "Total outputs", "Time taken", "repairTime", "conflictCnfTime", "satSolvingTime", "unateTime"])

runs = []

v = list(product(rectify, depth)) # + [("1", "0")]

for (name, order), c, (r, d) in product(benchmarks, conflict, v):
        d2 = {"-b": name, "-v": order, "-c": c, "-r": r, "-d": d, "-t": str(TIMEOUT)}
        runs.append(d2)

# for (name, order) in benchmarks:
#     d2 = {"-b":name, "-v": order}
#     runs.append(d2)

pool = Pool()

pool = pool.map_async(run_code, runs)
pool.wait()
res = pool.get()

params = {}

benchmark_names = [x[0] for x in benchmarks]

for row in res:
    if (len(row) > 0):
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


        f.flush()
        f2.flush()
        f3.flush()
        f4.flush()
        f5.flush()
        f6.flush()
        

f.close()
f2.close()
f3.close()
f4.close()
f5.close()
f6.close()
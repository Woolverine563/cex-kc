from subprocess import run, check_output
from multiprocessing import Pool
from os import system
from csv import writer
from itertools import product

# TIMEOUT = 100000
# CTR = 0

def run_code(kwargs: dict):
    args = ["bin/main"]
    for k,v in kwargs.items():
        args.append(k)
        args.append(v)

    try:
        oup = check_output(args)
        
        lines = oup.splitlines()[-4:]

        initial = lines[0].split(b':')[1].strip().split()[0].decode()
        final = lines[1].split(b':')[1].strip().split()[0].decode()
        iters = lines[2].split()[1].decode()
        counterexs = lines[2].split(b':')[1].strip().split()[0].decode()
        idx = lines[2].split(b', ')[1].strip().split()[0].decode()
        numY = lines[2].split()[-2].decode()
        time = lines[3].strip().split()[0].decode()

        return [' '.join([str(x) for x in args[::2]])] + [initial, final, iters, counterexs, idx, numY, time]
    except Exception as e:
        print(e)
        return []

benchmarks = []

with open('benchmarks_to_run', 'r') as f:
    l = f.readlines()
    for i in range(len(l)//2):
        b_name = l[2*i].strip()
        b_order = l[2*i+1].strip()
        benchmarks.append((b_name, b_order))

system("make clean")
system("make BUILD=RELEASE")

d = {}

depth = ["0", "10", "20"]
rectify = ["1", "2", "3"]
conflict = ["1", "2"]


f = open('runs.csv', 'w')
wr = writer(f)
wr.writerow(["Arguments", "Initial size", "Final size", "Number of iterations", "Number of cex", "Outputs fixed", "Total outputs", "Time taken"])

runs = []

for (name, order), c, r, d in product(benchmarks, conflict, rectify, depth):
        d2 = {"-b": name, "-v": order, "-c": c, "-r": r, "-d": d}
        runs.append(d2)

pool = Pool()

v = pool.map_async(run_code, runs)
v.wait()
res = v.get()

for row in res:
    if (len(row) > 0):
        wr.writerow(row)
        f.flush()

f.close()
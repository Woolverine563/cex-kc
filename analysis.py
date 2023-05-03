from collections import defaultdict
import json, sys
from statistics import mean
import os
import copy
from typing import Any, Dict, List
from scipy.stats import gmean
from util import *
import itertools
import matplotlib.pyplot as plt
from subprocess import STDOUT, check_output, run

MAN_TIMEOUT = 3600 + 100 # slack
PLOT_CNT = 0

def print_aligned(field: str, value: str):
    print(f'{field:<40} -> {value:>5}')

def separateConfigs(results: List[Result], filterFields: Dict[str, Any] = {}):
    data: Dict[Config, List[Result]] = defaultdict(list)

    for r in results:
        config, res = r.extract(CONFIG_FIELDS)
        data[config].append(res)

    for k in list(data.keys()):
        if (not k.match(filterFields)):
            data.pop(k)
     
    return data

def compareFastCNFTime(results):
    # for each pair, either fully solved or partially solved is possible
    # for now, we can work merely with fully solved
    # create map of common config -> fast/slow
    assert len(results) > 0
    # keys = [BNAME_FIELD, VORDER_FIELD] + results[0].keys()

    d = {}
    for r in results:
        d[r.config] = r

    mapping = {}

    for k in d.keys():
        if (k.__dict__[FASTCNF_FIELD] == FASTCNF_FIELD):
            dc = copy.deepcopy(k.__dict__)
            dc.pop(FASTCNF_FIELD)
            d1 = copy.deepcopy(k.__dict__)
            d2 = copy.deepcopy(k.__dict__)
            d2[FASTCNF_FIELD] = EMPTY

            mapping[Config(dc)] = {True: d[Config(d1)], False: d[Config(d2)]}

    plot = {}
    
    for k, v in mapping.items():
        # print(v)
        if v[True].isSolved and v[False].isSolved :
            plot[k] = float(v[True].results[TOT_TIME]) / float(v[False].results[TOT_TIME])

    print(list(plot.values()))

def genericTotalAnalysis(results: List[Result]):
    # total results for every possible config
    data = separateConfigs(results)

    # total benchmarks solved
    total = set()
    for k, v in data.items():
        s = set([(r.benchmark, r.orderfile) for r in filter(lambda x : x.isSolved, v)])
        total.update(s)

    print(f"{'Total benchmarks solved across all configurations':<50} -> {len(total)} out of {max([len(v) for v in data.values()])}")

    err_results = list(filter(lambda x : x.results["ERROR"] != EMPTY, itertools.chain(*data.values())))

    errors = [(x.benchmark, x.orderfile) for x in err_results]
    timeouts = [(x.benchmark, x.orderfile) for x in filter(lambda x : 'timed out' in x.results["ERROR"], err_results)]

    other_errs = [x.results["ERROR"] for x in filter(lambda x : 'timed out' not in x.results["ERROR"], err_results)]

    # print(f"{'Errored':<50} -> {len(errors)} errors across {len(set(errors))} unique benchmarks")
    print(f"{'Hard Timeouts (process was killed)':<50} -> {len(timeouts)} timeouts across {len(set(timeouts))} unique benchmarks")
    # print(f"{'Other errors':<50} -> {other_errs}")
    print()
    print()

def genericAnalysis(c: Config, results: List[Result], file: str):

    cnt = len(list(filter(lambda x : x.isSolved, results)))
    print_aligned('Fully Solved', f'{cnt}')

    par2 = 0
    avg = 0
    for s in results:
        if s.isSolved:
            par2 += float(s.results[TOT_TIME])
            avg += float(s.results[TOT_TIME])
        else:
            par2 += c.__dict__[TIMEOUT_FIELD] * 2

    par2 /= len(results)
    avg /= len(list(filter(lambda s: s.isSolved, results)))
    print_aligned('PAR2 Score', f'{par2:.2f}')
    print_aligned("Avg Solved Runtime",f"{avg:.2f}")

    d = defaultdict(set)

    for r in results:
        analysDict = r.analyse()
        for x,y in analysDict.items():
            d[x].update(set([r] if y else []))
    
    assert len(set(d.keys()).difference(set(FILENAMES))) == 0

    folder = f'{file.rsplit("/", 1)[0]}/{c.hash()}'
    os.makedirs(folder, exist_ok=True)

    print()
    for k1 in ORDER_TYPES:
        v1 = d[k1]
        print_aligned(k1, f'{len(v1)}')
        with open(f'{folder}/{k1}', 'w') as f:
            f.writelines([f'{x.benchmark}\n' for x in v1])

    print_aligned(f"{FIXEDCONF} \u22c2 {SOMEU}", f"{len(d[FIXEDCONF].intersection(d[SOMEU]))}")        
    print_aligned(f"{NOCONF} \u22c2 {SOMEU}", f"{len(d[NOCONF].intersection(d[SOMEU]))}")
    print_aligned(f"{FIXEDCONF} \u22c2 {NOU}", f"{len(d[FIXEDCONF].intersection(d[NOU]))}")
    print_aligned(f"{NOCONF} \u22c2 {NOU}", f"{len(d[NOCONF].intersection(d[NOU]))}")
    print()

def beyondManthan(c: Config, results: List[Result]):
    with open('beyond-manthan.txt') as f:
        bmarksBeyond = f.read().strip().split()

    res = {True:[], False:[]}
    for b in bmarksBeyond:
        for r in results:
            if (b in r.benchmark) and r.isSolved:
                res[r.isAllU()].append(b)

    # we are running with single config for now...

    print(f'Number of benchmarks solved which were not solved by Manthan -> {len(res[True]) + len(res[False])}')
    print()

def ratioOutputsSolved(c: Config, results: List[Result]):
    global PLOT_CNT

    values = defaultdict(list)
    for res in results:
        if (res.error != EMPTY):
            continue
        tot_oups = int(res.results[TOT_OUTPUTS])
        fixed_oups = int(res.results[FIXED_OUTPUTS])
        
        if tot_oups == 0:
            continue
        values[res.isSolved].append((res.benchmark, fixed_oups/tot_oups, fixed_oups, tot_oups, int(res.results[INIT_UN]), int(res.results[FIN_UN])))

    bar1 = values[False]

    if (len(bar1) == 0):
        return
    bar1.sort(key=lambda x: x[1])
    xvalues = [x[0] for x in bar1]
    y1 = [x[1] for x in bar1]

    print(f'Outputs fixed v/s Total outputs :')
    print_aligned(f'Mean ratio (w 0s)', f'{mean(y1):.2f}')
    print_aligned(f'Mean ratio (wo 0s)', f'{mean([_ for _ in y1 if _ != 0]):.2f}')
    print_aligned(f'Geometric mean ratio', f'{gmean([_ for _ in y1 if _ != 0]):.2f}')
    print()
    
    almostSolved = list(filter(lambda t: 0.9 <= t[1] <= 1.0, bar1))
    print(f"Almost solved {len(almostSolved)} benchmarks -> \n")
    for x in almostSolved:
        print_aligned(f'Benchmark', f'{x[0]}')
        print_aligned(f'Ratio', f'{x[2]}/{x[3]} = {x[1]:.2f}')
        print_aligned(f'Initial Unates', f'{x[4]}')
        print_aligned(f'Final Unates', f'{x[5]}')
        print()

    plt.figure()
    plt.bar(xvalues, y1, color='blue', label='Outputs fixed / Total outputs', width=1.0)
    # plt.bar(xvalues, y2, color='green', label='% total unates', bottom=y1, width=1.0)
    plt.xticks([])
    plt.ylabel('Ratio')
    plt.xlabel('Unsolved benchmarks')
    plt.legend(loc='upper left')
    plt.autoscale(enable=True, axis='both', tight=True)
    plt.tight_layout()
    plt.savefig(f'graph_{PLOT_CNT}_ratio_outputs.png')
    PLOT_CNT += 1

def unatePostProcessing(c: Config, results: List[Result], file: str, force_run: bool = False):
    global PLOT_CNT

    if not c.bool_field(UNATE_FIELD):
        return

    run(["make", "postprocess"])

    folder = getattr(c, OUTFOLDR_FIELD) if (hasattr(c, OUTFOLDR_FIELD)) else f"{file.rsplit('/', 1)[0]}/"

    values = []
    for res in results:
        if (res.error != EMPTY):
            continue
        unatesPref = f"{folder}/Unates/{res.benchmark.split('/')[-1].rsplit('.', 1)[0]}"
        pUnatesF, nUnatesF = unatesPref + '.pUnates', unatesPref + '.nUnates'

        if (not os.path.exists(pUnatesF)) or (not os.path.exists(nUnatesF)):
            continue

        unatesOnlyPref = f"{folder}/UnatesOnly/{res.benchmark.split('/')[-1].rsplit('.', 1)[0]}"
        pUOnlyF, nUOnlyF = unatesOnlyPref + '.pUnates', unatesOnlyPref + '.nUnates'

        if (force_run) or (not os.path.exists(pUOnlyF)) or (not os.path.exists(nUOnlyF)):
            oup = check_output(["bin/postprocess", "-b", res.benchmark, "-p", f"{unatesPref}.pUnates", "-n", f"{unatesPref}.nUnates", "--out", folder])

            pu, nu, puOnly, nuOnly = map(int, oup.decode().split())
        else:
            def line_count(file):
                with open(file) as f:
                    return len(list(filter(lambda x: x != '', f.readlines())))

            [pu, nu, puOnly, nuOnly] = map(line_count, [pUnatesF, nUnatesF, pUOnlyF, nUOnlyF])

        if (pu+nu == 0):
            continue
        values.append((res, (puOnly+nuOnly)/(pu+nu), pu+nu, puOnly+nuOnly))

    if (len(values) == 0):
        return

    values.sort(key=lambda x: x[1])

    yvalues = [x[1] for x in values]
    # print(describe(yvalues))
    print('Unates Postprocessing Ratio -> ')
    for t in values:
        if (t[1] > 0):
            print(f'{t[0].benchmark} : {t[3]} / {t[2]} = {t[1]:.2f}')

    plt.figure()
    plt.bar(range(len(yvalues)), yvalues, label='Unate outputs w unique soln / Total unate outputs', width=1.0)
    plt.xticks([])
    plt.ylabel('Ratio')
    plt.xlabel('Benchmarks')
    plt.legend(loc='upper left')
    plt.autoscale(enable=True, axis='both', tight=True)
    plt.tight_layout()
    plt.savefig(f'graph_{PLOT_CNT}_ratio_unates.png')
    PLOT_CNT += 1

    
def scatterPlot(resultsv1: List[Result], resultsv2: List[Result], name = ''):
    d = defaultdict(dict)
    for r in resultsv1:
        if r.error == EMPTY:
            d[r.benchmark]["1"] = int(r.results[NUM_IT])
    
    for r in resultsv2:
        if r.error == EMPTY:
            d[r.benchmark]["2"] = int(r.results[NUM_IT])

    lv1, lv2 = [], []
    b = []        

    for k,v in d.items():
        if (len(v) == 2):
            lv1.append(v["1"])
            lv2.append(v["2"])
            b.append(k)

    # print(list(filter(lambda t: t[1] < t[2], zip(b, lv1, lv2))))

    plt.figure()
    plt.scatter(lv1, lv2)
    plt.axis('square')
    plt.show()    

if __name__ == "__main__":    

    allResults = []

    for file in sys.argv[1:]:
        with open(file, 'r') as f:
            data = json.load(f)

        results = [Result(x) for x in data]
        allResults.extend(results)
        print('='*120)
        print(f"Results JSON : {file}")
        print('='*120)
        print('\n')

        genericTotalAnalysis(results)
        resDict = separateConfigs(results)

        for c, res in resDict.items():
            print('-'*120)
            # print(f"{c} -> {c.hash()}")
            if c.match({DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 1}) :
                print("Default Configuration + Unate ON + Dynamic Ordering ON + Conflict Optimization OFF - DO")
            elif c.match({DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 1}) :
                print("Default Configuration + Unate ON + Dynamic Ordering OFF + Conflict Optimization OFF - SO")    
            elif c.match({DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 2}) :
                print("Default Configuration + Unate ON + Dynamic Ordering ON + Conflict Optimization ON - CDO")
            elif c.match({DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 2}):
                print("Default Configuration + Unate ON + Dynamic Ordering OFF + Conflict Optimization ON - CSO")
            else:
                # TODO : Address this more modularly for other possible config tweaks
                print("Non-default Configuration")
            print('-'*120)
            genericAnalysis(c, res, file)
            beyondManthan(c, res)
            # ratioOutputsSolved(c, res)
            # unatePostProcessing(c, res, file)

    # scatter plot b/w conflict v2 and conflict v1
    # DOv1 = separateConfigs(allResults, {DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 1}).popitem()[1]
    # DOv2 = separateConfigs(allResults, {DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 2}).popitem()[1]
    # NoDOv1 = separateConfigs(allResults, {DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 1}).popitem()[1]
    # NoDOv2 = separateConfigs(allResults, {DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 2}).popitem()[1]

    # scatterPlot(DOv1, DOv2, name = 'DO')
    # scatterPlot(NoDOv1, NoDOv2, name = 'NoDO')
    


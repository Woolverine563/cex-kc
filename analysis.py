from ast import Str
from collections import defaultdict
from hashlib import md5
import json, sys
from statistics import mean
import os
import copy
from typing import Any, Dict, List
from scipy.stats import describe, gmean
from util import *
import itertools
import matplotlib.pyplot as plt
from subprocess import STDOUT, check_output, run

MAN_TIMEOUT = 3600 + 100 # slack
PLOT_CNT = 0

class Config:
    def __init__(self, config: Dict[str, str]):
        self.__dict__.update(config)

    def bool_field(self, field: str) -> bool:
        assert field in BOOL_FIELDS
        return (getattr(self, field) == field)

    def keys(self):
        return self.__dict__.keys()

    def match(self, fields: Dict[str, Any]) -> bool:
        for f, v in fields.items():
            ans = self.__dict__.get(f, None)
            if (ans is None) or (ans != v):
                return False
        return True

    def __eq__(self, __o):
        return self.__dict__ == __o.__dict__

    def __str__(self):
        return f"Config({self.__dict__})"

    def __repr__(self) -> str:
        return self.__str__()

    def __hash__(self):
        return str(self).__hash__()

    def hash(self):
        return md5(str(self).encode()).hexdigest()

class Result:
    def __init__(self, data: dict):
        self.benchmark: str = data[BNAME]
        self.orderfile: str = data[VORDER]
        self.hash: str = data.pop(HASH)
        self.config = Config(data["config"])

        self.isSolved: bool = data["isSolved"]
        self.results: Dict[str, Any] = data["results"]
        self.files: Dict[str, Any] = data["files"]

        self.error: str = data["results"][ERR]

    def keys(self):
        return list(self.config.keys())

    def matches(self, config):
        return config == self.config

    def extract(self, fields):
        d1, d2 = {}, copy.deepcopy(self)

        for f in fields:
            d1[f] = d2.config.__dict__.pop(f)

        return Config(d1), d2

    def __str__(self) -> str:
        return f"Result([{'V' if self.isSolved else 'X'}] ({self.benchmark},{self.orderfile}) : {self.config} -> {self.results})"

    def __repr__(self) -> str:
        return self.__str__()

    def updateTimeout(self, timeout=None):
        if (timeout is not None) and self.isSolved and (float(self.results[TOT_TIME]) > timeout):
            self.isSolved = False

    def isAllU(self):
        return self.isSolved and (int(self.results[TOT_OUTPUTS]) == 0)
    def isNoConf(self):
        return self.isSolved and (not self.isAllU()) and (int(self.results[NUM_CEX]) == 0)
    def isNoU(self):
        return self.isSolved and (int(self.results[FIN_UN]) == 0)   
    def isFixedConf(self):
        return self.isSolved and (not self.isNoConf()) and (not self.isAllU())
    def isSomeU(self):
        return self.isSolved and (not self.isAllU()) and (not self.isNoU())

    def analyse(self) -> Dict[str, bool]:
        return {ALLUNATES: self.isAllU(), NOCONF: self.isNoConf(), NOU: self.isNoU(), FIXEDCONF: self.isFixedConf() , SOMEU: self.isSomeU()}

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

    print(f"Total benchmarks solved across any config: {len(total)} out of {max([len(v) for v in data.values()])}")

    err_results = list(filter(lambda x : x.results["ERROR"] != EMPTY, itertools.chain(*data.values())))

    errors = [(x.benchmark, x.orderfile) for x in err_results]
    timeouts = [(x.benchmark, x.orderfile) for x in filter(lambda x : 'timed out' in x.results["ERROR"], err_results)]

    other_errs = [x.results["ERROR"] for x in filter(lambda x : 'timed out' not in x.results["ERROR"], err_results)]

    print(f"Errored : {len(errors)} errors across {len(set(errors))} unique benchmarks")
    print(f"Hard Timeouts : {len(timeouts)} timeouts across {len(set(timeouts))} unique benchmarks out of above")
    print(f"Other errors : {other_errs}")
    print()
    print()

def genericAnalysis(c: Config, results: List[Result], file: str):

    cnt = len(list(filter(lambda x : x.isSolved, results)))
    print(f"Fully Solved : {cnt}")

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
    print(f"PAR2 Score : {par2:.2f}")
    print(f"Avg Solved Runtime : {avg:.2f}")

    d = defaultdict(set)

    for r in results:
        analysDict = r.analyse()
        for x,y in analysDict.items():
            if (y):
                d[x].add(r)
        # for f, _ in r.files.items():
        #     d[f] = d.get(f, set())
        #     d[f].add(r)
    
    assert len(set(d.keys()).difference(set(FILENAMES))) == 0

    folder = f'{file.rsplit("/", 1)[0]}/{c.hash()}'
    os.makedirs(folder, exist_ok=True)

    print("[")
    for k1, v1 in sorted(d.items()):
        print(f"\t{k1} -> {len(v1)}")
        with open(f'{folder}/{k1}', 'w') as f:
            f.writelines([f'{x.benchmark}\n' for x in v1])

    print(f"\t{FIXEDCONF} \u22c2 {SOMEU} -> {len(d[FIXEDCONF].intersection(d[SOMEU]))}")        
    print(f"\t{NOCONF} \u22c2 {SOMEU} -> {len(d[NOCONF].intersection(d[SOMEU]))}")
    print(f"\t{FIXEDCONF} \u22c2 {NOU} -> {len(d[FIXEDCONF].intersection(d[NOU]))}")
    print(f"\t{NOCONF} \u22c2 {NOU} -> {len(d[NOCONF].intersection(d[NOU]))}")
    print("]")
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

    print(f'Beyond Manthan (not allUnates): {len(list(res[False]))} benchmarks')
    for b in res[False]:
        print(b)
    print()

    print(f'Beyond Manthan (allUnates): {len(list(res[True]))} benchmarks')
    for b in res[True]:
        print(b)
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

    print(f'Outputs fixed v/s Total outputs ->')
    print(f'Mean ratio (w 0s) : {mean(y1):.2f}')
    print(f'Mean ratio (wo 0s) : {mean([_ for _ in y1 if _ != 0]):.2f}')
    print(f'Geometric mean ratio : {gmean([_ for _ in y1 if _ != 0]):.2f}')
    print()
    
    almostSolved = list(filter(lambda t: 0.9 <= t[1] <= 1.0, bar1))
    print(f"Almost solved {len(almostSolved)} benchmarks -> \n")
    for x in almostSolved:
        print(f'Benchmark : {x[0]}')
        print(f'Ratio : {x[2]} / {x[3]} = {x[1]:.2f}')
        print(f'Unates : {x[4]} init, {x[5]} fin')
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
        print('='*100)
        print(f"{file}")
        print('='*100)
        print('\n')

        genericTotalAnalysis(results)
        resDict = separateConfigs(results)

        for c, res in resDict.items():
            print('-'*100)
            print(f"{c} -> {c.hash()}")
            print('-'*100)
            genericAnalysis(c, res, file)
            beyondManthan(c, res)
            ratioOutputsSolved(c, res)
            unatePostProcessing(c, res, file)

    # scatter plot b/w conflict v2 and conflict v1
    # DOv1 = separateConfigs(allResults, {DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 1}).popitem()[1]
    DOv2 = separateConfigs(allResults, {DYNORDER_FIELD: DYNORDER_FIELD, CFORMULA_FIELD: 2}).popitem()[1]
    # NoDOv1 = separateConfigs(allResults, {DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 1}).popitem()[1]
    # NoDOv2 = separateConfigs(allResults, {DYNORDER_FIELD: EMPTY, CFORMULA_FIELD: 2}).popitem()[1]

    # scatterPlot(DOv1, DOv2, name = 'DO')
    # scatterPlot(NoDOv1, NoDOv2, name = 'NoDO')
    


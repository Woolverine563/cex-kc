from ast import Str
from collections import defaultdict
from hashlib import md5
import json, sys
from statistics import mean
import os
import copy
from typing import Any, Dict, List
import scipy.stats
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
            d2[FASTCNF_FIELD] = ''

            mapping[Config(dc)] = {True: d[Config(d1)], False: d[Config(d2)]}

    plot = {}
    
    for k, v in mapping.items():
        # print(v)
        if v[True].isSolved and v[False].isSolved :
            plot[k] = float(v[True].results[TOT_TIME]) / float(v[False].results[TOT_TIME])

    print(list(plot.values()))

def genericAnalysis(results: List[Result]):
    # total results for every possible config
    data = separateConfigs(results)

    # total benchmarks solved
    total = set()
    for k, v in data.items():
        total.update(set([(r.benchmark, r.orderfile) for r in filter(lambda x : x.isSolved, v)]))

    print(f"Total benchmarks solved across any config: {len(total)} out of {max([len(v) for v in data.values()])}")

    err_results = list(filter(lambda x : x.results["ERROR"] != '', itertools.chain(*data.values())))

    errors = [(x.benchmark, x.orderfile) for x in err_results]
    timeouts = [(x.benchmark, x.orderfile) for x in filter(lambda x : 'timed out' in x.results["ERROR"], err_results)]

    other_errs = [x.results["ERROR"] for x in filter(lambda x : 'timed out' not in x.results["ERROR"], err_results)]

    print(f"Errored : {len(errors)} errors across {len(set(errors))} unique benchmarks")
    print(f"Hard Timeouts : {len(timeouts)} timeouts across {len(set(timeouts))} unique benchmarks out of above")
    print(f"Other errors : {other_errs}")
    print()
    print()

    for k, v in data.items():
        # Per config details
        print(f"{k} -> {k.hash()}")

        cnt = len(list(filter(lambda x : x.isSolved, v)))
        print(f"Fully Solved : {cnt}")

        par2 = 0
        for s in v:
            if s.isSolved:
                par2 += float(s.results[TOT_TIME])
            else:
                par2 += k.__dict__[TIMEOUT_FIELD] * 2


        par2 /= len(v)
        print(f"PAR2 Score : {par2:.2f}")

        d = defaultdict(set)

        for r in v:
            analysDict = r.analyse()
            for x,y in analysDict.items():
                if (y):
                    d[x].add(r)
            # for f, _ in r.files.items():
            #     d[f] = d.get(f, set())
            #     d[f].add(r)
        
        assert len(set(d.keys()).difference(set(FILENAMES))) == 0

        folder = f'{sys.argv[1].rsplit("/", 1)[0]}/{k.hash()}'
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

def beyondManthan(results: List[Result]):
    with open('beyond-manthan.txt') as f:
        data = f.read().strip().split()

    res = defaultdict(lambda : [])
    for d in data:
        for r in results:
            if (d in r.benchmark) and r.isSolved and r.isFixedConf() and (not r.config.bool_field(DYNORDER_FIELD)):
                res[d].append(r)
    for d,r in res.items():
        print(d)
        print(r)
        print()

def ratioOutputsSolved(results: List[Result]):
    global PLOT_CNT
    data = separateConfigs(results)

    # we may need per config graph
    for _, r in data.items():
        values = defaultdict(list)
        for res in r:
            if (res.error != ''):
                continue
            tot_oups = int(res.results[TOT_OUTPUTS])
            fixed_oups = int(res.results[FIXED_OUTPUTS])
            
            if tot_oups == 0:
                continue
            values[res.isSolved].append((res.benchmark, fixed_oups/tot_oups, fixed_oups, tot_oups, int(res.results[INIT_UN]), int(res.results[FIN_UN])))

        bar1 = values[False]
        bar1.sort(key=lambda x: x[1])
        xvalues = [x[0] for x in bar1]
        y1 = [x[1] for x in bar1]

        print(mean(y1), scipy.stats.gmean(list(filter(lambda x: x != 0, y1))), mean(list(filter(lambda x: x != 0, y1))))
        print()
        
        almostSolved = list(filter(lambda t: 0.9 <= t[1] <= 1.0, bar1))
        print(f"Almost solved {len(almostSolved)} benchmarks")
        print(almostSolved)

        plt.figure()
        plt.bar(xvalues, y1, color='blue', label='Outputs fixed / Non-unate outputs', width=1.0)
        # plt.bar(xvalues, y2, color='green', label='% total unates', bottom=y1, width=1.0)
        plt.xticks([])
        plt.ylabel('Ratio')
        plt.xlabel('Unsolved benchmarks')
        plt.legend(loc='upper left')
        plt.autoscale(enable=True, axis='both', tight=True)
        plt.tight_layout()
        plt.savefig(f'graph_{PLOT_CNT}_ratio_outputs.png')
        PLOT_CNT += 1

def unatePostProcessing(results: List[Result], force_run = False):
    global PLOT_CNT
    data = separateConfigs(results, {UNATE_FIELD: UNATE_FIELD})

    run(["make", "postprocess"])

    for c, r in data.items():
        # folder
        folder = getattr(c, OUTFOLDR_FIELD) if (hasattr(c, OUTFOLDR_FIELD)) else f"{sys.argv[1].rsplit('/', 1)[0]}/"

        values = []
        for res in r:
            if (res.error != ''):
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
            continue
        values.sort(key=lambda x: x[1])

        yvalues = [x[1] for x in values]
        print(scipy.stats.describe(yvalues))
        print([(t[0].benchmark, t[1], t[2], t[3]) for t in values if t[1] > 0])

        plt.figure()
        plt.bar(range(len(yvalues)), yvalues, width=1.0)
        plt.xticks([])
        plt.ylabel('Ratio')
        plt.xlabel('Benchmarks')
        plt.autoscale(enable=True, axis='both', tight=True)
        plt.tight_layout()
        plt.savefig(f'graph_{PLOT_CNT}_ratio_unates.png')
        PLOT_CNT += 1

for file in sys.argv[1:]:
    with open(file, 'r') as f:
        data = json.load(f)

    results = [Result(x) for x in data]
    print('\n' + '-'*100)
    print(f"{file}")
    print('-'*100 + '\n')

    genericAnalysis(results)
    beyondManthan(results)
    ratioOutputsSolved(results)
    unatePostProcessing(results)
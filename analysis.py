from ast import Str
from collections import defaultdict
from datetime import date
from email.policy import default
from hashlib import md5
import json, sys
import os
import copy
from typing import Any, Dict, List

from more_itertools import difference
from util import *
import itertools
import matplotlib.pyplot as plt

MAN_TIMEOUT = 3600 + 100 # slack

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

    for k in data.keys():
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

    err_results = list(filter(lambda x : x.results["ERROR"] is not '', itertools.chain(*data.values())))

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

def percentageOutputsSolved(results: List[Result]):
    data = separateConfigs(results)
    i = 0

    # we may need per config graph
    for _, r in data.items():
        values = defaultdict(list)
        for res in r:
            if (res.error != ''):
                continue
            tot_oups = int(res.results[TOT_OUTPUTS])
            fixed_oups = int(res.results[FIXED_OUTPUTS])

            values[res.isSolved].append((res.benchmark, fixed_oups/tot_oups if tot_oups != 0 else 1))

        bar1 = values[False]
        bar1.sort(key=lambda x: x[1])
        xvalues = [x[0] for x in bar1]
        y1 = [x[1] for x in bar1]

        plt.figure()
        plt.bar(xvalues, y1, color='blue', label='Outputs fixed / Non-unate outputs', width=1.0)
        # plt.bar(xvalues, y2, color='green', label='% total unates', bottom=y1, width=1.0)
        plt.xticks([])
        plt.ylabel('Ratio')
        plt.xlabel('Unsolved benchmarks')
        plt.legend(loc='upper left')
        plt.autoscale(enable=True, axis='both', tight=True)
        plt.tight_layout()
        plt.savefig(f'graph_{i}.png')
        i += 1

# with open(sys.argv)
with open(sys.argv[1], 'r') as f:
    data = json.load(f)

results = [Result(x) for x in data]

genericAnalysis(results)
beyondManthan(results)
percentageOutputsSolved(results)
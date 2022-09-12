from datetime import date
import json, sys
import copy
from util import *
import itertools
import matplotlib.pyplot as plt

class Config:
    def __init__(self, config):
        self.__dict__.update(config)

    def keys(self):
        return self.__dict__.keys()

    def __eq__(self, __o):
        return self.__dict__ == __o.__dict__

    def __str__(self):
        return f"Config({self.__dict__})"

    def __repr__(self) -> str:
        return self.__str__()

    def __hash__(self):
        return str(self).__hash__()

class Result:
    def __init__(self, data: dict):
        self.benchmark = data[BNAME]
        self.orderfile = data[VORDER]
        self.hash = data.pop(HASH)
        self.config = Config(data["config"])

        self.isSolved = data["isSolved"]
        self.results = data["results"]
        self.files = data["files"]

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

def genericAnalysis(results):
    # total results for every possible config
    filterFields =  CONFIG_FIELDS

    data = {}

    for r in results:
        r1, r2 = r.extract(filterFields)
        data.setdefault(r1, []).append(r2)

    for k, v in data.items():
        cnt = len(list(filter(lambda x : x.isSolved, v)))
        print(f"{k} -> {cnt} fully solved")

    # total benchmarks solved
    total = set()
    for k, v in data.items():
        total.update(set([(r.benchmark, r.orderfile) for r in filter(lambda x : x.isSolved, v)]))

    print(f"Total benchmarks solved : {len(total)}")
    print()

    err_results = list(filter(lambda x : x.results["ERROR"] is not '', itertools.chain(*data.values())))

    errors = [(x.benchmark, x.orderfile) for x in err_results]
    timeouts = [(x.benchmark, x.orderfile) for x in filter(lambda x : 'timed out' in x.results["ERROR"], err_results)]

    other_errs = [x.results["ERROR"] for x in filter(lambda x : 'timed out' not in x.results["ERROR"], err_results)]

    print(f"Errored : {len(errors)} errors across {len(set(errors))} unique benchmarks")
    print(f"Hard Timeouts : {len(timeouts)} timeouts across {len(set(timeouts))} unique benchmarks out of above")
    print(f"Other errors : {other_errs}")
    print()

    par2_dict = {}

    for k, v in data.items():
        par2 = 0

        for s in v:
            if s.isSolved:
                par2 += float(s.results[TOT_TIME])
            else:
                par2 += k.__dict__[TIMEOUT_FIELD] * 2

        par2 /= len(v)
        par2_dict[k] = par2

    print(f"PAR2 scores : {par2_dict}")
    print()

    for k, v in data.items():
        d = {}

        for r in v:
            for f, _ in r.files.items():
                d[f] = d.get(f, 0) + 1
        
        print(f"{k} -> {sorted(d.items())} counts, total = {sum(d.values())}")

# with open(sys.argv)
with open(sys.argv[1], 'r') as f:
    data = json.load(f)

results = [Result(x) for x in data]

genericAnalysis(results)
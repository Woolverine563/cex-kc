import json, sys
import copy
from util import *
import matplotlib.pyplot as plt

class Config:
    def __init__(self, config):
        self.__dict__.update(config)

    def keys(self):
        return self.__dict__.keys()

    def __eq__(self, __o):
        return self.__dict__ == __o.__dict__

    def __hash__(self):
        return str(self.__dict__).__hash__()

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


# with open(sys.argv)
with open(sys.argv[1], 'r') as f:
    data = json.load(f)

results = [Result(x) for x in data]

compareFastCNFTime(results)
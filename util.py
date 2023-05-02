import copy
from hashlib import md5
from typing import Any, Dict, List

# SOME CONSTANTS LIE HERE

LINES_COUNT = 7


# NOW ALL IS FIELDS AND SUCH

BNAME = "Benchmark Name"
VORDER = "Variable Ordering"
HASH = "Hash"

EMPTY = ""
BNAME_FIELD = "-b"
VORDER_FIELD = "-v"
OUTFOLDR_FIELD = "--out"
TIMEOUT_FIELD = "-t"
UNATE_FIELD = "-u"
FASTCNF_FIELD = "-f"
DYNORDER_FIELD = "-o"
CFORMULA_FIELD = "-c"
BOOL_FIELDS = [UNATE_FIELD,"-s",DYNORDER_FIELD,FASTCNF_FIELD]
VAL_FIELDS = [CFORMULA_FIELD, "-r", "-d", TIMEOUT_FIELD, "--unateTimeout"]
NON_BOOL_FIELDS = [BNAME_FIELD, VORDER_FIELD] + VAL_FIELDS
CONFIG_FIELDS = BOOL_FIELDS + VAL_FIELDS
FIELDS = [BNAME_FIELD, VORDER_FIELD] + CONFIG_FIELDS + [HASH]
# name, order, booleans, values

INIT_UN = "Initial unates"
FIN_UN = "Final unates"
PHASE_CNT = "Phase Count"
TOT_OUTPUTS = "Total outputs without unate"
FIXED_OUTPUTS = "Outputs fixed"
NUM_IT = "Number of iterations"
NUM_CEX = "Number of cex"
TOT_TIME = "Time taken"
ERR = "ERROR"

RESULTS = ["Initial size", "Final size", INIT_UN, FIN_UN, PHASE_CNT, NUM_IT, NUM_CEX, FIXED_OUTPUTS, TOT_OUTPUTS, TOT_TIME, "repairTime", "conflictCnfTime", "satSolvingTime", "unateTime", "compressTime", "rectifyCnfTime", "rectifyUnsatCoreTime", "overallCnfTime", ERR]

# ["Benchmark", "VarOrder", "unate?","shannon?", "dynamic?", "fastCNF?"]
HEADER = FIELDS + RESULTS

ALLUNATES = "allUnates"
NOCONFU = "noConflictsWithUnates"
NOCONF = "noConflicts"
NOU = "noUnates"
OTHER = "others"
ERROR = "error"
FIXEDCONF = "fixedConflicts"
SOMEU = "someUnates"

FILENAMES = [ALLUNATES, NOCONFU, NOCONF, NOU, OTHER, ERROR, FIXEDCONF, SOMEU]

ORDER_TYPES = [ALLUNATES, SOMEU, NOU, FIXEDCONF, NOCONF]

IS_SOLVED = "isSolved"



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


# row maps directly to HEADER
def process(D: dict, outputs: list, error: bool) :
    # U in the end signifies Unate, no U means No Unate case
    row = [D[k] for k in FIELDS] + outputs
    bname, vorder, hash = D[BNAME_FIELD], D[VORDER_FIELD], D[HASH]
    def isSolved():
        return not error and (int(row[HEADER.index(FIXED_OUTPUTS)]) == int(row[HEADER.index(TOT_OUTPUTS)]))
    def isAllU():
        return int(row[HEADER.index(TOT_OUTPUTS)]) == 0 and isSolved()
    def isNoConfU():
        return (UNATE_FIELD in row) and (int(row[HEADER.index(NUM_CEX)]) == 0) and isSolved()
    def isNoConf():
        return (UNATE_FIELD not in row) and (int(row[HEADER.index(NUM_CEX)]) == 0) and isSolved()
    def isNoU():
        # fully solved but had no unates
        return (UNATE_FIELD in row) and (int(row[HEADER.index(FIN_UN)]) == 0) and isSolved()
    
    d = dict()
    d[ERROR] = error
    d[ALLUNATES] = isAllU()
    d[NOCONFU] = isNoConfU()
    d[NOCONF] = isNoConf()
    d[NOU] = isNoU()
    d[OTHER] = isSolved() and (not any(d.values()))

    for k in [BNAME_FIELD, VORDER_FIELD, HASH]:
        D.pop(k)

    d_all = dict({BNAME: bname, VORDER: vorder, HASH: hash, "config": D, IS_SOLVED: isSolved(), "results": dict(zip(RESULTS, outputs)), "files": d})
   
    return row, bname, hash, d, d_all
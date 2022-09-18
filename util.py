BNAME = "Benchmark Name"
VORDER = "Variable Ordering"
HASH = "Hash"

BNAME_FIELD = "-b"
VORDER_FIELD = "-v"
TIMEOUT_FIELD = "-t"
UNATE_FIELD = "-u"
FASTCNF_FIELD = "-f"
BOOL_FIELDS = [UNATE_FIELD,"-s","-o",FASTCNF_FIELD]
VAL_FIELDS = [ "-c", "-r", "-d", TIMEOUT_FIELD, "--unateTimeout"]
NON_BOOL_FIELDS = [BNAME_FIELD, VORDER_FIELD] + VAL_FIELDS
CONFIG_FIELDS = BOOL_FIELDS + VAL_FIELDS
FIELDS = [BNAME_FIELD, VORDER_FIELD] + CONFIG_FIELDS + [HASH]
# name, order, booleans, values

INIT_UN = "Initial unates"
FIN_UN = "Final unates"
TOT_OUTPUTS = "Total outputs without unate"
FIXED_OUTPUTS = "Outputs fixed"
NUM_CEX = "Number of cex"
TOT_TIME = "Time taken"

RESULTS = ["Initial size", "Final size", INIT_UN, FIN_UN, "Number of iterations", NUM_CEX, FIXED_OUTPUTS, TOT_OUTPUTS, TOT_TIME, "repairTime", "conflictCnfTime", "satSolvingTime", "unateTime", "compressTime", "rectifyCnfTime", "rectifyUnsatCoreTime", "overallCnfTime", "ERROR"]

# ["Benchmark", "VarOrder", "unate?","shannon?", "dynamic?", "fastCNF?"]
HEADER = FIELDS + RESULTS

ALLUNATES = "allUnates"
NOCONFU = "noConflictsWithUnates"
NOCONF = "noConflicts"
NOU = "noUnates"
OTHER = "others"
ERROR = "error"

FILENAMES = [ALLUNATES, NOCONFU, NOCONF, NOU, OTHER, ERROR]

IS_SOLVED = "isSolved"

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
    if (error):
        d[ERROR] = True
    else:
        if isAllU():
            d[ALLUNATES] = True
        if isNoConfU():
            d[NOCONFU] = True
        if isNoConf():
            d[NOCONF] = True
        if isNoU():
            d[NOU] = True
        if (len(d) == 0) and isSolved():
            d[OTHER] = True

    for k in [BNAME_FIELD, VORDER_FIELD, HASH]:
        D.pop(k)

    d_all = dict({BNAME: bname, VORDER: vorder, HASH: hash, "config": D, IS_SOLVED: isSolved(), "results": dict(zip(RESULTS, outputs)), "files": d})
   
    return row, bname, hash, d, d_all
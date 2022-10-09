#include "unatesPostProcess.h"

using namespace std;

vector<pair<int, bool>> unates, unatesOnly;

int main(int argc, char** argv) {
    map<string, int> name2IdF;
    map<int, string> id2NameF;
    string line;

    int pu = 0, nu = 0, puOnly = 0, nuOnly = 0;

    optParser.positional_help("");
    optParser.add_options()
    ("b, benchmark", "Benchmark", cxxopts::value<string>(options.benchmark))
    ("p, pos", "Positive unates file", cxxopts::value<string>(options.pUnate))
    ("n, neg", "Negative unates file", cxxopts::value<string>(options.nUnate))
    ("out", "Output folder path", cxxopts::value<string>(options.outFolderPath))
    ("h, help", "Print help");

    auto result = optParser.parse(argc, argv);
    
    if (result.count("help")) {
        cout << optParser.help({"", "Group"}) << endl;
        exit(0);
    }

    if (!result.count("benchmark") || !result.count("pos") || !result.count("neg")) {
        cerr << endl << "Error: input not specified appropriately" << endl;
        cout << optParser.help({"", "Group"}) << endl;
        exit(1);
    }

    auto FNtk = getNtk(options.benchmark, true);
    auto FAig = Abc_NtkToDar(FNtk, 0, 0);
    
    int i;
    Abc_Obj_t* pPi;
    Abc_NtkForEachCi(FNtk, pPi, i) {
        string objName = Abc_ObjName(pPi);
        name2IdF[objName] = pPi->Id;
        id2NameF[pPi->Id - 1] = objName;
    }    

    ifstream posStream(options.pUnate);
    assert(posStream.is_open());
    while (getline(posStream, line)) {
        if (line != "") {
            unates.push_back(make_pair(name2IdF[line] - 1, true)); // no asserts inserted, AIG IO id is here
            pu++;
        }
    }
    posStream.close();

    ifstream negStream(options.nUnate);
    assert(negStream.is_open());
    while (getline(negStream, line)) {
        if (line != "") {
            unates.push_back(make_pair(name2IdF[line] - 1, false));
            nu++;
        }
    }
    negStream.close();

    for (auto obj : unates) {
        auto Aig = Aig_ManDupSimpleDfs(FAig);

        auto pObj = Aig_Compose(Aig, Aig_ManCo(Aig, 0)->pFanin0, Aig_NotCond(Aig_ManConst0(Aig), !obj.second), obj.first);
        Aig_ObjPatchFanin0(Aig, Aig_ManCo(Aig, 0), pObj);
        Aig_ManCleanup(Aig);

        auto Cnf = Cnf_Derive(Aig, 0);
        auto pSat = sat_solver_new();
        if (addCnfToSolver(pSat, Cnf)) {
            lbool status = sat_solver_solve(pSat, NULL, NULL, 0, 0, 0, 0);
            assert(status != l_Undef);
            if (status == l_False) {
                unatesOnly.push_back(obj);
            }
        }
        else {
            unatesOnly.push_back(obj);
        }
        sat_solver_delete(pSat);
        Cnf_DataFree(Cnf);
        Aig_ManStop(Aig);
    }

    ofstream pUnates(options.outFolderPath + "/UnatesOnly/" + getFileName(options.benchmark) + ".pUnates");
    ofstream nUnates(options.outFolderPath + "/UnatesOnly/" + getFileName(options.benchmark) + ".nUnates");

    for (auto obj : unatesOnly) {
        string ans = id2NameF[obj.first];
        if (obj.second) {
            pUnates << ans << endl;
            puOnly++;
        }
        else {
            nUnates << ans << endl;
            nuOnly++;
        }
    }

    pUnates.close();
    nUnates.close();

    Aig_ManStop(FAig);
    Abc_NtkDelete(FNtk);

    cout << pu << " " << nu << " " << puOnly << " " << nuOnly << endl;
}
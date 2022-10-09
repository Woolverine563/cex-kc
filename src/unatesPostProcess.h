#include <iostream>
#include <vector>
#include <map>
#include "cxxopts.hpp"

#define ABC_NAMESPACE NS_ABC

extern "C"
{
#include "misc/util/abc_global.h"
#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "base/cmd/cmd.h"
#include "base/abc/abc.h"
#include "misc/nm/nmInt.h"
#include "sat/cnf/cnf.h"
#include "sat/bsat/satStore.h"
#include "sat/bsat/satSolver.h"
#include "sat/bsat/satSolver2.h"
#include "opt/mfs/mfs.h"
#include "opt/mfs/mfsInt.h"
#include "bool/kit/kit.h"
#include "bdd/cudd/cuddInt.h"
#include "bdd/cudd/cudd.h"
}
namespace ABC_NAMESPACE
{
	extern "C"
	{
		Aig_Man_t *Abc_NtkToDar(Abc_Ntk_t *pNtk, int fExors, int fRegisters);
		Abc_Ntk_t *Abc_NtkFromAigPhase(Aig_Man_t *pMan);
		int Aig_ObjTerSimulate(Aig_Man_t *pAig, Aig_Obj_t *pNode, Vec_Int_t *vSuppLits);
		// static Cnf_Man_t * s_pManCnf;
		void Aig_ConeMark_rec(Aig_Obj_t *pObj);
	}
}

using namespace std;
using namespace ABC_NAMESPACE;

struct postProcessOption
{
    string benchmark;
    string pUnate;
    string nUnate;
    string outFolderPath;
};

cxxopts::Options optParser("unates-postprocess");
postProcessOption options;
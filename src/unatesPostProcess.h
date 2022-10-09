#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
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


Abc_Ntk_t *getNtk(string pFileName, bool fraig)
{
	string cmd, initCmd, varsFile, line;

	initCmd = "balance; rewrite -l; refactor -l; balance; rewrite -l; \
						rewrite -lz; balance; refactor -lz; rewrite -lz; balance";

	auto pAbc = Abc_FrameGetGlobalFrame();

	cmd = "read " + pFileName;
	if (Cmd_CommandExecute(pAbc, cmd.c_str()))
	{ // Read the AIG
		return NULL;
	}
	cmd = fraig ? initCmd : "balance";
	if (Cmd_CommandExecute(pAbc, cmd.c_str()))
	{ // Simplify
		return NULL;
	}

	Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
	// Aig_Man_t* pAig = Abc_NtkToDar(pNtk, 0, 0);
	return pNtk;
}

bool addCnfToSolver(sat_solver *pSat, Cnf_Dat_t *cnf)
{
	bool retval = true;
	sat_solver_setnvars(pSat, sat_solver_nvars(pSat) + cnf->nVars);
	for (int i = 0; i < cnf->nClauses; i++)
		if (!sat_solver_addclause(pSat, cnf->pClauses[i], cnf->pClauses[i + 1]))
		{
			retval = false;
		}
	return retval;
}

string getFileName(string s)
{
	size_t i;

	i = s.rfind('/', s.length());
	if (i != string::npos)
	{
		s = s.substr(i + 1);
	}
	assert(s.length() != 0);

	i = s.rfind('.', s.length());
	if (i != string::npos)
	{
		s = s.substr(0, i);
	}
	assert(s.length() != 0);

	return (s);
}

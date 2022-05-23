#ifndef HELPER_H
#define HELPER_H

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>
#include <list>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>
#include <signal.h>
#include "cxxopts.hpp"

// #ifndef NO_UNIGEN
// #include "cusp.h"
// #else
namespace CMSat{
	class CUSP {
	public:
		static bool unigenRunning;
		static int getSolutionMapSize(){return 0;}
	};
}
// #endif

#define ABC_NAMESPACE NS_ABC

extern "C" {
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
namespace ABC_NAMESPACE {
	extern "C" {
	Aig_Man_t * Abc_NtkToDar(Abc_Ntk_t * pNtk, int fExors, int fRegisters);
	Abc_Ntk_t * Abc_NtkFromAigPhase(Aig_Man_t * pMan);
	int Aig_ObjTerSimulate( Aig_Man_t * pAig, Aig_Obj_t * pNode, Vec_Int_t * vSuppLits );
	// static Cnf_Man_t * s_pManCnf;
	void Aig_ConeMark_rec(Aig_Obj_t* pObj);
	}
}

using namespace std;
using namespace ABC_NAMESPACE;

#define STR_HELPER(X)		#X
#define STR(X)				STR_HELPER(X)
#define UNIGEN_OUT_DIR		"out"
#define UNIGEN_INPUT_FNAME 	"errorFormula"
#define UNIGEN_OUTPT_FNAME	"unigen_output.txt"
#define UNIGEN_DIMAC_FNAME 	UNIGEN_INPUT_FNAME ".cnf"
#define UNIGEN_MODEL_FNAME	UNIGEN_INPUT_FNAME "_0.txt"
#define UNIGEN_OUTPT_FPATH	UNIGEN_OUTPT_FNAME
#define UNIGEN_MODEL_FPATH	UNIGEN_OUT_DIR "/" UNIGEN_MODEL_FNAME
#define UNIGEN_DIMAC_FPATH 	UNIGEN_DIMAC_FNAME
#define UNIGEN_PY 			"UniGen2.py"
#define UNIGEN_SAMPLES_DEF	110000
#define UNIGEN_THREADS_DEF	19
#define INIT_COLLAPSE_PARAM	4
#define REF_COLLAPSE_PARAM	3
#define UNIGEN_THRESHOLD 	0.3
#define WAIT_SAMPLES_DEF 	110
#define FMCAD_SIZE_THRESH	1e6
#define UNATE_TIMEOUT		3600

// #define DEBUG
// #define DEBUG_CHUNK
// #define COMPARE_SAIGS // Uncomment to compare 2 SAigs
#ifdef DEBUG
	#define OUT( x ) cout << "DEBUG: " << x << endl
#else
	#define OUT( x )
#endif

typedef std::chrono::time_point<std::chrono::steady_clock> chrono_steady_time;
#define TIME_NOW			 std::chrono::steady_clock::now()
#define TIME_MEASURE_START   helper_time_measure_start = TIME_NOW;
#define TIME_MEASURE_ELAPSED (std::chrono::duration_cast<std::chrono::microseconds>(TIME_NOW-helper_time_measure_start).count()/1000000.0)
#define TIMED(var, compn) 			 TIME_MEASURE_START compn; var += TIME_MEASURE_ELAPSED;
extern  chrono_steady_time helper_time_measure_start;
extern  chrono_steady_time main_time_start;

class  edge;
class  node;
class  Nnf_Man;

enum sType {skolemR0, skolemR1, skolemRx};
struct optionStruct {
	bool	fixAllIndices;
	bool 	unate;
	int 	timeout;
	string 	benchmark;
	string 	outFName;
	string 	varsOrder;
	int 	conflictCheck;
	int 	rectifyProc;
	int 	depth;
	int 	unateTimeout;
	bool	useShannon;
	bool 	allowUnivQuantify;
	bool 	dynamicOrdering;
};

struct ConflictCounterEx {
	vector<int> X;
	vector<int> Y;
	int idx = 0;
};

struct Lit {
	int lit;

	Lit(int l) : lit(l) {}
	Lit(int v, int c) : lit(Abc_Var2Lit(v, c)) {}
	Lit(const Lit& l) : lit(l.lit) {}

	inline int getLit() const {
		return lit;
	}

	inline int getVar() const {
		return Abc_Lit2Var(lit);
	}

	inline bool isCompl() const {
		return Abc_LitIsCompl(lit);
	}

	inline Lit getCompl() const {
		return Lit(Abc_LitNot(lit));
	}

	inline bool operator<(const Lit& other) const {
		return Abc_Lit2Var(lit) < Abc_Lit2Var(other.lit);
	}

	inline operator int() const {
		return lit;
	}
};

extern vector<int> varsSInv;
extern vector<int> varsXF, varsXS;
extern vector<int> varsYF, varsYS; // to be eliminated
extern int numOrigInputs, numX, numY;
extern vector<string> varsNameX, varsNameY;
extern Abc_Frame_t* pAbc;
extern sat_solver* m_pSat;
extern Cnf_Dat_t* m_FCnf;
extern cxxopts::Options optParser;
extern optionStruct options;
extern vector<vector<int> > k2Trend;
extern ConflictCounterEx pi;
extern int it;

int 			CommandExecute(Abc_Frame_t* pAbc, string cmd);
vector<string> 	tokenize( const string& p_pcstStr, char delim );
string 			type2String(Aig_Type_t t);
bool 			Equate(sat_solver *pSat, int varA, int varB);
bool 			Xor(sat_solver *pSat, int varA, int varB);
Abc_Ntk_t* 		getNtk(string pFileName, bool fraig);
int 			getNumY(string varsFile);
void 			populateVars(Abc_Ntk_t* FNtk, string varsFile,
					vector<int>& varsXF, vector<int>& varsXS,
					vector<int>& varsYF, vector<int>& varsYS,
					map<string,int>& name2IdF, map<int,string>& id2NameF);
Aig_Obj_t* 		Aig_SubstituteConst(Aig_Man_t* pMan, Aig_Obj_t* initAig, int varId, int one);
Aig_Obj_t* 		Aig_Substitute(Aig_Man_t* pMan, Aig_Obj_t* initAig, int varId, Aig_Obj_t* func);
bool 			addVarToSolver(sat_solver* pSat, int varNum, int val);
int 			getCnfCoVarNum(Cnf_Dat_t* cnf, Aig_Man_t* aig, int nthCo);
lit 			OR(sat_solver* pSat, lit lh, lit rh);
bool 			addCnfToSolver(sat_solver* pSat, Cnf_Dat_t* cnf);
void 			evaluateAig(Aig_Man_t* formula, const vector<int> &cex);
Aig_Obj_t* 		satisfiesVec(Aig_Man_t* formula, const vector<int>& cex, const vector<int>& coObjs, bool reEvaluate);
Aig_Obj_t* 		generalize(Aig_Man_t*pMan, vector<int> cex, const vector<int>& rl);
bool 			Aig_Support_rec(Aig_Man_t* pMan, Aig_Obj_t* root, int inpNodeId, map<Aig_Obj_t*,bool>& memo);
bool 			Aig_Support(Aig_Man_t* pMan, Aig_Obj_t* root, int inpNodeId);
vector<Aig_Obj_t *> Aig_SupportVec(Aig_Man_t* pMan, Aig_Obj_t* root);
Aig_Obj_t* 		Aig_AndAigs(Aig_Man_t* pMan, Aig_Obj_t* Aig1, Aig_Obj_t* Aig2);
Aig_Obj_t* 		Aig_OrAigs(Aig_Man_t* pMan, Aig_Obj_t* Aig1, Aig_Obj_t* Aig2) ;
Aig_Obj_t* 		AND_rec(Aig_Man_t* SAig, vector<Aig_Obj_t* >& nodes, int start, int end);
Aig_Obj_t* 		newAND(Aig_Man_t* SAig, vector<Aig_Obj_t* >& nodes) ;
Aig_Obj_t* 		projectPi(Aig_Man_t* pMan, const vector<int> &cex, const int m);
Aig_Man_t* 		compressAig(Aig_Man_t* SAig);
Aig_Man_t* 		compressAigByNtk(Aig_Man_t* SAig);
Aig_Man_t* 		compressAigByNtkMultiple(Aig_Man_t* SAig, int times);
Aig_Obj_t* 		OR_rec(Aig_Man_t* SAig, vector<int>& nodes, int start, int end);
Aig_Obj_t* 		newOR(Aig_Man_t* SAig, vector<int>& nodes);
Aig_Obj_t* 		Aig_XOR(Aig_Man_t*p, Aig_Obj_t*p0, Aig_Obj_t*p1);
Aig_Obj_t* 		Aig_SubstituteVec(Aig_Man_t* pMan, Aig_Obj_t* initAig, vector<int> varIdVec,
					vector<Aig_Obj_t*>& funcVec);
vector<Aig_Obj_t* >	Aig_SubstituteVecVec(Aig_Man_t* pMan, Aig_Obj_t* initAig, vector<vector<Aig_Obj_t*> >& funcVecs);
void 			Aig_ComposeVec_rec( Aig_Man_t * p, Aig_Obj_t * pObj, vector<Aig_Obj_t *>& pFuncVec,
					vector<Aig_Obj_t* >& iVarObjVec );
Aig_Obj_t*	 	Aig_ComposeVec( Aig_Man_t * p, Aig_Obj_t * pRoot, vector<Aig_Obj_t *>& pFuncVec,
					vector<int>& iVarVec );
void 			Aig_VecVecConeUnmark_rec(Aig_Obj_t * pObj);
void 			Aig_ComposeVecVec_rec(Aig_Man_t* p, Aig_Obj_t* pObj, vector<vector<Aig_Obj_t*> >& pFuncVecs);
vector<Aig_Obj_t* > Aig_ComposeVecVec(Aig_Man_t* p, Aig_Obj_t* pRoot, vector<vector<Aig_Obj_t*> >& pFuncVecs);
void			parseOptions(int argc, char * argv[]);
void			parseOptionsOrdering(int argc, char * argv[]);
string			getFileName(string s);
int 			checkUnateSyntacticAll(Aig_Man_t* FAig, vector<int>&unate);
int 			checkUnateSemanticAll(Aig_Man_t* FAig, vector<int>&unate);
int 			checkUnateSemAll(Aig_Man_t* FAig, vector<int>&unate);//Same as checkUnateSemanticAll but more memory efficient
void 			populateVars(Abc_Ntk_t* FNtk, string varsFile, vector<string>& varOrder,
					vector<int>& varsXF, vector<int>& varsYF,
					map<string,int>& name2IdF, map<int,string>& id2NameF);
void 			printAig(Aig_Man_t* pMan);
int 			Aig_DagSizeWithConst(Aig_Obj_t * pObj);
Aig_Obj_t*		coreAndIntersect(Aig_Man_t*, Aig_Man_t*);
Aig_Man_t* 		PositiveToNormal(Aig_Man_t* SAig);
Aig_Man_t* 		PositiveToNormalWithNeg(Aig_Man_t* SAig);
Aig_Man_t* 		NormalToPositive(Aig_Man_t* &FAig);
bool 			Aig_IsPositive(Aig_Man_t* SAig);
void 			repair(Aig_Man_t* SAig);
bool 			isConflict(Aig_Man_t* SAig, int idx);
void 			setUnatesSaig(Aig_Man_t* SAig, vector<int> &unates);
int 			checkUnate(Aig_Man_t* SAig, vector<int> &unates, int repairedIndex);
void 			doShannonExp(Aig_Man_t* SAig, int idx);
Cnf_Dat_t* 		getErrorFormulaCNF(Aig_Man_t* SAig);
Cnf_Dat_t* 		getConflictFormulaCNF(Aig_Man_t* SAig, int idx);
Cnf_Dat_t* 		getConflictFormulaCNF2(Aig_Man_t* SAig, int idx);
lbool 			solveAndModel(Aig_Man_t* SAig, Cnf_Dat_t* cnf);
void 			calcLeastOccurrenceSAig(Aig_Man_t *SAig, int minIdx);

template<class T>
void print(T v) {
	cout << v;
}

template<class T>
void print(vector<T> v, string delim) {
	for (int i = 0; i < v.size(); ++i) {
		print(v[i]);
		cout << delim;
	}
}

#endif

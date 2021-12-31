
////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////
#include "helper.h"
#include "nnf.h"

using namespace std;

ostream& operator<<(ostream& os, const ConflictCounterEx& cce) {
	os << "X: ";
	for (int x : cce.X) {
		os << x << " ";
	}
	os << endl;
	os << "Y: ";
	for (int y : cce.Y) {
		os << y << " ";
	}
	os << endl;
	return os;
}

////////////////////////////////////////////////////////////////////////
///                           GLOBALS                                ///
////////////////////////////////////////////////////////////////////////
vector<int> varsSInv;
vector<int> varsXF, varsXS;
vector<int> varsYF, varsYS; // to be eliminated.
ConflictCounterEx pi;
int numOrigInputs = 0, numX = 0, numY = 0;
vector<string> varsNameX, varsNameY;
Abc_Frame_t* pAbc = NULL;
sat_solver* m_pSat = NULL;
Cnf_Dat_t* m_FCnf = NULL;
lit m_f = 0;
double sat_solving_time = 0;
double verify_sat_solving_time = 0;
double reverse_sub_time = 0;
double compression_time = 0;
double repair_time = 0;
double core_comp_time = 0;
double resolv_time = 0;
chrono_steady_time helper_time_measure_start = TIME_NOW;
chrono_steady_time main_time_start = TIME_NOW;

int it = 0;

Cnf_Dat_t* getErrorFormulaCNF(Aig_Man_t* SAig) {
	vector<Aig_Obj_t*> skolems(numY);

	Aig_Obj_t* pObj;
	Aig_Man_t* Formula = Aig_ManDupSimpleDfs(SAig);

	// no unates for now
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (auto i : varsXS) {
		vars.push_back(i + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(Formula, i)));
	}

	pObj = Aig_ManCo(Formula, 0)->pFanin0;
	pObj = Aig_ComposeVec(Formula, pObj, funcs, vars);

	assert(!Aig_ObjIsCo(Aig_Regular(pObj)));
	Aig_ObjPatchFanin0(Formula, Aig_ManCo(Formula, 0), pObj);

	for (int i = 0; i < numY; i++) {
		// assumes r0
		auto v = vars;
		auto f = funcs;

		for (int j = 0; j < numY; j++) {
			if (j < i) {
				v.push_back(varsYS[j]);
				v.push_back(varsYS[j] + numOrigInputs);

				f.push_back(Aig_ManConst1(Formula));
				f.push_back(Aig_ManConst1(Formula));
			}
			else if (j == i) {
				v.push_back(varsYS[j]);
				v.push_back(varsYS[j] + numOrigInputs);

				f.push_back(Aig_ManConst1(Formula));
				f.push_back(Aig_ManConst0(Formula));
			}
			else {
				v.push_back(varsYS[j] + numOrigInputs);
				f.push_back(Aig_Not(Aig_ManCi(Formula, varsYS[j])));
			}
		}

		pObj = Aig_ManCo(Formula, 0)->pFanin0;
		skolems[i] = Aig_ComposeVec(Formula, pObj, f, v);

		// Aig_ObjPrint(Formula, skolems[i]);
		// cout << endl;
	}


	vars.clear();
	funcs.clear();

	for (int i = 0; i < numY; i++) {
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(Formula, varsYS[i])));
	}

	pObj = Aig_ManCo(Formula, 0)->pFanin0;
	pObj = Aig_ComposeVec(Formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(Formula, Aig_ManCo(Formula, 0), pObj);

	// formula is devoid of all negations now!
	// need to add F(X, Y')
	// reusing Ybar
	pObj = Aig_ManCo(Formula, 0)->pFanin0;
	vars.clear();
	funcs.clear();

	for (int i = 0; i < numY; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_ManCi(Formula, varsYS[i] + numOrigInputs));
	}

	pObj = Aig_And(Formula, Aig_ComposeVec(Formula, pObj, funcs, vars), Aig_Not(pObj));

	Aig_ObjPatchFanin0(Formula, Aig_ManCo(Formula, 0), pObj);

	pObj = Aig_ManCo(Formula, 0)->pFanin0;

	for (int i = 0; i < numY; i++) {
		Aig_Obj_t* iff_clause = Aig_Not(Aig_XOR(Formula, skolems[i], Aig_ManCi(Formula, varsYS[i])));
		pObj = Aig_And(Formula, pObj, iff_clause);
	}

	Aig_ObjPatchFanin0(Formula, Aig_ManCo(Formula, 0), pObj);
	// Formula is now the error formula!
	// Convert to cnf, but first let's compress it

	// Formula = Aig_ManDupSimpleDfs(Formula);	

	// might need to compress
	// Formula = compressAig(Formula);
	Aig_ManCleanup(Formula);

	// printAig(Formula);

	assert(Aig_ManCoNum(Formula) == 1);
	auto err_cnf = Cnf_Derive(Formula, 0);

	Aig_ManStop(Formula);
	return err_cnf;
}

Cnf_Dat_t* getConflictFormulaCNF(Aig_Man_t* SAig, int idx) {
	Aig_Man_t* formula = Aig_ManDupSimpleDfs(SAig);

	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (int i = 0; i < numY; i++) {
		if (i < idx) {
			vars.push_back(varsYS[i]);
			funcs.push_back(Aig_ManConst1(formula));
			vars.push_back(varsYS[i] + numOrigInputs);
			funcs.push_back(Aig_ManConst1(formula));
		}
		else if (i > idx) {
			vars.push_back(varsYS[i] + numOrigInputs);
			funcs.push_back(Aig_Not(Aig_ManCi(formula, varsYS[i])));
		}
	}

	Aig_Obj_t* pObj = Aig_ManCo(formula, 0)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 0), pObj);
	Aig_ManCleanup(formula);

	// og : 11
	Aig_ObjCreateCo(formula, Aig_ManCo(formula, 0)->pFanin0); // 10
	Aig_ObjCreateCo(formula, Aig_ManCo(formula, 0)->pFanin0); // 01

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst1(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst1(formula));

	pObj = Aig_ManCo(formula, 0)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 0), pObj);

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst1(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst0(formula));

	pObj = Aig_ManCo(formula, 1)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 1), Aig_Not(pObj));

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst0(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst1(formula));

	pObj = Aig_ManCo(formula, 2)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 2), Aig_Not(pObj));

	// formula = compressAig(formula);
	Aig_ManCleanup(formula);
	auto ans = Cnf_Derive(formula, 0);
	Aig_ManStop(formula);

	return ans;
}

Cnf_Dat_t* getConflictFormulaCNF2(Aig_Man_t* SAig, int idx) {
		Aig_Obj_t* pObj;
	Aig_Man_t* formula = Aig_ManDupSimpleDfs(SAig);

	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	// conflictFormulaOrig AND F

	// X, Y, Ybar

	// f1 <- X, Y, \bar{y_k}
	// F <- X, Y'

	for (int i = 0; i < numY; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_ManCi(formula, varsYS[i] + numOrigInputs));
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(formula, varsYS[i] + numOrigInputs)));
	}

	// COs are as follows - 0 : F, 1 : 11, 2 : 10, 3 : 01

	Aig_ObjCreateCo(formula, Aig_ManCo(formula, 0)->pFanin0);

	pObj = Aig_ManCo(formula, 0)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 0), pObj);

	vars.clear();
	funcs.clear();

	for (int i = 0; i < numY; i++) {
		if (i < idx) {
			vars.push_back(varsYS[i]);
			funcs.push_back(Aig_ManConst1(formula));
			vars.push_back(varsYS[i] + numOrigInputs);
			funcs.push_back(Aig_ManConst1(formula));
		}
		else if (i > idx) {
			vars.push_back(varsYS[i] + numOrigInputs);
			funcs.push_back(Aig_Not(Aig_ManCi(formula, varsYS[i])));
		}
	}

	pObj = Aig_ManCo(formula, 1)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 1), pObj);

	// now create two other COs for 10, 01
	Aig_ObjCreateCo(formula, Aig_ManCo(formula, 1)->pFanin0);
	Aig_ObjCreateCo(formula, Aig_ManCo(formula, 1)->pFanin0);

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst1(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst1(formula));

	pObj = Aig_ManCo(formula, 1)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 1), pObj);

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst1(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst0(formula));

	pObj = Aig_ManCo(formula, 2)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 2), Aig_Not(pObj));

	vars.clear();
	funcs.clear();
	vars.push_back(varsYS[idx]);
	funcs.push_back(Aig_ManConst0(formula));
	vars.push_back(varsYS[idx] + numOrigInputs);
	funcs.push_back(Aig_ManConst1(formula));

	pObj = Aig_ManCo(formula, 3)->pFanin0;
	pObj = Aig_ComposeVec(formula, pObj, funcs, vars);
	Aig_ObjPatchFanin0(formula, Aig_ManCo(formula, 3), Aig_Not(pObj));

	// formula = compressAig(formula);
	Aig_ManCleanup(formula);
	auto ans = Cnf_Derive(formula, 0);
	Aig_ManStop(formula);

	return ans;
}

lbool solveAndModel(Aig_Man_t* SAig, Cnf_Dat_t* cnf) {
	assert(pi.idx < numY);
	
	sat_solver* solver = sat_solver_new();
	bool allOk = addCnfToSolver(solver, cnf);
	if (!allOk) {
		// cout << "No conflict!" << endl;

		Cnf_DataFree(cnf);
		sat_solver_delete(solver);
		return l_False;
	}

	clock_t sat_start = clock();
	lbool status = sat_solver_solve(solver, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0,
						(ABC_INT64_T)0, (ABC_INT64_T)0);
	clock_t sat_end = clock();

	sat_solving_time += double(sat_end - sat_start) / CLOCKS_PER_SEC;

	if (status == l_True) {

		// have to add 1 to node IDs since first node is always the const 1 node

		vector<int> vars(numX+numY);
		for (int i = 0; i < numX; i++) {
			vars[i] = cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))];
		}
		for (int i = 0; i < numY; i++) {
			vars[i+numX] = cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))];
		}

		int* model = Sat_SolverGetModel(solver, vars.data(), vars.size());

		pi.X.assign(model, model + numX);
		pi.Y.assign(model + numX, model + numX + numY);

		free(model);
	}

	Cnf_DataFree(cnf);
	sat_solver_delete(solver);

	return status;
}


int main(int argc, char** argv) {
    Abc_Obj_t* pAbcObj;
	Aig_Obj_t* pAigObj;
	map<string, int> name2IdF;
	map<int, string> id2NameF;
	int i, j;
	Abc_Ntk_t *FNtk;
	Aig_Man_t *FAig, *SAig;
	int compressFreq = 4;

    parseOptions(argc, argv);

    FNtk = getNtk(options.benchmark,true);
    FAig = Abc_NtkToDar(FNtk, 0, 0);

    Aig_ManCleanup(FAig);

	numOrigInputs = Aig_ManCiNum(FAig);

	populateVars(FNtk, options.varsOrder,
				varsXF, varsXS,
				varsYF, varsYS,
				name2IdF, id2NameF);
	Abc_NtkDelete(FNtk);

	SAig = NormalToPositive(FAig);
	Aig_ManStop(FAig);

	SAig = compressAigByNtkMultiple(SAig, 3);

	auto F = Aig_ManDupSimpleDfs(SAig);

	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (auto i : varsYS) {
		vars.push_back(i + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(F, i)));
	}

	Aig_ObjPatchFanin0(F, Aig_ManCo(F, 0), Aig_ComposeVec(F, Aig_ManCo(F, 0)->pFanin0, funcs, vars));
	Aig_ManCleanup(F);

	m_FCnf = Cnf_Derive(F, 0);

	assert(m_FCnf != NULL);

	printAig(SAig);

	int initSize = Aig_ManObjNum(SAig);

	clock_t start = clock();
	pi.idx = 0;
	i = 0;
	if (options.conflictCheck == 0) {
		while (true) {
			Cnf_Dat_t* err_cnf = getErrorFormulaCNF(SAig);
			auto ans = solveAndModel(SAig, err_cnf);
			if (ans == l_False) {
				break;
			}
			assert(ans != l_Undef);
			i += 1;

			double x = clock();
			repair(SAig);

			if (it % 100 == 0) {
				SAig = compressAig(SAig);
			}
			Aig_ManCleanup(SAig);
			repair_time += (clock() - x) / CLOCKS_PER_SEC;
		}
	} 
	else {
		while (pi.idx < numY) {
			Cnf_Dat_t *conflict_cnf;

			if (options.conflictCheck == 1) {
				conflict_cnf = getConflictFormulaCNF(SAig, pi.idx);
			}
			else {
				conflict_cnf = getConflictFormulaCNF2(SAig, pi.idx);
				// this is the newer conflict formula
			}

			auto ans = solveAndModel(SAig, conflict_cnf);

			if (ans == l_False) {
				pi.idx += 1;
			}
			else {
				i += 1;
				assert(ans != l_Undef);

				#ifdef DEBUG
					assert(isConflict(SAig, pi.idx));
				#endif

				double x = clock();

				repair(SAig);
				repair_time += (clock() - x) / CLOCKS_PER_SEC;
			}
			// should we cache recent counter-examples, LRU caching maybe?

			if (it % compressFreq == 0) {
				// perform compression every once in a while

				int inCnt = Aig_ManObjNum(SAig);
				SAig = compressAig(SAig);
				int outCnt = Aig_ManObjNum(SAig);

				if ((outCnt >= int(inCnt*0.99)) && (compressFreq < 1000)) {
					// non substantial benefits are not useful
					compressFreq *= 2;
				}
	
				#ifdef DEBUG
					assert(Aig_IsPositive(SAig));
				#endif
			}
			Aig_ManCleanup(SAig);

			if ( (clock() - start) / CLOCKS_PER_SEC > options.timeOut) {
				break;
			}
		}	
	}

	cout << "DONE!" << endl;

	assert(Aig_ManCheck(SAig));
	SAig = compressAigByNtkMultiple(SAig, 3);
	printAig(SAig);

	cout << "Initial formula had : " << initSize << " nodes" << endl;
	cout << "Final formula has : " << Aig_ManObjNum(SAig) << " nodes"<< endl;

	cout << "Took " << it << " iterations of algorithm : " << i << " number of counterexamples, " << pi.idx << " outputs repaired out of " << numY << " outputs" << endl;

	cout << double(clock() - start)/CLOCKS_PER_SEC << " seconds" << endl;
	
	// Aig_ManDumpVerilog(SAig, (char*) (options.outFName).c_str());
	Aig_ManStop(SAig);

	return 0;
}

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////
#include "helper.h"
#include "nnf.h"

using namespace std;

////////////////////////////////////////////////////////////////////////
///                           GLOBALS                                ///
////////////////////////////////////////////////////////////////////////
vector<int> varsSInv;
vector<int> varsXF, varsXS;
vector<int> varsYF, varsYS; // to be eliminated.
vector<int> varsCNF;		// Aig Node ID to Cnf Var mapping!
vector<int> AigToCNF;
ConflictCounterEx pi;
int numOrigInputs = 0, numX = 0, numY = 0;
vector<string> varsNameX, varsNameY;
Abc_Frame_t *pAbc = NULL;
sat_solver *conflictSolver = NULL;
bool alreadyFalse = false;
Cnf_Dat_t *FCnf = NULL;

chrono_steady_time helper_time_measure_start;

double repairTime, conflictCnfTime, satSolvingTime, unateTime, compressTime;

int it = 0;
int init_unates = 0;
int tot_unates = 0;

int main(int argc, char **argv)
{
	map<string, int> name2IdF;
	map<int, string> id2NameF;
	int i;
	Abc_Ntk_t *FNtk;
	Aig_Man_t *FAig, *SAig, *stoSAig;
	Cnf_Dat_t *conflict_cnf;
	int compressFreq = 5;
	int repaired = 0;

	parseOptions(argc, argv);

	FNtk = getNtk(options.benchmark, true);
	FAig = Abc_NtkToDar(FNtk, 0, 0);

	Aig_ManCleanup(FAig);

	numOrigInputs = Aig_ManCiNum(FAig);

	populateVars(FNtk, options.varsOrder,
				 varsXF, varsXS,
				 varsYF, varsYS,
				 name2IdF, id2NameF);

	varsCNF = vector<int>(numX + numY, -1);
	AigToCNF = vector<int>(numX + numY + 1, -1);
	conflictSolver = sat_solver_new();
	///////////////////////////////////////
	// INITIALIZATION
	//////////////////////////////////////

	vector<int> unates(numY, -1);

	SAig = NormalToPositive(FAig);
	Aig_ManStop(FAig);
	SAig = compressAig(SAig);

	int initSize = Aig_ManObjNum(SAig);

	clock_t start = clock();
	pi.idx = 0;
	i = 0;

	if (options.dynamicOrdering)
	{
		calcLeastOccurrenceSAig(SAig, pi.idx);
	}

	if (options.unate)
	{
		TIMED(unateTime, int cnt = checkUnate(SAig, unates, pi.idx);
			  init_unates += cnt;
			  tot_unates += cnt;
			  setUnatesSaig(SAig, unates);)
	}

	FAig = PositiveToNormalWithNeg(SAig);
	FCnf = Cnf_Derive(FAig, 0);
	Aig_ManStop(FAig);

	TIMED(
		conflictCnfTime,
		if (options.conflictCheck == 1) {
			conflict_cnf = getConflictFormulaCNF(SAig, pi.idx);
		} else {
			conflict_cnf = getConflictFormulaCNF2(SAig, pi.idx);
			// this is the newer conflict formula
		})

	alreadyFalse = !addCnfToSolver(conflictSolver, conflict_cnf);
	for (int i = 0; i < numX; i++)
	{
		varsCNF[i] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))];
	}
	for (int i = 0; i < numY; i++)
	{
		varsCNF[i + numX] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))];
	}
	Cnf_DataFree(conflict_cnf);

	int begSize = Aig_ManObjNum(SAig);
	stoSAig = Aig_ManDupSimpleDfs(SAig);

	if (options.conflictCheck == 0)
	{
		assert(false);
		while (true)
		{
			Cnf_Dat_t *err_cnf = getErrorFormulaCNF(SAig);
			auto ans = solveAndModel(SAig);
			if (ans == l_False)
			{
				break;
			}
			assert(ans != l_Undef);
			i += 1;

			repair(SAig);

			if (it % 100 == 0)
			{
				SAig = compressAig(SAig);
			}
			Aig_ManCleanup(SAig);
		}
	}
	else
	{
		while ((pi.idx < numY) && ((clock() - start) / CLOCKS_PER_SEC <= options.timeout))
		{

			// disabling unates for now with new conflict formula!
			if (options.unate && (unates[pi.idx] != -1))
			{
				pi.idx++;
				if (options.dynamicOrdering)
				{
					calcLeastOccurrenceSAig(SAig, pi.idx);
				}

				int cnt = checkUnate(SAig, unates, pi.idx);
				tot_unates += cnt;
				setUnatesSaig(SAig, unates);
				Aig_ManCleanup(SAig);

				if (cnt > 0)
				{
					FAig = PositiveToNormalWithNeg(SAig);
					FCnf = Cnf_Derive(FAig, 0);
					Aig_ManStop(FAig);

					sat_solver_restart(conflictSolver);

					TIMED(
						conflictCnfTime,
						if (options.conflictCheck == 1) {
							conflict_cnf = getConflictFormulaCNF(SAig, pi.idx);
						} else {
							conflict_cnf = getConflictFormulaCNF2(SAig, pi.idx);
							// this is the newer conflict formula
						})

					alreadyFalse = !addCnfToSolver(conflictSolver, conflict_cnf);
					for (int i = 0; i < numX; i++)
					{
						varsCNF[i] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))];
					}
					for (int i = 0; i < numY; i++)
					{
						varsCNF[i + numX] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))];
					}
					Cnf_DataFree(conflict_cnf);
				}

				begSize = Aig_ManObjNum(SAig);
				if (options.useShannon)
				{
					Aig_ManStop(stoSAig);
					stoSAig = Aig_ManDupSimpleDfs(SAig);
				}
			}
			else
			{

				TIMED(satSolvingTime, lbool ans = solveAndModel(SAig);)

				if (ans == l_False)
				{
					repaired++;
					pi.idx++;

					if (pi.idx >= numY)
					{
						break;
					}

					sat_solver_restart(conflictSolver);

					TIMED(
						conflictCnfTime,
						if (options.conflictCheck == 1) {
							conflict_cnf = getConflictFormulaCNF(SAig, pi.idx);
						} else {
							conflict_cnf = getConflictFormulaCNF2(SAig, pi.idx);
							// this is the newer conflict formula
						})

					alreadyFalse = !addCnfToSolver(conflictSolver, conflict_cnf);
					for (int i = 0; i < numX; i++)
					{
						varsCNF[i] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsXS[i]))];
					}
					for (int i = 0; i < numY; i++)
					{
						varsCNF[i + numX] = AigToCNF[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))] = conflict_cnf->pVarNums[Aig_ObjId(Aig_ManCi(SAig, varsYS[i]))];
					}
					Cnf_DataFree(conflict_cnf);

					if (options.dynamicOrdering)
					{
						calcLeastOccurrenceSAig(SAig, pi.idx);
					}

					// if (options.unate) {
					// 	int cnt = checkUnate(SAig, unates);
					// 	tot_unates += cnt;
					// 	setUnatesSaig(SAig, unates);
					// 	Aig_ManCleanup(SAig);

					// 	if (cnt > 0) {
					// 		FAig = PositiveToNormalWithNeg(SAig);
					// 		FCnf = Cnf_Derive(FAig, 0);
					// 		Aig_ManStop(FAig);
					// 	}
					// }

					begSize = Aig_ManObjNum(SAig);
					if (options.useShannon)
					{
						Aig_ManStop(stoSAig);
						stoSAig = Aig_ManDupSimpleDfs(SAig);
					}
				}
				else
				{
					i++;
					assert(ans != l_Undef);

					#ifdef DEBUG
						assert(isConflict(SAig, pi.idx));
					#endif

					if (options.useShannon && (Aig_ManObjNum(SAig) >= int(1.75 * begSize)))
					{
						doShannonExp(stoSAig, pi.idx);
						Aig_ManStop(SAig);
						SAig = Aig_ManDupSimpleDfs(stoSAig);
						// known to fix all conflicts!
						// next SAT check would yield false now!
					}
					else
					{
						TIMED(repairTime, repair(SAig))
					}

					TIMED(
						compressTime, if (it % compressFreq == 0) {
							// perform compression every once in a while

							int inCnt = Aig_ManObjNum(SAig);
							SAig = compressAig(SAig);
							int outCnt = Aig_ManObjNum(SAig);

							if ((outCnt >= int(inCnt * 0.99)) && (compressFreq < 1000))
							{
								// non substantial benefits are not useful
								compressFreq *= 2;
							}
							else
							{
								// compression costs more for larger Aigs
								compressFreq = int(compressFreq * 1.2);
							}

#ifdef DEBUG
							assert(Aig_IsPositive(SAig));
#endif
						}

						Aig_ManCleanup(SAig);)
				}
			}
		}
	}

	cout << "DONE!" << endl;

	assert(Aig_ManCheck(SAig));
	SAig = compressAigByNtkMultiple(SAig, 3);
	// printAig(SAig);

	cout << "Initial formula had : " << initSize << " nodes" << endl;
	cout << "Final formula has : " << Aig_ManObjNum(SAig) << " nodes" << endl;

	cout << "Unates : " << init_unates << " initially out of " << tot_unates << endl;

	cout << "Took " << it << " iterations of algorithm : " << i << " number of counterexamples, " << repaired << " outputs repaired out of non-unate " << numY - tot_unates << " outputs" << endl;

	cout << double(clock() - start) / CLOCKS_PER_SEC << " seconds" << endl;
	cout << repairTime << " " << conflictCnfTime << " " << satSolvingTime << " " << unateTime << " " << compressTime << endl;

	// Aig_ManDumpVerilog(SAig, (char*) (options.outFName).c_str());
	Aig_ManStop(SAig);
	Aig_ManStop(stoSAig);
	Abc_NtkDelete(FNtk);

	return 0;
}
#include "helper.h"
// #include "formula.h"
#include "nnf.h"
// #include <boost/functional/hash.hpp>

using namespace ABC_NAMESPACE;

vector<int> nodeIdtoN;
cxxopts::Options optParser("cex-kc");
optionStruct options;

std::ostream& operator<<(std::ostream& os, const ConflictCounterEx& cce) {
	os << "X: ";
	for (int x : cce.X) {
		os << x << " ";
	}
	os << "\n";
	os << "Y: ";
	for (int y : cce.Y) {
		os << y << " ";
	}
	os << "\n";
	return os;
}

////////////////////////////////////////////////////////////////////////
///                      HELPER FUNCTIONS                            ///
////////////////////////////////////////////////////////////////////////
void parseOptions(int argc, char * argv[]) {
	optParser.positional_help("");
	optParser.add_options()
		("b, benchmark", "Specify the benchmark (required)", cxxopts::value<string>(options.benchmark), "FILE")
		("v, varsOrder", "Specify the variable ordering", cxxopts::value<string>(options.varsOrder), "FILE")
		("out", "Out folder path", cxxopts::value<string>(options.outFolderPath), "PATH")
		("c, conflictCheck", "Specifies the conflict check method", cxxopts::value<int>(options.conflictCheck)->default_value("2"))
		("r, rectifyProc", "Specifies the rectification procedure employed", cxxopts::value<int>(options.rectifyProc)->default_value("3"))
		("d, depth", "Specifies the depth of the cut nodes", cxxopts::value<int>(options.depth)->default_value("10"))
		("t, timeOut", "Specifies the timeout used", cxxopts::value<int>(options.timeout)->default_value("3600"))
		("a, allIndices", "Specifies whether all indices are fixed for a counter-example at once", cxxopts::value<bool>(options.fixAllIndices))
		("u, unate", "Specifies whether to use unates or not", cxxopts::value<bool>(options.unate))
		("s, shannon", "Use shannon expansion", cxxopts::value<bool>(options.useShannon))
		("unateTimeout", "Timeout for fixed-point unate computation", cxxopts::value<int>(options.unateTimeout)->default_value("600"))
		("q, allowUnivQuantify", "Allows universal quantification in cut choices", cxxopts::value<bool>(options.allowUnivQuantify))
		("o, dynamicOrdering", "Use dynamic ordering", cxxopts::value<bool>(options.dynamicOrdering))
		("f, useFastCnf", "Use Fast CNF", cxxopts::value<bool>(options.useFastCnf))
		("h, help", "Print this help");
		
	// optParser.parse_positional(vector<string>({"benchmark", "varsOrder"}));
	auto result = optParser.parse(argc, argv);

	if (result.count("help")) {
		cout << optParser.help({"", "Group"}) << std::endl;
		exit(0);
	}

	if (!result.count("benchmark"))
	{
		cerr << endl
			 << "Error: Benchmark not specified" << endl
			 << endl;
		cout << optParser.help({"", "Group"}) << endl;
		exit(0);
	}

	if (!result.count("varsOrder"))
	{
		cerr << endl
			 << "Error: Ordering not specified" << endl
			 << endl;
		cout << optParser.help({"", "Group"}) << endl;
		exit(0);
	}

	if (!result.count("out"))
	{
		cerr << endl
			 << "Error: Output Folder Path not specified" << endl
			 << endl;
		cout << optParser.help({"", "Group"}) << endl;
		exit(0);
	}

	options.unateTimeout = min(options.unateTimeout, options.timeout / 2);

	// optimization to compute conflict cnf, disabling things for now!
	// options.unate = false;
	// options.useShannon = false;
	// options.dynamicOrdering = false;
	options.conflictCheck = 2; // only allow 1 or 2 for now!
}

int CommandExecute(Abc_Frame_t* pAbc, string cmd) {
	int ret = Cmd_CommandExecute(pAbc, (char*) cmd.c_str());
	if(ret) {
		cout << "Cannot execute command \""<<cmd<<"\".\n";
		return 1;
	}
	else
		return ret;
}

vector<string> tokenize( const string& p_pcstStr, char delim )  {
	vector<string> tokens;
	stringstream   mySstream( p_pcstStr );
	string         temp;
	while( getline( mySstream, temp, delim ) ) {
		if(temp!="")
			tokens.push_back( temp );
	}
	return tokens;
}

string type2String(Aig_Type_t t) {
	switch(t) {
		case AIG_OBJ_NONE: return "AIG_OBJ_NONE";
		case AIG_OBJ_CONST1: return "AIG_OBJ_CONST1";
		case AIG_OBJ_CI: return "AIG_OBJ_CI";
		case AIG_OBJ_CO: return "AIG_OBJ_CO";
		case AIG_OBJ_BUF: return "AIG_OBJ_BUF";
		case AIG_OBJ_AND: return "AIG_OBJ_AND";
		case AIG_OBJ_EXOR: return "AIG_OBJ_EXOR";
		default: return "";
	}
}

bool Equate(sat_solver *pSat, int varA, int varB) {
	lit Lits[3];
	assert(varA!=0 && varB!=0);
	bool retval = true;
	// A -> B
	Lits[0] = toLitCond( abs(-varA), -varA<0 );
	Lits[1] = toLitCond( abs(varB), varB<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 )) {
		cerr << "Warning: In addCnfToSolver, sat_solver_addclause failed" << endl;
		retval = false;
	}

	// B -> A
	Lits[0] = toLitCond( abs(varA), varA<0 );
	Lits[1] = toLitCond( abs(-varB), -varB<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 )) {
		cerr << "Warning: In addCnfToSolver, sat_solver_addclause failed" << endl;
		retval = false;
	}
	return retval;
}

bool Xor(sat_solver *pSat, int varA, int varB) {
	lit Lits[3];
	assert(varA!=0 && varB!=0);
	bool retval = true;

	// A or B
	Lits[0] = toLitCond( abs(varA), varA<0 );
	Lits[1] = toLitCond( abs(varB), varB<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 )) {
		cerr << "Warning: In addCnfToSolver, sat_solver_addclause failed" << endl;
		retval = false;
	}

	// -A or -B
	Lits[0] = toLitCond( abs(-varA), -varA<0 );
	Lits[1] = toLitCond( abs(-varB), -varB<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 )) {
		cerr << "Warning: In addCnfToSolver, sat_solver_addclause failed" << endl;
		retval = false;
	}
	return retval;
}

Abc_Ntk_t*  getNtk(string pFileName, bool fraig) {
	string cmd, initCmd, varsFile, line;
	set<int> varsX;
	set<int> varsY; // To Be Eliminated
	map<string, int> name2Id;

	initCmd = "balance; rewrite -l; refactor -l; balance; rewrite -l; \
						rewrite -lz; balance; refactor -lz; rewrite -lz; balance";

	pAbc = Abc_FrameGetGlobalFrame();

	cmd = "read " + pFileName;
	if (CommandExecute(pAbc, cmd)) { // Read the AIG
		return NULL;
	}
	cmd = fraig?initCmd:"balance";
	if (CommandExecute(pAbc, cmd)) { // Simplify
		return NULL;
	}

	Abc_Ntk_t* pNtk =  Abc_FrameReadNtk(pAbc);
	// Aig_Man_t* pAig = Abc_NtkToDar(pNtk, 0, 0);
	return pNtk;
}

/** Function
 * Reads the varsFile and populates essential maps and vectors using FNtk, nnf.
 * @param FNtk      [in]
 * @param nnf       [in]
 * @param varsFile  [in]
 * @param varsXF    [out]   maps x_i  to Id in FAig
 * @param varsXS    [out]   maps x_i  to Id in SAig
 * @param varsYF    [out]   maps y_i  to Id in FAig
 * @param varsYS    [out]   maps y_i  to Id in SAig
 * @param name2IdF  [out]   maps name to Id in FAig
 * @param Id2nameF  [out]   maps Id in FAig to name
 */
void populateVars(Abc_Ntk_t* FNtk, string varsFile,
	vector<int>& varsXF, vector<int>& varsXS,
	vector<int>& varsYF, vector<int>& varsYS,
	map<string,int>& name2IdF, map<int,string>& id2NameF) {

	int i;
	Abc_Obj_t* pPi;
	string line;

	Abc_NtkForEachCi( FNtk, pPi, i ) {
		string variable_name = Abc_ObjName(pPi);
		name2IdF[variable_name] = pPi->Id;
	}
	#ifdef DEBUG_CHUNK
		OUT( "Prim Inputs F:" );
		Abc_NtkForEachCi( FNtk, pPi, i ) {
			OUT( i << ": " << pPi->Id << ", " << Abc_ObjName(pPi) );
		}
	#endif
	for(auto it:name2IdF)
		id2NameF[it.second] = it.first;

	OUT( "Reading vars to elim..." );
	auto name2IdFTemp = name2IdF;
	ifstream varsStream(varsFile);
	if(!varsStream.is_open())
		cout << "Var File " + varsFile + " does not exist!" << endl;
	assert(varsStream.is_open());
	while (getline(varsStream, line)) {
		if(line != "") {
			auto it = name2IdFTemp.find(line);
			assert(it != name2IdFTemp.end());
			varsYF.push_back(it->second - 1); // have to do -1
			name2IdFTemp.erase(it);
		}
	}
	for(auto it:name2IdFTemp) {
		varsXF.push_back(it.second - 1);
	}

		
	OUT( "Populating varsXS varsYS..." );
	for(auto it : varsXF) {
		// cout << "nnf.getNewAigNodeID: " << it << "->" << nnf.getNewAigNodeID(it) << endl;
		// varsXS.push_back(nnf.getNewAigNodeID(it));
		varsXS.push_back(it);
		assert(varsXS.back() != -1);
	}
	for(auto it : varsYF) {
		// cout << "nnf.getNewAigNodeID: " << it << "->" << nnf.getNewAigNodeID(it) << endl;
		// varsYS.push_back(nnf.getNewAigNodeID(it));
		varsYS.push_back(it);
		assert(varsYS.back() != -1);
	}

	numX = varsXS.size();
	numY = varsYS.size();

	if(numY <= 0) {
		cout << "Var File " + varsFile + " is empty!" << endl;
		cerr << "Var File " + varsFile + " is empty!" << endl;
		cerr << "  No Skolem functions to generate " << endl;
	//	exit (1);
	}
	assert(numY > 0);


	// for (int i = 0; i < numX; ++i) {
	// 	varsSInv[varsXS[i]] = i;
	// 	varsSInv[numOrigInputs + varsXS[i]] = numOrigInputs + i;
	// }
	// for (int i = 0; i < numY; ++i) {
	// 	varsSInv[varsYS[i]] = numX + i;
	// 	varsSInv[numOrigInputs + varsYS[i]] = numOrigInputs + numX + i;
	// }
}

/** Function
 * Composes input variable in initiAig with @param one, returns resulting Aig_Obj
 * @param pMan      [in out]    Aig Manager
 * @param initAig   [in]        Specifies the head of function tree
 * @param varId     [in]        (>=1) Specifies the variable to be substituted
 * @param one       [in]       set to 1 if varId is to be substituted by 1
 */
Aig_Obj_t* Aig_SubstituteConst(Aig_Man_t* pMan, Aig_Obj_t* initAig, int varId, int one) {
	Aig_Obj_t* const1 = Aig_ManConst1(pMan);
	Aig_Obj_t* constf = (one? const1: Aig_Not(const1));
	Aig_Obj_t* currFI = Aig_ObjIsCo(Aig_Regular(initAig))? initAig->pFanin0: initAig;
	Aig_Obj_t* afterCompose = Aig_Compose(pMan, currFI, constf, varId-1);
	assert(!Aig_ObjIsCo(Aig_Regular(afterCompose)));
	return afterCompose;
}

/** Function
 * Composes input variable in initiAig with @param func, returns resulting Aig_Obj
 * @param pMan      [in out]    Aig Manager
 * @param initAig   [in]        Specifies the head of function tree
 * @param varId     [in]        (>=1) Specifies the variable to be substituted
 * @param func      [in]        Specifies the function that supplants the input
 */
Aig_Obj_t* Aig_Substitute(Aig_Man_t* pMan, Aig_Obj_t* initAig, int varId, Aig_Obj_t* func) {
	Aig_Obj_t* currFI = Aig_ObjIsCo(Aig_Regular(initAig))? initAig->pFanin0: initAig;
	func = Aig_ObjIsCo(Aig_Regular(func))? func->pFanin0: func;
	Aig_Obj_t* afterCompose = Aig_Compose(pMan, currFI, func, varId-1);
	assert(!Aig_ObjIsCo(Aig_Regular(afterCompose)));
	return afterCompose;
}

Aig_Obj_t* Aig_SubstituteVec(Aig_Man_t* pMan, Aig_Obj_t* initAig, vector<int> varIdVec,
	vector<Aig_Obj_t*>& funcVec) {
	Aig_Obj_t* currFI = Aig_ObjIsCo(Aig_Regular(initAig))? initAig->pFanin0: initAig;
	for (int i = 0; i < funcVec.size(); ++i) {
		funcVec[i] = Aig_ObjIsCo(Aig_Regular(funcVec[i]))? funcVec[i]->pFanin0: funcVec[i];
	}
	for (int i = 0; i < varIdVec.size(); ++i) {
		varIdVec[i]--;
	}
	Aig_Obj_t* afterCompose = Aig_ComposeVec(pMan, currFI, funcVec, varIdVec);

	assert(!Aig_ObjIsCo(Aig_Regular(afterCompose)));
	return afterCompose;
}

vector<Aig_Obj_t* > Aig_SubstituteVecVec(Aig_Man_t* pMan, Aig_Obj_t* initAig,
	vector<vector<Aig_Obj_t*> >& funcVecs) {
	Aig_Obj_t* currFI;
	currFI = Aig_ObjIsCo(Aig_Regular(initAig))? initAig->pFanin0: initAig;
	for(int i = 0; i < funcVecs.size(); i++) {
		for (int j = 0; j < funcVecs[i].size(); ++j) {
			funcVecs[i][j] = Aig_ObjIsCo(Aig_Regular(funcVecs[i][j]))? funcVecs[i][j]->pFanin0: funcVecs[i][j];
		}
	}
	vector<Aig_Obj_t* > afterCompose = Aig_ComposeVecVec(pMan, currFI, funcVecs);
	for (int i = 0; i < afterCompose.size(); ++i) {
		assert(!Aig_ObjIsCo(Aig_Regular(afterCompose[i])));
	}
	return afterCompose;
}

/** Function
 * Asserts varNum to have value val
 * @param pSat      [in out]     Sat Solver
 * @param varNum    [in]         Variable number
 * @param val       [in]         Value to be assigned
 */
bool addVarToSolver(sat_solver* pSat, int varNum, int val) {
	lit l = toLitCond(varNum, (int)(val==0)?1:0);
	if(!sat_solver_addclause(pSat, &l, &l+1)) {
	// if(!sat_solver_clause_new(pSat, &l, &l+1, 0)) {
		return false;
	}
	return true;
}

/** Function
 * Returns variable number (in CNF) of specified Co
 * @param cnf       [in]        Cnf
 * @param aig       [in]        Aig
 * @param nthCo     [in]        Co
 */
int getCnfCoVarNum(Cnf_Dat_t* cnf, Aig_Man_t* aig, int nthCo) {
	return cnf->pVarNums[((Aig_Obj_t *)Vec_PtrEntry(aig->vCos, nthCo))->Id];
}

/** Function
 * Returns a new sat solver variable
 * @param pSat      [in]        Sat Solver
 * @param GCnf      [in]        Cnf
 * @param GAig      [in]        Aig
 * @param r         [in]        vector of Aig_Obj_t*
 */
int sat_solver_newvar(sat_solver* s) {
	// TODO use sat_solver_addvar
	sat_solver_setnvars(s, s->size+1);
	return s->size-1;
}

/** Function
 * Returns a new sat solver variable denoting OR of lh and rh
 * @param pSat      [in]        Sat Solver
 * @param lh        [in]        lhs
 * @param rh        [in]        rhs
 */
lit OR(sat_solver* pSat, lit lh, lit rh) {
	int nv = sat_solver_newvar(pSat);

	lit Lits[4];
	assert(lh!=0 && rh!=0);
	// nv -> lh or rh
	Lits[0] = toLitCond( abs(-nv), -nv<0 );
	Lits[1] = toLitCond( abs(lh), lh<0 );
	Lits[2] = toLitCond( abs(rh), rh<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 3 ))
		assert(false);

	// lh -> nv
	Lits[0] = toLitCond( abs(-lh), -lh<0 );
	Lits[1] = toLitCond( abs(nv), nv<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 ))
		assert(false);

	// rh -> nv
	Lits[0] = toLitCond( abs(-rh), -rh<0 );
	Lits[1] = toLitCond( abs(nv), nv<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 ))
		assert(false);

	return nv;
}

/** Function
 * Returns a new sat solver variable denoting AND of lh and rh
 * @param pSat      [in]        Sat Solver
 * @param lh        [in]        lhs
 * @param rh        [in]        rhs
 */
lit AND(sat_solver* pSat, lit lh, lit rh) {
	int nv = sat_solver_newvar(pSat);

	lit Lits[4];
	assert(lh!=0 && rh!=0);
	// lh and rh -> nv
	Lits[0] = toLitCond( abs( nv),  nv<0 );
	Lits[1] = toLitCond( abs(-lh), -lh<0 );
	Lits[2] = toLitCond( abs(-rh), -rh<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 3 ))
		assert(false);

	// nv -> lh
	Lits[0] = toLitCond( abs( lh),  lh<0 );
	Lits[1] = toLitCond( abs(-nv), -nv<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 ))
		assert(false);

	// nv -> rh
	Lits[0] = toLitCond( abs( rh),  rh<0 );
	Lits[1] = toLitCond( abs(-nv), -nv<0 );
	if(!sat_solver_addclause( pSat, Lits, Lits + 2 ))
		assert(false);

	return nv;
}

/** Function
 * Adds CNF Formula to Solver
 * @param pSat      [in]        Sat Solver
 * @param cnf       [in]        Cnf Formula
 */
bool addCnfToSolver(sat_solver* pSat, Cnf_Dat_t* cnf) {
	bool retval = true;
	sat_solver_setnvars(pSat, sat_solver_nvars(pSat) + cnf->nVars);
	for (int i = 0; i < cnf->nClauses; i++)
		if (!sat_solver_addclause(pSat, cnf->pClauses[i], cnf->pClauses[i+1])) {
			retval = false;
		}
	return retval;
}

/** Function
 * This evaluates all nodes of the Aig using values from cex in DFS order
 * The value of the node is stored in pObj->iData
 * @param formula   [in]        Aig Manager
 * @param cex       [in]        counter-example
 */
void evaluateAig(Aig_Man_t* formula, const vector<int> &cex) {
	int i;
	Aig_Obj_t* pObj;

	for (int i = 0; i < numX; ++i) {
		Aig_ManCi(formula, varsXS[i])->iData = cex[i];
	}
	for (int i = 0; i < numY; ++i) {
		Aig_ManCi(formula, varsYS[i])->iData = cex[numX + i];
	}
	for (int i = 0; i < numX; ++i) {
		Aig_ManCi(formula, varsXS[i]+numOrigInputs)->iData = cex[i+numOrigInputs];
	}
	for (int i = 0; i < numY; ++i) {
		Aig_ManCi(formula, varsYS[i]+numOrigInputs)->iData = cex[i+numX+numOrigInputs];
	}
	Aig_ManObj(formula,0)->iData = 1;

	Vec_Ptr_t* nodes = Aig_ManDfsAll(formula);

	Vec_PtrForEachEntry(Aig_Obj_t*, nodes, pObj, i) {
		if (Aig_ObjId(pObj) > 2*numOrigInputs) {
			pObj->iData = 1;
			int ld, rd;
			if(Aig_ObjFanin0(pObj) != NULL) {
				ld = Aig_ObjFanin0(pObj)->iData;
				if(Aig_IsComplement(pObj->pFanin0))
					ld = 1 - ld;

				pObj->iData *= ld;
			}
			if(Aig_ObjFanin1(pObj) != NULL) {
				rd = Aig_ObjFanin1(pObj)->iData;
				if(Aig_IsComplement(pObj->pFanin1))
					rd = 1 - rd;

				pObj->iData *= rd;
			}
		}
	}

	Vec_PtrFree(nodes);
	return;
}

/** Function
 * This evaluates all leaves of the Aig using values from cex
 * The value of the node is stored in pObj->iData
 * @param formula   [in]        Aig Manager
 * @param cex       [in]        counter-example
 */
void evaluateXYLeaves(Aig_Man_t* formula, const vector<int> &cex) {
	for (int i = 0; i < numX; ++i) {
		Aig_ManObj(formula, varsXS[i])->iData = cex[i];
		Aig_ObjSetTravIdCurrent(formula, Aig_ManObj(formula, varsXS[i]));
	}
	for (int i = 0; i < numY; ++i) {
		Aig_ManObj(formula, varsYS[i])->iData = cex[numX + i];
		Aig_ObjSetTravIdCurrent(formula, Aig_ManObj(formula, varsYS[i]));
	}

	// Setting Const1
	Aig_ManObj(formula,0)->iData = 1;
	Aig_ObjSetTravIdCurrent(formula, Aig_ManObj(formula,0));
}

/** Function
 * This evaluates and returns head using values from leaves
 * The value of the leaves is stored in pObj->iData
 * @param formula   [in]        Aig Manager
 * @param head      [in]        node to be evaluated
 */
bool evaluateAigAtNode(Aig_Man_t* formula, Aig_Obj_t*head) {

	int isComplement = Aig_IsComplement(head);
	head = Aig_Regular(head);

	if(Aig_ObjIsTravIdCurrent(formula, head)) {
		assert(head->iData == 0 or head->iData == 1);
		return (bool) isComplement xor head->iData;
	}

	if(Aig_ObjIsCo(head)) {
		Aig_ObjSetTravIdCurrent(formula,head);
		head->iData = (int) evaluateAigAtNode(formula, Aig_ObjChild0(head));
		return (bool) isComplement xor head->iData;
	}

	bool lc, rc;
	lc = evaluateAigAtNode(formula, Aig_ObjChild0(head));
	if(lc)
		rc = evaluateAigAtNode(formula, Aig_ObjChild1(head));
	else
		rc = true;

	head->iData = (int) (lc and rc);
	Aig_ObjSetTravIdCurrent(formula,head);
		return (bool) isComplement xor head->iData;
}

/** Function
 * Finds an element in coObjs that satisfies cex.
 * Returns the object, if found. Else, returns NULL.
 * Call evaluateAig(...) before this function.
 * @param formula   [in]        Aig Manager
 * @param cex       [in]        Counterexample
 * @param coObjs    [in]        Co numbers in Aig Manager to check
 */
Aig_Obj_t* satisfiesVec(Aig_Man_t* formula, const vector<int>& cex, const vector<int>& coObjs, bool reEvaluate) {

	if(reEvaluate)
		evaluateAig(formula, cex);

	OUT("satisfiesVec...");

	Aig_Obj_t* currRet = NULL;
	int currMinSupp = std::numeric_limits<int>::max();

	for(int i = 0; i < coObjs.size(); i++) {
		OUT("Accessing Co "<<coObjs[i]<<" Id "<< Aig_ManCo(formula,coObjs[i])->Id);

		if(Aig_ManCo(formula,coObjs[i])->iData == 1) {
			OUT("Satisfied ID " << Aig_ManCo(formula,coObjs[i])->Id);

			vector<Aig_Obj_t* > tempSupp = Aig_SupportVec(formula, Aig_ManCo(formula,coObjs[i]));
			int tempSuppLen = tempSupp.size();

			if(tempSuppLen < currMinSupp) {
				currMinSupp = tempSuppLen;
				currRet = Aig_ManCo(formula,coObjs[i]);
			}
		}
	}
	OUT("Nothing satisfied");
	return currRet;
}

/** Function
 * Performs the function of the GENERALIZE routine as mentioned in FMCAD paper.
 * @param pMan      [in]        Aig Manager
 * @param cex       [in]        counter-example to be generalized
 * @param rl        [in]        Underaproximations of Cant-be sets
 */
Aig_Obj_t* generalize(Aig_Man_t*pMan, vector<int> cex, const vector<int>& rl) {
	return satisfiesVec(pMan, cex, rl, true);
}

/** Function
 * Recursively checks if inpNodeId lies in support of root.
 * @param pMan      [in]        Aig Manager
 * @param root      [in]        Function to check support of
 * @param inpNodeId [in]        ID of input variable to be checked
 * @param memo      [in]        A map for memoization
 */
bool Aig_Support_rec(Aig_Man_t* pMan, Aig_Obj_t* root, int inpNodeId, map<Aig_Obj_t*,bool>& memo) {
	if(root == NULL)
		return false;

	if(root->Id == inpNodeId)
		return true;

	if(Aig_ObjIsCi(root) || Aig_ObjIsConst1(root))
		return false;

	if(memo.find(root) != memo.end())
		return memo[root];

	bool result = false;
	if(Aig_ObjFanin0(root) != NULL)
		result = result || Aig_Support_rec(pMan, Aig_ObjFanin0(root), inpNodeId, memo);
	if(Aig_ObjFanin1(root) != NULL)
		result = result || Aig_Support_rec(pMan, Aig_ObjFanin1(root), inpNodeId, memo);

	memo[root] = result;
	return result;
}

/** Function
 * Checks if inpNodeId lies in support of root.
 * @param pMan      [in]        Aig Manager
 * @param root      [in]        Function to check support of
 * @param inpNodeId [in]        ID of input variable to be checked
 */
bool Aig_Support(Aig_Man_t* pMan, Aig_Obj_t* root, int inpNodeId) {
	map<Aig_Obj_t*,bool> memo;
	return Aig_Support_rec(pMan,Aig_Regular(root),inpNodeId,memo);
}

void Aig_ConeSupportVecAndMark_rec(Aig_Obj_t * pObj, set<Aig_Obj_t *>&retSupport) {
    assert(!Aig_IsComplement(pObj));
    if(pObj == NULL || Aig_ObjIsMarkA(pObj)) {
    	return;
    }
    else if(Aig_ObjIsConst1(pObj)) {
    	Aig_ObjSetMarkA(pObj);
    	return;
    }
    else if(Aig_ObjIsCi(pObj)) {
    	Aig_ObjSetMarkA(pObj);
		retSupport.insert(pObj);
		return;
    }

    Aig_ConeSupportVecAndMark_rec(Aig_ObjFanin0(pObj), retSupport);
    Aig_ConeSupportVecAndMark_rec(Aig_ObjFanin1(pObj), retSupport);

    assert(!Aig_ObjIsMarkA(pObj)); // loop detection
    Aig_ObjSetMarkA(pObj);
    return;
}

void Aig_ConeSupportVecUnmark_rec(Aig_Obj_t * pObj) {
    assert(!Aig_IsComplement(pObj));
    if(pObj == NULL || !Aig_ObjIsMarkA(pObj)) {
    	return;
    }
    else if(Aig_ObjIsConst1(pObj) || Aig_ObjIsCi(pObj)) {
    	Aig_ObjClearMarkA(pObj);
		return;
    }
    Aig_ConeSupportVecUnmark_rec(Aig_ObjFanin0(pObj));
    Aig_ConeSupportVecUnmark_rec(Aig_ObjFanin1(pObj));
    assert(Aig_ObjIsMarkA(pObj)); // loop detection
    Aig_ObjClearMarkA(pObj);
}

vector<Aig_Obj_t *> Aig_SupportVec(Aig_Man_t* pMan, Aig_Obj_t* root) {
    set<Aig_Obj_t *> retSupport;
    Aig_ConeSupportVecUnmark_rec(Aig_Regular(root));
    Aig_ConeSupportVecAndMark_rec(Aig_Regular(root), retSupport);
    Aig_ConeSupportVecUnmark_rec(Aig_Regular(root));
	return vector<Aig_Obj_t *>(retSupport.begin(), retSupport.end());
}

/** Function
 * Returns And of Aig1 and Aig2
 * @param pMan      [in]        Aig Manager
 * @param Aig1      [in]
 * @param Aig2      [in]
 */
Aig_Obj_t* Aig_AndAigs(Aig_Man_t* pMan, Aig_Obj_t* Aig1, Aig_Obj_t* Aig2) {
	Aig_Obj_t* lhs = Aig_ObjIsCo(Aig_Regular(Aig1))? Aig_Regular(Aig1)->pFanin0: Aig1;
	Aig_Obj_t* rhs = Aig_ObjIsCo(Aig_Regular(Aig2))? Aig_Regular(Aig2)->pFanin0: Aig2;
	return Aig_And(pMan, lhs, rhs);
}

/** Function
 * Returns Or of Aig1 and Aig2
 * @param pMan      [in]        Aig Manager
 * @param Aig1      [in]
 * @param Aig2      [in]
 */
Aig_Obj_t* Aig_OrAigs(Aig_Man_t* pMan, Aig_Obj_t* Aig1, Aig_Obj_t* Aig2) {
	Aig_Obj_t* lhs = Aig_ObjIsCo(Aig_Regular(Aig1))? Aig_Regular(Aig1)->pFanin0: Aig1;
	Aig_Obj_t* rhs = Aig_ObjIsCo(Aig_Regular(Aig2))? Aig_Regular(Aig2)->pFanin0: Aig2;
	return Aig_Or(pMan, lhs, rhs);
}

Aig_Obj_t* AND_rec(Aig_Man_t* SAig, vector<Aig_Obj_t* >& nodes, int start, int end) {
	// cout << "And_rec on start: " << start << " " << "end " << end << endl;
	assert(end > start);
	if(end == start+1)
		return nodes[start];

	int mid = (start+end)/2;
	Aig_Obj_t* lh = AND_rec(SAig, nodes, start, mid);
	Aig_Obj_t* rh = AND_rec(SAig, nodes, mid, end);
	Aig_Obj_t* nv = Aig_And(SAig, lh, rh);

	return nv;
}

Aig_Obj_t* newAND(Aig_Man_t* SAig, vector<Aig_Obj_t* >& nodes) {
	return AND_rec(SAig, nodes, 0, nodes.size());
}


Aig_Obj_t* projectPi(Aig_Man_t* pMan, const vector<int> &cex, const int m) {
	vector<Aig_Obj_t*> pi_m(numOrigInputs - m - 1);
	for(int i = 0; i < numX; i++) {
		pi_m[i] = (cex[i] == 1)?Aig_ManObj(pMan, varsXS[i]):Aig_Not(Aig_ManObj(pMan, varsXS[i]));
	}
	for(int i = m + 1; i < numY; i++) {
		pi_m[numX + i - m - 1] = (cex[numX + i] == 1)?Aig_ManObj(pMan, varsYS[i]):Aig_Not(Aig_ManObj(pMan, varsYS[i]));
	}
	return newAND(pMan, pi_m);
}

Aig_Obj_t* projectPiSmall(Aig_Man_t* pMan, const vector<int> &cex) {
	vector<Aig_Obj_t*> pi_m(numX);
	for(int i = 0; i < numX; i++) {
		pi_m[i] = (cex[i] == 1)?Aig_ManObj(pMan, varsXS[i]):Aig_Not(Aig_ManObj(pMan, varsXS[i]));
	}
	return newAND(pMan, pi_m);
}

/** Function
 * Compresses Aig using the compressAig() routine
 * Deletes SAig and returns a compressed version
 * @param SAig      [in]        Aig to be compressed
 */
Aig_Man_t* compressAig(Aig_Man_t* SAig) {
	// OUT("Cleaning up...");
	// int removed = Aig_ManCleanup(SAig);
	// cout << "Removed " << removed <<" nodes" << endl;

	Aig_Man_t* temp = SAig;
	// Dar_ManCompress2( Aig_Man_t * pAig, int fBalance,
	//                  int fUpdateLevel, int fFanout,
	//                  int fPower, int fVerbose )
	SAig =  Dar_ManCompress2(SAig, 1, 1, 26, 1, 0);
	Aig_ManStop( temp );
	return SAig;
}

/** Function
 * Compresses Aig by converting it to an Ntk and performing a bunch of steps on it.
 * Deletes SAig and returns a compressed version
 * @param SAig      [in]        Aig to be compressed
 */
Aig_Man_t* compressAigByNtk(Aig_Man_t* SAig) {
	Aig_Man_t* temp;
	string command;

	OUT("Cleaning up...");
	int removed = Aig_ManCleanup(SAig);
	cout << "Removed " << removed <<" nodes" << endl;

	SAig =  Dar_ManCompress2(temp = SAig, 1, 1, 26, 1, 0);
	Aig_ManStop(temp);

	Abc_Ntk_t * SNtk = Abc_NtkFromAigPhase(SAig);
	Abc_FrameSetCurrentNetwork(pAbc, SNtk);

	// TODO: FIX
	// if(options.evalAigAtNode) {
	// 	command = "balance; rewrite -l; rewrite -lz; balance; rewrite -lz; \
	// 				balance; rewrite -l; refactor -l; balance; rewrite -l; \
	// 				rewrite -lz; balance; refactor -lz; rewrite -lz; balance;";
	// } else {
		command = "fraig; balance; rewrite -l; rewrite -lz; balance; rewrite -lz; \
					balance; rewrite -l; refactor -l; balance; rewrite -l; \
					rewrite -lz; balance; refactor -lz; rewrite -lz; balance;";
	// }

	if (Cmd_CommandExecute(pAbc, (char*)command.c_str())) {
		cout << "Cannot preprocess SNtk" << endl;
		return NULL;
	}
	SNtk = Abc_FrameReadNtk(pAbc);
	temp = Abc_NtkToDar(SNtk, 0, 0);
	Aig_ManStop(SAig);
	return temp;
}


/** Function
 * Compresses Aig by converting it to an Ntk and performing a bunch of steps on it.
 * Deletes SAig and returns a compressed version
 * @param SAig      [in]        Aig to be compressed
 * @param times     [in]        Number of compression cycles to be run
 */
Aig_Man_t* compressAigByNtkMultiple(Aig_Man_t* SAig, int times) {
	Aig_Man_t* temp;
	string command;

	Aig_ManCleanup(SAig);
	Abc_Ntk_t * SNtk = Abc_NtkFromAigPhase(SAig);
	Abc_FrameSetCurrentNetwork(pAbc, SNtk);

	// TODO: FIX
	assert(true);
	command = "rewrite -lz; refactor -l;";

	// cout << "balancing..." << endl;
	if (Cmd_CommandExecute(pAbc, "balance;")) {
		cout << "Cannot preprocess SNtk" << endl;
		return NULL;
	}

	for (int i = 0; i < times; ++i)	{
		// cout << "cycle " << i << ": " << command;
		if (Cmd_CommandExecute(pAbc, (char*)command.c_str())) {
			cout << "Cannot preprocess SNtk" << endl;
			return NULL;
		}
	}

	// cout << "balancing..." << endl;
	if (Cmd_CommandExecute(pAbc, "balance;")) {
		cout << "Cannot preprocess SNtk" << endl;
		return NULL;
	}

	SNtk = Abc_FrameReadNtk(pAbc);
	temp = Abc_NtkToDar(SNtk, 0, 0);
	Aig_ManStop(SAig);
	return temp;
}

Aig_Obj_t* OR_rec(Aig_Man_t* SAig, vector<int>& nodes, int start, int end) {

	assert(end > start);

	if(end == start+1)
		return Aig_ObjChild0(Aig_ManCo(SAig,nodes[start]));

	int mid = (start+end)/2;
	Aig_Obj_t* lh = OR_rec(SAig, nodes, start, mid);
	Aig_Obj_t* rh = OR_rec(SAig, nodes, mid, end);
	Aig_Obj_t* nv = Aig_Or(SAig, lh, rh);

	return nv;
}

void Aig_ObjDeleteCo( Aig_Man_t * p, Aig_Obj_t * pObj) {
	// Function Not Tested, use at your own risk!
	assert( Aig_ObjIsCo(pObj) );
	Aig_ObjDisconnect(p,pObj);
	pObj->pFanin0 = NULL;
	p->nObjs[pObj->Type]--;
	Vec_PtrWriteEntry( p->vObjs, pObj->Id, NULL );
	Aig_ManRecycleMemory( p, pObj );
	Vec_PtrRemove(p->vCos, pObj);
}

Aig_Obj_t* newOR(Aig_Man_t* SAig, vector<int>& nodes) {
	return OR_rec(SAig, nodes, 0, nodes.size());
}

Aig_Obj_t* Aig_XOR(Aig_Man_t*p, Aig_Obj_t*p0, Aig_Obj_t*p1) {
	return Aig_Or( p, Aig_And(p, p0, Aig_Not(p1)), Aig_And(p, Aig_Not(p0), p1) );
}

void Aig_ComposeVec_rec( Aig_Man_t * p, Aig_Obj_t * pObj) {
	assert( !Aig_IsComplement(pObj) );
	if ( Aig_ObjIsMarkA(pObj) || Aig_ObjIsConst1(pObj) || Aig_ObjIsCi(pObj) ) {
		return;
			}
	Aig_ComposeVec_rec( p, Aig_ObjFanin0(pObj) );
	Aig_ComposeVec_rec( p, Aig_ObjFanin1(pObj) );
	pObj->pData = Aig_And( p, Aig_ObjChild0Copy(pObj), Aig_ObjChild1Copy(pObj) );
	assert( !Aig_ObjIsMarkA(pObj) ); // loop detection
	Aig_ObjSetMarkA( pObj );
}

Aig_Obj_t * Aig_ComposeVec( Aig_Man_t * p, Aig_Obj_t * pRoot, vector<Aig_Obj_t *>& pFuncVec,
	vector<int>& iVarVec ) {
	int i;
	Aig_Obj_t* pObj;
	
	// quit if the PI variable is not defined
	assert(pFuncVec.size() == iVarVec.size());

	for(auto iVar: iVarVec) {
		if (iVar >= Aig_ManCiNum(p)) {
			printf( "Aig_Compose(): The PI variable %d is not defined.\n", iVar );
			return NULL;
		}
	}
	Aig_ManConst1(p)->pData = Aig_ManConst1(p);

	Aig_ManForEachCi(p, pObj, i) {
		pObj->pData = pObj;
	}	

	for (int i = 0; i < iVarVec.size(); i++) {
		Aig_ManCi(p, iVarVec[i])->pData = pFuncVec[i];
	}

	// recursively perform composition
	Aig_ComposeVec_rec( p, Aig_Regular(pRoot));
	// clear the markings
	Aig_ConeUnmark_rec( Aig_Regular(pRoot) );
	return Aig_NotCond( (Aig_Obj_t *)Aig_Regular(pRoot)->pData, Aig_IsComplement(pRoot) );
}

void Aig_VecVecConeUnmark_rec(Aig_Obj_t * pObj) {
	assert(!Aig_IsComplement(pObj));
	// cout<<"Aig_VecVecConeUnmark_rec: ";
	// Aig_ObjPrintVerbose(pObj,1);
	// cout <<endl;
	// if(Aig_ObjIsConst1(pObj) || Aig_ObjIsCi(pObj))
 //    	cout << "TOOOO111" << endl;
	if(!Aig_ObjIsMarkA(pObj))
		return;
	if(Aig_ObjIsConst1(pObj) || Aig_ObjIsCi(pObj)) {
		// cout << "TOOOO" << endl;
		delete (vector<Aig_Obj_t*> *)(pObj->pData);
		assert(Aig_ObjIsMarkA(pObj)); // loop detection
		Aig_ObjClearMarkA(pObj);
		return;
	}
	delete (vector<Aig_Obj_t*> *)(pObj->pData);
	Aig_VecVecConeUnmark_rec(Aig_ObjFanin0(pObj));
	Aig_VecVecConeUnmark_rec(Aig_ObjFanin1(pObj));
	assert(Aig_ObjIsMarkA(pObj)); // loop detection
	Aig_ObjClearMarkA(pObj);
}

void Aig_ComposeVecVec_rec(Aig_Man_t* p, Aig_Obj_t* pObj, vector<vector<Aig_Obj_t*> >& pFuncVecs) {
	assert(!Aig_IsComplement(pObj));
	if(Aig_ObjIsMarkA(pObj) )
		return;
	vector<Aig_Obj_t *>* tempData = new vector<Aig_Obj_t* >();
	if(Aig_ObjIsConst1(pObj)) {
		for(int i = 0; i < pFuncVecs.size(); i++)
			tempData->push_back(pObj);
		pObj->pData = tempData;
		assert(!Aig_ObjIsMarkA(pObj)); // loop detection
		Aig_ObjSetMarkA(pObj);
		return;
	}
	if(Aig_ObjIsCi(pObj)) {
		int i = nodeIdtoN[(pObj->Id) - 1];
//		cout << "Printing nodeIdtoN @ i " << i << "  " << i - numX  << " " << i - 2 * numX << endl;
		for(int j = 0; j < pFuncVecs.size(); j++)
		{
			if ((i >= numX && i < numOrigInputs))
				tempData->push_back(pFuncVecs[j][i - numX]); // - numX]);
	 		else if( ( i >= numOrigInputs + numX ))
				tempData->push_back(pFuncVecs[j][i - (2 * numX)]);
			else
				tempData->push_back(pObj);
		}
		pObj->pData = tempData;
		assert(!Aig_ObjIsMarkA(pObj)); // loop detection
		Aig_ObjSetMarkA(pObj);
		return;
	}
	Aig_ComposeVecVec_rec(p, Aig_ObjFanin0(pObj), pFuncVecs);
	Aig_ComposeVecVec_rec(p, Aig_ObjFanin1(pObj), pFuncVecs);
	for(int j = 0; j < pFuncVecs.size(); j++) {
		Aig_Obj_t *l = Aig_ObjFanin0(pObj)? Aig_NotCond(((vector<Aig_Obj_t* >*)(Aig_ObjFanin0(pObj)->pData))->at(j), Aig_ObjFaninC0(pObj)) : NULL;
		Aig_Obj_t *r = Aig_ObjFanin1(pObj)? Aig_NotCond(((vector<Aig_Obj_t* >*)(Aig_ObjFanin1(pObj)->pData))->at(j), Aig_ObjFaninC1(pObj)) : NULL;
		tempData->push_back(Aig_And(p, l, r));
	}
	pObj->pData = tempData;
	assert(!Aig_ObjIsMarkA(pObj)); // loop detection
	Aig_ObjSetMarkA(pObj);
}

vector<Aig_Obj_t* > Aig_ComposeVecVec(Aig_Man_t* p, Aig_Obj_t* pRoot,
	vector<vector<Aig_Obj_t*> >& pFuncVecs) {
	Aig_ComposeVecVec_rec(p, Aig_Regular(pRoot), pFuncVecs);
	// clear the markings
	vector<Aig_Obj_t* >* pRootpData = (vector<Aig_Obj_t* > *)(Aig_Regular(pRoot)->pData);
	int pRootIsComp = Aig_IsComplement(pRoot);
	vector<Aig_Obj_t* > result;
	for(int i = 0; i < pRootpData->size(); i++) {
		result.push_back(Aig_NotCond(pRootpData->at(i), pRootIsComp));
	}
	Aig_VecVecConeUnmark_rec(Aig_Regular(pRoot));
	return result;
}

static void Sat_SolverClauseWriteDimacs( FILE * pFile, clause * pC) {
	int i;
	for ( i = 0; i < (int)pC->size; i++ )
		fprintf( pFile, "%s%d ", (lit_sign(pC->lits[i])? "-": ""),  lit_var(pC->lits[i]));
	fprintf( pFile, "0\n" );
}

void Sat_SolverWriteDimacsAndIS(sat_solver * p, char * pFileName,
	lit* assumpBegin, lit* assumpEnd, vector<int>&IS, vector<int>&retSet) {
	Sat_Mem_t * pMem = &p->Mem;
	FILE * pFile;
	clause * c;
	int i, k, nUnits;

	// count the number of unit clauses
	nUnits = 0;
	for ( i = 0; i < p->size; i++ )
		if ( p->levels[i] == 0 && p->assigns[i] != 3 )
			nUnits++;

	// start the file
	pFile = pFileName ? fopen( pFileName, "wb" ) : stdout;
	if ( pFile == NULL )
	{
		printf( "Sat_SolverWriteDimacs(): Cannot open the ouput file.\n" );
		return;
	}

	fprintf( pFile, "p cnf %d %d\n", p->size, Sat_MemEntryNum(&p->Mem, 0)-1+Sat_MemEntryNum(&p->Mem, 1)+nUnits+(int)(assumpEnd-assumpBegin) );

	// TODO: Print Independent Support
	i=0;
	fprintf( pFile, "c ind ");
	for(auto it:IS) {
		if(i == 10) {
			fprintf( pFile, "0\nc ind ");
			i = 0;
		}
		fprintf( pFile, "%d ", it);
		i++;
	}
	fprintf( pFile, "0\n");

	// TODO: Print Return Set
	i=0;
	fprintf( pFile, "c ret ");
	for(auto it:retSet) {
		if(i == 10) {
			fprintf( pFile, "0\nc ret ");
			i = 0;
		}
		fprintf( pFile, "%d ", it);
		i++;
	}
	fprintf( pFile, "0\n");

	// write the original clauses
	Sat_MemForEachClause(pMem, c, i, k)
		Sat_SolverClauseWriteDimacs(pFile, c);

	// write the learned clauses
	Sat_MemForEachLearned(pMem, c, i, k)
		Sat_SolverClauseWriteDimacs(pFile, c);

	// write zero-level assertions
	for (i = 0; i < p->size; i++)
		if (p->levels[i] == 0 && p->assigns[i] != 3) // varX
			fprintf(pFile, "%s%d 0\n",
					(p->assigns[i] == 1)? "-": "",    // var0
					 i);

	// write the assump
	if (assumpBegin) {
		for (;assumpBegin != assumpEnd; assumpBegin++) {
			fprintf( pFile, "%s%d 0\n",
					 lit_sign(*assumpBegin)? "-": "",
					 lit_var(*assumpBegin));
		}
	}

	fprintf(pFile, "\n");
	if (pFileName) fclose(pFile);
}

string getFileName(string s) {
	size_t i;

	i = s.rfind('/', s.length());
	if (i != string::npos) {
		s = s.substr(i+1);
	}
	assert(s.length() != 0);

	i = s.rfind('.', s.length());
	if (i != string::npos) {
		s = s.substr(0,i);
	}
	assert(s.length() != 0);

	return(s);
}

int checkUnateSyntacticAll(Aig_Man_t* FAig, vector<int>&unate, int beginIdx) {
	Nnf_Man nnfSyntatic(FAig);
	assert(nnfSyntatic.getCiNum() == numOrigInputs);

	int numUnate = 0;
	for(int i = beginIdx; i < numY; ++i) {
		if (unate[i] == -1) {
			int refPos = nnfSyntatic.getCiPos(varsYF[i])->getNumRef();
			int refNeg = nnfSyntatic.getCiNeg(varsYF[i])->getNumRef();
			if(refPos == 0) {
				unate[i] = 0;
				cout << "Var y" << i << " (" << varsYF[i] << ") is negative unate (syntactic)" << endl;
			} else if(refNeg == 0) {
				unate[i] = 1;
				cout << "Var y" << i << " (" << varsYF[i] << ") is positive unate (syntactic)" << endl;
			}
			if (unate[i] != -1) {
				numUnate++;
			}
		}
	}
	cout << "Found " << numUnate << " syntactic unates" << endl;
	return numUnate;
}

//Temporarily coded by Shetal
int checkUnateSemAll(Aig_Man_t* FAig, vector<int>&unate, int beginIdx) { 
	// Aig_ManPrintStats(FAig);

	auto unate_start = std::chrono::steady_clock::now();
//Is this required?
//	nodeIdtoN.resize(numOrigInputs);
//	for(int i = 0; i < numX; i++) {
//		nodeIdtoN[varsXF[i] - 1] = i;
//	}
//	for(int i = 0; i < numY; i++) {
//		nodeIdtoN[varsYF[i] - 1] = numX + i;
//	}
	
	// cout << " Preparing for semantic unate checks " << endl;
	sat_solver *pSat = sat_solver_new();
	Cnf_Dat_t *SCnf = Cnf_Derive_Wrapper(FAig, Aig_ManCoNum(FAig));
	addCnfToSolver(pSat, SCnf);
	int numCnfVars = SCnf->nVars;
	Cnf_Dat_t* SCnf_copy = Cnf_DataDup(SCnf);
 	Cnf_DataLift (SCnf_copy, numCnfVars);	
	addCnfToSolver(pSat, SCnf_copy);
	int status, numUnate, totalNumUnate = 0;
	assert(unate.size()==numY);
//Equate the X's and Y's. 
	lit Lits[3];
	for(int i = 0; i < numX; ++i) {
		Lits[0] = toLitCond( SCnf->pVarNums[ varsXF[i] + 1], 0 );
		Lits[1] = toLitCond( SCnf_copy->pVarNums[ varsXF[i] + 1], 1 );
	//	cout << " Adding clause " <<  Lits [0] << " " << Lits [1] << endl;
        	if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
            		assert( 0 );
		Lits[0] = toLitCond( SCnf->pVarNums[ varsXF[i] + 1], 1 );
		Lits[1] = toLitCond( SCnf_copy->pVarNums[ varsXF[i] + 1], 0 );
        	if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
            		assert( 0 );
	//	cout << " Adding clause " <<  Lits [0] << " " << Lits [1] << endl;
	}

	int cont_Vars[numY]; //control Variables
	for(int i = 0; i < numY; ++i) {
	
		cont_Vars[i] = sat_solver_addvar (pSat);
		Lits[0] = toLitCond( SCnf->pVarNums[ varsYF[i] + 1], 0 );
		Lits[1] = toLitCond( SCnf_copy->pVarNums[ varsYF[i] + 1], 1 );
		Lits[2] = Abc_Var2Lit (cont_Vars[i], 0);
//		cout << " Adding clause for " << i << " "  <<  Lits [0] << " " << Lits [1] << " " << Lits [2] << endl;
        	if ( !sat_solver_addclause( pSat, Lits, Lits+3 ) )
            		assert( 0 );
		Lits[0] = toLitCond( SCnf->pVarNums[ varsYF[i] + 1], 1 );
		Lits[1] = toLitCond( SCnf_copy->pVarNums[ varsYF[i] + 1], 0 );
		Lits[2] = Abc_Var2Lit (cont_Vars[i], 0);
///		cout << " Adding clause for " << i << " "  <<  Lits [0] << " " << Lits [1] << " " << Lits [2] << endl;
        	if ( !sat_solver_addclause( pSat, Lits, Lits+3 ) )
            		assert( 0 );
		if (unate [i] != -1) // Y is already syntactically unate
		{
			//unate[i] = -1;
			//cout << "Adding " << varsYF[i] << " as unate " << unate [i] << endl;
			addVarToSolver(pSat, SCnf->pVarNums[varsYF[i] + 1], unate[i]);
			addVarToSolver(pSat, SCnf_copy->pVarNums[varsYF[i] + 1], unate[i]);
		}
	}
	
	//lit LA[numY+4];
	lit LA[numY+2];

	//cout << "NumCnfVars " <<  numCnfVars << endl;
	//cout << "SCnfVars " <<  SCnf->nVars << endl;
	int coID = getCnfCoVarNum(SCnf, FAig, 0);
	
	Aig_Obj_t * pCo = Aig_ManCo(FAig, 0);
	assert (coID == SCnf->pVarNums[pCo->Id]);

	addVarToSolver(pSat, SCnf->pVarNums[pCo->Id], 1);
	addVarToSolver(pSat, SCnf_copy->pVarNums[pCo->Id], 0);
//	LA[0] = toLitCond( SCnf->pVarNums[pCo->Id],0);	
//	LA[1] = toLitCond(SCnf_copy->pVarNums[pCo->Id],1); //Check whether this is positive or negative	-- assuming 0 is true

	do {
		numUnate = 0;
		for (int i = beginIdx; i < numY; ++i)
		{
	//		cout << "Checking @ " << i << " for " << varsYF[i] << " YIndex = " << yIndex << "Unate " << unate [i] << endl;	
			for (int j = 0; j < numY; j++)
			{
				if (j == i)
					LA[2+j] = Abc_Var2Lit (cont_Vars[j], 0);
				else
					LA[2+j] = Abc_Var2Lit (cont_Vars[j], 1);
				//cout << " Control variable " << j   << " is " << LA [4+j] << endl;
			}

			if(unate[i] == -1) {
				// Check if positive unate
				LA[0] = toLitCond(SCnf->pVarNums[varsYF[i] + 1], 1); //check the 0 and 1's
				LA[1] = toLitCond(SCnf_copy->pVarNums[varsYF[i] + 1], 0);
				//cout << "Printing assumptions for pos unate : " << LA [0] << " " << LA [1] << " " << LA [2] << " " << LA [3] << endl;
				status = sat_solver_solve(pSat, LA, LA+2+numY, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
				//cout << "Status of sat solver " << status << endl;
				if (status == l_False) {
					unate[i] = 1;
					cout << "Var y" << i << " (" << varsYF[i] + 1 << ") is positive unate (semantic)" << endl;
					// sat_solver_push(pSat, toLitCond(SCnf->pVarNums[varsYF[i]],0));
					addVarToSolver(pSat, SCnf->pVarNums[varsYF[i] + 1], 1);
					addVarToSolver(pSat, SCnf_copy->pVarNums[varsYF[i] + 1], 1);
					numUnate++;
				}
			}
			if(unate[i] == -1) {
				// Check if negative unate
				LA[0] = toLitCond(SCnf->pVarNums[varsYF[i] + 1], 0); //check the 0 and 1's
				LA[1] = toLitCond(SCnf_copy->pVarNums[varsYF[i] + 1], 1);
				//cout << "Printing assumptions for neg unate : " << LA [0] << " " << LA [1] << " " << LA [2] << " " << LA [3] << endl;
				//LA[0] = toLitCond(getCnfCoVarNum(SCnf, FAig, negUnates[i]),1);
				status = sat_solver_solve(pSat, LA, LA+2+numY, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0);
				//cout << "Status of sat solver " << status << endl;
				if (status == l_False) {
					cout << "Var y" << i << " (" << varsYF[i] + 1 << ") is negative unate (semantic)" << endl;
					unate[i] = 0;
					// sat_solver_push(pSat, toLitCond(SCnf->pVarNums[varsYF[i]],1));
					addVarToSolver(pSat, SCnf->pVarNums[varsYF[i] + 1], 0);
					addVarToSolver(pSat, SCnf_copy->pVarNums[varsYF[i] + 1], 0);
					numUnate++;
				}
			}
		}
		cout << "Found " << numUnate << " semantic unates" << endl;
		totalNumUnate += numUnate;

		auto unate_end = std::chrono::steady_clock::now();
		auto unate_run_time = std::chrono::duration_cast<std::chrono::microseconds>(unate_end - unate_start).count()/1000000.0;
		if(unate_run_time >= options.unateTimeout) {
			cout << "checkUnateSemanticAll Timed Out" << endl;
			break;
		}
	}
	while(numUnate > 0);

	sat_solver_delete(pSat);
	Cnf_DataFree(SCnf);
	Cnf_DataFree(SCnf_copy);

//	cout << "TotalNumUnate =  " << totalNumUnate << endl;
	return totalNumUnate;

}

int getNumY(string varsFile) {
	string line;
	int tempnumY = 0;
	ifstream varsStream(varsFile);
	if(!varsStream.is_open()) {
		cout << "Var File " + varsFile + " does not exist!" << endl;
		cerr << "Var File " + varsFile + " does not exist!" << endl;
	}
	assert(varsStream.is_open());
	while (getline(varsStream, line)) {
		if(line != "") {
			tempnumY++;
		}
	}
	return tempnumY;
}

void populateVars(Abc_Ntk_t* FNtk, string varsFile, vector<string>& varOrder,
	vector<int>& varsXF, vector<int>& varsYF,
	map<string,int>& name2IdF, map<int,string>& id2NameF) {

	int i;
	Abc_Obj_t* pPi;
	string line;

	Abc_NtkForEachCi( FNtk, pPi, i ) {
		string variable_name = Abc_ObjName(pPi);
		name2IdF[variable_name] = pPi->Id;
	}

	for(auto it:name2IdF)
		id2NameF[it.second] = it.first;

	auto name2IdFTemp = name2IdF;
	ifstream varsStream(varsFile);
	if(!varsStream.is_open()) {
		cout << "Var File " + varsFile + " does not exist!" << endl;
		cerr << "Var File " + varsFile + " does not exist!" << endl;
	}
	assert(varsStream.is_open());
	while (getline(varsStream, line)) {
		if(line != "") {
			auto it = name2IdFTemp.find(line);
			assert(it != name2IdFTemp.end());
			varOrder.push_back(line);
			varsYF.push_back(it->second);
			name2IdFTemp.erase(it);
		}
	}
	for(auto it:name2IdFTemp) {
		varsXF.push_back(it.second);
	}

	numX = varsXF.size();
	numY = varsYF.size();
	numOrigInputs = numX + numY;

	if(numY <= 0) {
		cout << "Var File " + varsFile + " is empty!" << endl;
		cerr << "Var File " + varsFile + " is empty!" << endl;
		assert(numY > 0);
	}
}

void printAig(Aig_Man_t* pMan) {
	int i;
	Aig_Obj_t* pAigObj;
	cout << "\nAig: " << endl;
	Aig_ManForEachObj( pMan, pAigObj, i )
	    Aig_ObjPrintVerbose( pAigObj, 1 ), printf( "\n" );
	cout << endl;
}

int Aig_ConeCountWithConstAndMark_rec( Aig_Obj_t * pObj ) {
    int Counter;
    assert( !Aig_IsComplement(pObj) );
    if (!Aig_ObjIsNode(pObj) || Aig_ObjIsMarkA(pObj) )
    {
		if (Aig_ObjIsConst1(pObj) || Aig_ObjIsCi(pObj)) {
			if(Aig_ObjIsMarkA(pObj))
				return 0;
			else {
				Aig_ObjSetMarkA( pObj );
				return 1;
			}
		}
		else
	        return 0;
    }
    Counter = 1 + Aig_ConeCountWithConstAndMark_rec( Aig_ObjFanin0(pObj) ) +
        Aig_ConeCountWithConstAndMark_rec( Aig_ObjFanin1(pObj) );
    assert( !Aig_ObjIsMarkA(pObj) ); // loop detection
    Aig_ObjSetMarkA( pObj );
    return Counter;
}

void Aig_ConeWithConstUnmark_rec( Aig_Obj_t * pObj ) {
    assert( !Aig_IsComplement(pObj) );
    if ( !Aig_ObjIsNode(pObj) || !Aig_ObjIsMarkA(pObj) )
    {
		if (Aig_ObjIsConst1(pObj)|| Aig_ObjIsCi(pObj))
			Aig_ObjClearMarkA( pObj );
	    return;
    }
    Aig_ConeWithConstUnmark_rec( Aig_ObjFanin0(pObj) );
    Aig_ConeWithConstUnmark_rec( Aig_ObjFanin1(pObj) );
    assert( Aig_ObjIsMarkA(pObj) ); // loop detection
    Aig_ObjClearMarkA( pObj );
}

int Aig_DagSizeWithConst( Aig_Obj_t * pObj ) {
    int Counter;
    Counter = Aig_ConeCountWithConstAndMark_rec( Aig_Regular(pObj) );
    Aig_ConeWithConstUnmark_rec( Aig_Regular(pObj) );
    return Counter;
}

Cnf_Dat_t* Cnf_DeriveWithF(Aig_Man_t* Aig) {
	auto Cnf1 = Cnf_Derive(Aig, 0);
	unordered_map<int,int> cnfInvMap;

	for (int i = 0; i < numX + numY + 1; i++) {
		assert(m_FCnf->pVarNums[i] != -1);
		cnfInvMap[m_FCnf->pVarNums[i]] = Cnf1->pVarNums[i];
	}

	// pVarNums has not been fixed...
	
	auto v = ABC_ALLOC( int *, Cnf1->nClauses + m_FCnf->nClauses + 1 );
	v[0] = ABC_ALLOC( int, Cnf1->nLiterals + m_FCnf->nLiterals );

	int maxVar = -1;

	memcpy(v[0], Cnf1->pClauses[0], sizeof(int) * Cnf1->nLiterals);

	for (int i = 0; i < Cnf1->nClauses; i++) {
		int clSize = Cnf1->pClauses[i+1] - Cnf1->pClauses[i];
		v[i+1] = v[i] + clSize;
	}

	memcpy(v[Cnf1->nClauses], m_FCnf->pClauses[0], sizeof(int) * m_FCnf->nLiterals);
	
	for (int i = 0; i < m_FCnf->nClauses; i++) {
		int clSize = m_FCnf->pClauses[i+1] - m_FCnf->pClauses[i];

		for (auto j = v[Cnf1->nClauses + i]; j != v[Cnf1->nClauses + i] + clSize; j++) {
			auto it = cnfInvMap.find(Abc_Lit2Var(*j));

			if (it != cnfInvMap.end()) {
				*j = Abc_Var2Lit(it->second, Abc_LitIsCompl(*j));
			}
			else {
				// tseitin literal
				*j += 2*Cnf1->nVars;
			}
			maxVar = max(maxVar, Abc_Lit2Var(*j));
		}

		v[Cnf1->nClauses + i + 1] = v[Cnf1->nClauses + i] + clSize;
	}

	Cnf1->nClauses += m_FCnf->nClauses;
	Cnf1->nLiterals += m_FCnf->nLiterals;
	free(Cnf1->pClauses[0]);
	free(Cnf1->pClauses);
	Cnf1->pClauses = v;

	// Cnf vars begin from 1, so nVars = maxVar?
	Cnf1->nVars = maxVar;

	return Cnf1;

}

// assumes all other nodes in DFS order
// patches zeroth CO only
inline void patchCo(Aig_Man_t* Aig, Aig_Obj_t* pObj) {
	Aig_ObjDeleteCo(Aig, Aig_ManCo(Aig, 0));
	Aig_ObjCreateCo(Aig, pObj);
	Aig_ManCleanup(Aig);
}


// we know that the result is going to be negated, 
// therefore our literals are going to be chosen as the opposite
// of the literals in the original formula
// lit has been unlifted, but var as 0 still denotes Const1
Aig_Obj_t* getLit(Aig_Man_t* SAig, int lit) {
	int var = Abc_Lit2Var(lit);
	assert(var > 0);

	var -= 1;
	if (find(varsXS.begin(), varsXS.end(), var) != varsXS.end()) {
		return Aig_NotCond(Aig_ManCi(SAig, var), Abc_LitIsCompl(lit));
	}
	else {
		return Aig_ManCi(SAig, var + (Abc_LitIsCompl(lit) ? numOrigInputs : 0));
	}

}

list<set<Lit>> resolveClauses(list<set<Lit>> lClauses, Lit tsVarMax) {
	typedef set<Lit> lSet;

	while (true) {
		int tsVar = -1;
		vector<pair<list<lSet>::iterator, lSet::iterator>> pos;
		vector<pair<list<lSet>::iterator, lSet::iterator>> neg;

		// find a tseitin variable, better would be choose the one with min p*n or p+n though
		auto itCls = lClauses.begin();
		for (; itCls != lClauses.end(); itCls++) {
			auto it = itCls->begin();
			if ((it != itCls->end()) && (it->getVar() <= tsVarMax)) { // *it <= tsVarMax
				tsVar = it->getVar();
				break;
			}
		}

		if (tsVar == -1) {
			#ifdef DEBUG
				for (auto it = lClauses.begin(); it != lClauses.end(); it++) {
					assert(!it->empty());
					assert(it->begin()->getVar() > tsVarMax);
				}
			#endif
			return lClauses;
		}

		for (; itCls != lClauses.end(); itCls++) {
			auto it = itCls->lower_bound(Abc_Var2Lit(tsVar, 0));
			if ((it != itCls->end()) && (it->getVar() == tsVar)) {
				assert((next(it) == itCls->end()) || (next(it)->getVar() != tsVar));

				if (it->isCompl()) {
					neg.push_back(pair(itCls, it));
				}
				else {
					pos.push_back(pair(itCls, it));
				}
			}
		}

		int p = pos.size();
		int n = neg.size();

		if (p == 0) {
			for (int i = 0; i < n; i++) {
				neg[i].first->erase(neg[i].second);
				if (neg[i].first->empty()) {
					lClauses.erase(neg[i].first);
				}
			}
		}
		else if (n == 0) {
			for (int i = 0; i < p; i++) {
				pos[i].first->erase(pos[i].second);
				if (pos[i].first->empty()) {
					lClauses.erase(pos[i].first);
				}
			}
		}
		else {
			for (int i = 0; i < p; i++) {
				for (int j = 0; j < n; j++) {
					auto cl1 = pos[i].first;
					auto cl2 = neg[j].first;
					
					// merge clauses
					lSet clMerged;
					bool trueClause = false;
					auto it1 = cl1->begin(), it2 = cl2->begin();

					for (; !trueClause && (it1 != cl1->end()) && (it2 != cl2->end()); ) {
						if (*it1 < *it2) {
							clMerged.insert(*it1);
							it1++;
						}
						else if (*it2 < *it1) {
							clMerged.insert(*it2);
							it2++;
						}
						else if (it1->isCompl() == it2->isCompl()) {
							clMerged.insert(*it1);		
							it1++;
							it2++;
						}
						else if (it1 == pos[i].second) {
							it1++;
							it2++;
						}
						else {
							trueClause = true;
							break;
						}
					}
					while ((it1 != cl1->end()) && !trueClause) {
						clMerged.insert(*it1);
						it1++;
					}
					while ((it2 != cl2->end()) && !trueClause) {
						clMerged.insert(*it2);
						it2++;
					}

					#ifdef DEBUG
						assert(clMerged.find(Abc_Var2Lit(tsVar,0)) == clMerged.end());
					#endif

					if (trueClause) continue;
					else {
						lClauses.push_back(clMerged);
					}
				}
			}
		
			for (int i = 0; i < p; i++) {
				lClauses.erase(pos[i].first);
			}
			for (int i = 0; i < n; i++) {
				lClauses.erase(neg[i].first);
			}
		}
	}
}

Aig_Obj_t* createNodeForTsLit(Aig_Man_t* SAig, const unordered_map<int, vector<vector<Lit>>> &tsDeps,  unordered_map<int, 
								pair<bool, Aig_Obj_t*>> &visLit, Lit tsRoot, int tsVarMax, int lifter) {
	// int tsRoot = tsRoot.getVar();
	int tsVar = tsRoot.getVar();
	Aig_Obj_t* pObj;
	if (visLit[tsRoot].second != NULL) {
	}
	else if (tsRoot.getVar() > tsVarMax) {
		visLit[tsRoot] = pair(true, getLit(SAig, tsRoot.getLit() - 2*lifter));
		visLit[tsRoot.getCompl()] = pair(true, getLit(SAig, tsRoot.getCompl().getLit() - 2*lifter));
	}
	else if (tsRoot.isCompl()) {
		auto pObj = createNodeForTsLit(SAig, tsDeps, visLit, tsRoot.getCompl(), tsVarMax, lifter);
		
		vector<int> vars;
		vector<Aig_Obj_t*> funcs;

		for (auto i : varsYS) {
			vars.push_back(i);
			funcs.push_back(Aig_Not(Aig_ManCi(SAig, i + numOrigInputs)));
			vars.push_back(i + numOrigInputs);
			funcs.push_back(Aig_Not(Aig_ManCi(SAig, i)));
		}

		pObj = Aig_ComposeVec(SAig, pObj, funcs, vars);
		visLit[tsRoot] = pair(true, Aig_Not(pObj));
	}
	else {
		pObj = Aig_ManConst1(SAig);
		for (auto x : tsDeps.at(tsVar)) {
			auto pObj2 = Aig_ManConst0(SAig);
			for (auto y : x) {
				pObj2 = Aig_Or(SAig, pObj2, createNodeForTsLit(SAig, tsDeps, visLit, y, tsVarMax, lifter));
			}
			pObj = Aig_And(SAig, pObj, pObj2);
		}
		visLit[tsRoot] = pair(true, pObj);
	}
	return visLit[tsRoot].second;
}

// // TODO : remove all map refs by a simple ref to an index map and then to vectors...
// // TODO : reducing the dependencies based on the clauses in the core?
Aig_Obj_t* findAndSubsTsVars(Aig_Man_t* SAig, list<pair<bool, vector<Lit>>> &coreCls, int tsVarMax, int lifter) {
	unordered_map<int, vector<vector<Lit>>> tsDeps; // stores tsVar to other vars which define it
	list<pair<bool, vector<Lit>>>::iterator it;
	unordered_map<int, bool> occurs;


	// assumes that every clause is a definer of its first variable
	for (it = coreCls.begin(); it != coreCls.end(); ) {
		if (it->second.size() == 1) {
			it++;
			continue;
		}

		occurs.clear();
		while ((it != coreCls.end()) && !it->second.begin()->isCompl() && (it->second.size() > 1)) {
			if (it->first) {
				for (auto it2 = next(it->second.begin()); it2 != it->second.end(); it2++) {
					occurs[it2->getVar()] = true;
				}
			}
			it = coreCls.erase(it);
		}

		int cnt = 0;
		list<list<pair<bool, vector<Lit>>>::iterator> record;
		while ((it != coreCls.end()) && it->second.begin()->isCompl() && (it->second.size() > 1)) {
			vector<Lit> clOccurs;
			if (it->first) {
				for (auto it2 = next(it->second.begin()); it2 != it->second.end(); it2++) {
					if (occurs[it2->getVar()]) {
						clOccurs.push_back(*it2);
					}
				}

				// add everything if the clause is empty
				if (clOccurs.empty()) {
					clOccurs = vector<Lit>(next(it->second.begin()), it->second.end());
				}
				tsDeps[it->second.begin()->getVar()].push_back(clOccurs);
				cnt++;
			}
			record.push_back(it);
			it++;
		}
		if (cnt == 0) {
			for (auto x : record) {
				tsDeps[x->second.begin()->getVar()].push_back(vector<Lit>(next(x->second.begin()), x->second.end()));
			}
		}

		for (auto delIt : record) {
			coreCls.erase(delIt);
		}
	}

	// all signs flipped because negation in the end
	auto pObj = Aig_ManConst0(SAig);
	unordered_map<int, pair<bool, Aig_Obj_t*>> visLit;
	for (auto it = coreCls.begin(); it != coreCls.end(); it++) {
		if (it->first) {
			auto pObj2 = Aig_ManConst1(SAig);
			for (auto j : it->second) {
				pObj2 = Aig_And(SAig, pObj2, createNodeForTsLit(SAig, tsDeps, visLit, j.getCompl(), tsVarMax, lifter));
			}
			pObj = Aig_Or(SAig, pObj, pObj2);
		}
	}
	return pObj;
}

// clauses have to be converted to conjunctive clauses because negation
Aig_Obj_t* removeTsVars(Aig_Man_t* SAig, list<pair<bool, vector<Lit>>> &coreCls, int tsVarMax, int lifter) {
	assert(coreCls.size() > 0);

	Aig_Obj_t* pObj = findAndSubsTsVars(SAig, coreCls, tsVarMax, lifter);
	return pObj;
}

// take 
Aig_Obj_t* coreAndIntersect(Aig_Man_t* SAig, Aig_Man_t* Aig1) {
	auto Cnf1 = Cnf_Derive(Aig1, 0);

	auto vec = ABC_ALLOC( int*, m_FCnf->nClauses + 1 );
	vec[0] = ABC_ALLOC( int, m_FCnf->nLiterals);
	memcpy(vec[0], m_FCnf->pClauses[0], (sizeof(int)) * (m_FCnf->nLiterals));

	for (int i = 0; i < m_FCnf->nClauses; i++) {
		int clSize = m_FCnf->pClauses[i+1] - m_FCnf->pClauses[i];
		vec[i+1] = vec[i] + clSize;
	}

	unordered_map<int,int> cnf1_Map;
	unordered_map<int,int> cnf2_Map;

	// cnf1 and cnf2 on diff variables, need to fix

	// can also equate them :)
	assert(min(Cnf1->nVars, m_FCnf->nVars) >= numX+numY);
	for (int i = 0; i < numX+numY+1; i++) {
		assert(Cnf1->pVarNums[i] != -1);
		assert(m_FCnf->pVarNums[i] != -1);
		cnf1_Map[Cnf1->pVarNums[i]] = i;
		cnf2_Map[m_FCnf->pVarNums[i]] = i;
	}
	int lifter = (Cnf1->nVars + m_FCnf->nVars + 2); // 2 is buffer since vars are supposed to be from 1
	// lift both Cnfs to bigger variables

	auto solver = sat_solver_new();
	sat_solver_store_alloc(solver);

	for (int v = 0; v < Cnf1->nLiterals; v++) {
		int lit = Cnf1->pClauses[0][v];
		int var = Abc_Lit2Var(lit);

		auto it = cnf1_Map.find(var);
		if (it != cnf1_Map.end()) {
			Cnf1->pClauses[0][v] = Abc_Var2Lit(lifter + it->second, Abc_LitIsCompl(lit));
		}
	}
	// cnf1 lifted

	for (int v = 0; v < m_FCnf->nLiterals; v++) {
		int lit = vec[0][v];
		int var = Abc_Lit2Var(lit);

		auto it = cnf2_Map.find(var);
		if (it != cnf2_Map.end()) {
			vec[0][v] = Abc_Var2Lit(lifter + it->second, Abc_LitIsCompl(lit));
		} else {
			vec[0][v] += 2*Cnf1->nVars; // cnf1 and cnf2 vars independent
		}
	}

	// 

	for (int c = 0; c < Cnf1->nClauses; c++) {
		sat_solver_addclause(solver, Cnf1->pClauses[c], Cnf1->pClauses[c+1]);
	}
	sat_solver_store_mark_clauses_a(solver);

	for (int c = 0; c < m_FCnf->nClauses; c++) {
		sat_solver_addclause(solver, vec[c], vec[c+1]);
	}
	
	sat_solver_store_mark_roots(solver);

	lbool result = sat_solver_solve(solver, NULL, NULL, 0, 0, 0, 0);

	if (result == l_True) {
		vector<int> vars(numX+numY+1);

		for (int i = 0; i < numX+numY+1; i++) {
			vars[i] = lifter + i;
		}
		int* model = Sat_SolverGetModel(solver, vars.data(), vars.size());

		for (int i = 0; i < vars.size(); i++) {
			cout << "Aig ID " << i << " : " << model[i] << endl;
		}
		// cout << endl;

		cout << "Y order : " << endl;
		for (auto i : varsYS) {
			cout << i << " ";
		}
		cout << endl;
	}

	assert(result == l_False);

	auto pSatCnf = (Sto_Man_t *)sat_solver_store_release(solver);
	auto proof = Intp_ManAlloc();
	auto core = (Vec_Int_t *) Intp_ManUnsatCore(proof, pSatCnf, 0, 0);
	
	sort(core->pArray, core->pArray+core->nSize);

	// pair of (necessaryClause, literals)
	list<pair<bool, vector<Lit>>> coreClauses;

	auto curr = pSatCnf->pHead;
	list<set<Lit>> debug;
	vector<pair<int, int>> cls;
	int j = 0;
	while (curr != pSatCnf->pTail) {
		if (curr->Id < Cnf1->nClauses) {
			cls.push_back(pair(curr->Id, j));
		}
		curr = curr->pNext;
		j++;
	}

	for (int i = 0; i < cls.size(); i++) {
		bool necessary = false;
		if (binary_search(core->pArray, core->pArray+core->nSize, cls[i].first)) {
			necessary = true;
		}
		auto q = cls[i].second;
		coreClauses.push_back(pair(necessary, vector<Lit>(Cnf1->pClauses[q], Cnf1->pClauses[q+1])));
	}

	// core needs to be expanded till it contains all tseitin clauses for any ts variable!
	auto pObj = removeTsVars(SAig, coreClauses, Cnf1->nVars, lifter);

	// need to put out this check for now because pObj is not an Aig, also which Aig to use even?

	#ifdef DEBUG
		auto F = Aig_ManDupSimpleDfs(SAig);
		auto pObj2 = Aig_ManCo(F, 0)->pFanin0;
		pObj2 = Aig_And(F, pObj2, Aig_Not(Aig_Transfer(SAig, F, pObj, 2*numOrigInputs)));
		
		vector<int> vars;
		vector<Aig_Obj_t*> funcs;
		for (auto i : varsYS) {
			vars.push_back(i + numOrigInputs);
			funcs.push_back(Aig_Not(Aig_ManCi(F, i)));
		}

		Aig_ObjPatchFanin0(F, Aig_ManCo(F, 0), Aig_ComposeVec(F, pObj2, funcs, vars));
		auto Cnf3 = Cnf_Derive(F, 0);
		auto solver2 = sat_solver_new();
		for (int c = 0; c < Cnf3->nClauses; c++) {
			sat_solver_addclause(solver2, Cnf3->pClauses[c], Cnf3->pClauses[c+1]);
		}
		assert(sat_solver_solve(solver2, NULL, NULL, 0, 0, 0, 0) == l_False);
		sat_solver_delete(solver2);
		Cnf_DataFree(Cnf3);
		Aig_ManStop(F);
	#endif

	Intp_ManFree(proof);
	Sto_ManFree(pSatCnf);
	Vec_IntFree(core);
	sat_solver_delete(solver);
	Cnf_DataFree(Cnf1);
	free(vec[0]);
	free(vec);
	return pObj;
}

void removeNegations(Aig_Man_t* SAig) {
	Aig_Obj_t* pObj;
	for (auto x : varsXS) {
		pObj = Aig_ManCi(SAig, x + numOrigInputs);
		assert(Aig_ObjRefs(pObj) == 0);

		Vec_PtrWriteEntry(SAig->vCis, x+numOrigInputs, NULL);
		Vec_PtrWriteEntry(SAig->vObjs, pObj->Id, NULL);
		SAig->nObjs[AIG_OBJ_CI]--;
	}
	for (auto i : varsYS) {
		pObj = Aig_ManCi(SAig, i + numOrigInputs);
		assert(Aig_ObjRefs(Aig_ManCi(SAig, i + numOrigInputs)) == 0);

		Vec_PtrWriteEntry(SAig->vCis, i+numOrigInputs, NULL);
		Vec_PtrWriteEntry(SAig->vObjs, pObj->Id, NULL);
		SAig->nObjs[AIG_OBJ_CI]--;
	}
	auto ptr = Vec_PtrAlloc(SAig->vCis->nSize/2);
	int i;
	Vec_PtrForEachEntry(Aig_Obj_t*, SAig->vCis, pObj, i) {
		if (pObj == NULL) continue;
		Vec_PtrPush(ptr, pObj);
	}
	ABC_FREE(SAig->vCis);
	SAig->vCis = ptr;
	SAig->nDeleted += SAig->vCis->nSize;
}

Aig_Man_t* PositiveToNormal(Aig_Man_t* SAig) {
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	Aig_Man_t* FAig = Aig_ManDupSimpleDfs(SAig);

	for (int i = 0; i < numY; i++) {
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(FAig, varsYS[i])));
	}

	auto pObj = Aig_ComposeVec(FAig, Aig_ManCo(FAig, 0)->pFanin0, funcs, vars);
	patchCo(FAig, pObj);
	Aig_ManCleanup(FAig);

	removeNegations(FAig);
	assert(Aig_ManCheck(FAig));
	return FAig;
}

Aig_Man_t* PositiveToNormalWithNeg(Aig_Man_t* SAig) {
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	Aig_Man_t* FAig = Aig_ManDupSimpleDfs(SAig);

	for (int i = 0; i < numY; i++) {
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(FAig, varsYS[i])));
	}

	auto pObj = Aig_ComposeVec(FAig, Aig_ManCo(FAig, 0)->pFanin0, funcs, vars);
	patchCo(FAig, pObj);
	Aig_ManCleanup(FAig);

	assert(Aig_ManCheck(FAig));
	return FAig;
}

Aig_Man_t* NormalToPositive(Aig_Man_t* &FAig) {
	auto temp = Aig_ManDupSimpleDfs(FAig);
	Aig_ManStop(FAig);
	FAig = temp;

	Nnf_Man nnf;
	nnf.init(FAig);

	// dumpNnf(&nnf, options.benchmark + ".nnf");

	Aig_Man_t* SAig = nnf.createAigWithoutClouds();

	// remove negations of X already, we don't even need them at all
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (auto i : varsXS) {
		vars.push_back(i + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(SAig, i)));
	}

	auto pAigObj = Aig_ManCo(SAig, 0)->pFanin0;
	pAigObj = Aig_ComposeVec(SAig, pAigObj, funcs, vars);

	assert(!Aig_ObjIsCo(Aig_Regular(pAigObj)));
	Aig_ObjPatchFanin0(SAig, Aig_ManCo(SAig, 0), pAigObj);

	Aig_ManCleanup(SAig);
	assert(Aig_ManCheck(SAig));
	assert(Aig_ManCheck(FAig));
	return SAig;
}

bool Aig_ObjIsPositive(Aig_Man_t* SAig, Aig_Obj_t* pObj, const unordered_set<int> &ids, unordered_map<Aig_Obj_t*, bool> &memoiz) {
	if (memoiz.find(pObj) != memoiz.end()) {
		return memoiz[pObj];
	}
	else {
		bool ans;

		if (Aig_ObjIsConst1(Aig_Regular(pObj))) {
			ans = true;
		}
		else if (Aig_ObjIsCi(Aig_Regular(pObj))) {
			if (ids.find(Aig_Regular(pObj)->Id) != ids.end()) {
				ans = true;
			}
			else {
				ans = !bool(Aig_IsComplement(pObj));
			}
		}
		else {
			assert(Aig_ObjIsAnd(Aig_Regular(pObj)));
			ans =  Aig_ObjIsPositive(SAig, Aig_NotCond(Aig_ObjChild0(Aig_Regular(pObj)), Aig_IsComplement(pObj)), ids, memoiz) &&
					Aig_ObjIsPositive(SAig, Aig_NotCond(Aig_ObjChild1(Aig_Regular(pObj)), Aig_IsComplement(pObj)), ids, memoiz);
		}
		memoiz[pObj] = ans;
		return ans;
	}
}

bool Aig_IsPositive(Aig_Man_t* SAig) {
	assert(Aig_ManCoNum(SAig) == 1);
	Aig_Obj_t* pObj = Aig_ManCo(SAig, 0)->pFanin0;
	unordered_map<Aig_Obj_t*, bool> memoiz;

	unordered_set<int> ids;
	for (auto i : varsXS) {
		ids.insert(Aig_ManCi(SAig, i)->Id);
	}

	return Aig_ObjIsPositive(SAig, pObj, ids, memoiz);
}

Aig_Obj_t* Aig_ObjConvertToPos(Aig_Man_t* SAig, Aig_Obj_t* pObj, unordered_map<Aig_Obj_t*, Aig_Obj_t*> &memoiz,  const unordered_set<int> &ids) {
	if (memoiz[pObj] != NULL) return memoiz[pObj];
	else {
		Aig_Obj_t* res;

		if (Aig_ObjIsConst1(Aig_Regular(pObj))) res = pObj;
		else if (Aig_ObjIsCi(Aig_Regular(pObj))) {
			// SAig always has PIs at the beginning
			int id = Aig_ObjId(Aig_Regular(pObj));
			// ids of CIs are always in the beginning

			if (ids.find(id) != ids.end()) {
				res = pObj;
			}
			else {
				// Y variables
				res = (Aig_IsComplement(pObj) ? Aig_ManCi(SAig, id-1 + numOrigInputs) : Aig_ManCi(SAig, id-1));
			}
		}
		else {
			assert(Aig_ObjIsAnd(Aig_Regular(pObj)));

			if (Aig_IsComplement(pObj)) {
				res = Aig_Or(SAig, Aig_ObjConvertToPos(SAig, Aig_Not(Aig_Regular(pObj)->pFanin0), memoiz, ids),
									Aig_ObjConvertToPos(SAig, Aig_Not(Aig_Regular(pObj)->pFanin1), memoiz, ids));
			}
			else {
				res = Aig_And(SAig, Aig_ObjConvertToPos(SAig, pObj->pFanin0, memoiz, ids),
									Aig_ObjConvertToPos(SAig, pObj->pFanin1, memoiz, ids));
			}
		}
		memoiz[pObj] = res;
		return res;
	}
}

bool isConflict(Aig_Man_t* SAig, int k) {
	// does not contain Xbar
	assert(pi.X.size() == numX);
	assert(pi.Y.size() == numY);
	assert(k < numY);

	vector<Aig_Obj_t*> cex(numX+2*(numY-1));
	vector<int> vars(numX+2*(numY-1));

	for (int i = 0; i < numX; i++) {
		cex[i] = Aig_NotCond(Aig_ManConst0(SAig), pi.X[i]);
		vars[i] = varsXS[i];
	}

	// numX ... numY - 1 ... numY - 1

	for (int i = 0; i < numY; i++) {
		if (i < k) {
			vars[i+numX] = varsYS[i];
			vars[i+numX+numY-1] = varsYS[i] + numOrigInputs;
			cex[i+numX] = Aig_ManConst1(SAig);
			cex[i+numX+numY - 1] = Aig_ManConst1(SAig);
		}
		else if (i > k) {
			vars[i+numX - 1] = varsYS[i];
			vars[i+numX+numY - 2] = varsYS[i] + numOrigInputs;
			cex[i+numX - 1] = Aig_NotCond(Aig_ManConst0(SAig), pi.Y[i]);
			cex[i+numX+numY - 2] = Aig_NotCond(Aig_ManConst1(SAig), pi.Y[i]);
		}
	}

	auto pObj = Aig_ComposeVec(SAig, Aig_ManCo(SAig, 0)->pFanin0, cex, vars);


	auto const1 = Aig_ManConst1(SAig);
	auto yk = Aig_ManCi(SAig, varsYS[k]);
	auto ykbar = Aig_ManCi(SAig, varsYS[k] + numOrigInputs);

	const1->iData = 1;

	Aig_ManIncrementTravId(SAig);
	Aig_ObjSetTravIdCurrent(SAig, yk);
	Aig_ObjSetTravIdCurrent(SAig, ykbar);
	Aig_ObjSetTravIdCurrent(SAig, const1);

	yk->iData = 1;
	ykbar->iData = 1;

	bool eval11 = evaluateAigAtNode(SAig, pObj);

	Aig_ManIncrementTravId(SAig);
	Aig_ObjSetTravIdCurrent(SAig, yk);
	Aig_ObjSetTravIdCurrent(SAig, ykbar);
	Aig_ObjSetTravIdCurrent(SAig, const1);

	yk->iData = 1;
	ykbar->iData = 0;

	bool eval10 = evaluateAigAtNode(SAig, pObj);

	Aig_ManIncrementTravId(SAig);
	Aig_ObjSetTravIdCurrent(SAig, yk);
	Aig_ObjSetTravIdCurrent(SAig, ykbar);
	Aig_ObjSetTravIdCurrent(SAig, const1);

	yk->iData = 0;
	ykbar->iData = 1;

	bool eval01 = evaluateAigAtNode(SAig, pObj);

	#ifdef DEBUG
		assert(Aig_IsPositive(SAig));
	#endif

	Aig_ManCleanup(SAig);

	return eval11 && !eval10 && !eval01;
}

void Rectify(Aig_Man_t* SAig, int k) {
	Aig_Obj_t* clause = Aig_ManConst0(SAig);

	for (int i = 0; i < numX; i++) {
		if (pi.X[i] == 1) {
			clause = Aig_Or(SAig, clause, Aig_Not(Aig_ManCi(SAig, varsXS[i])));
		}
		else {
			clause = Aig_Or(SAig, clause, Aig_ManCi(SAig, varsXS[i]));
		}
	}

	for (int i = k+1; i < numY; i++) {
		if (pi.Y[i] == 1) {
			clause = Aig_Or(SAig, clause, Aig_ManCi(SAig, varsYS[i] + numOrigInputs));
		}
		else {
			clause = Aig_Or(SAig, clause, Aig_ManCi(SAig, varsYS[i]));
		}
	}

	clause = Aig_And(SAig, clause, Aig_ManCo(SAig, 0)->pFanin0);

	patchCo(SAig, clause);
}

// stores the depth for each node
void postDfs(Aig_Man_t* Aig, Aig_Obj_t* pObj, int k, bool univQuantify) {
	assert(!Aig_IsComplement(pObj));
	assert(!Aig_ObjIsCo(pObj));

	int Id = Aig_ObjId(pObj);

	if (pObj->iData != 0) {
		return;
	}
	else if (Aig_ObjIsCi(pObj) || Aig_ObjIsConst1(pObj)) {
		if (univQuantify || ((Id != Aig_ObjId(Aig_ManCi(Aig, varsYS[k]))) && (Id != Aig_ObjId(Aig_ManCi(Aig, varsYS[k] + numOrigInputs))))) {
			pObj->iData = 1;
		}
		else {
			pObj->iData = -1;
		}
	}
	else if (Aig_ObjIsNode(pObj)) {
		auto c1 = Aig_ObjFanin0(pObj);
		auto c2 = Aig_ObjFanin1(pObj);

		postDfs(Aig, c1, k, univQuantify);
		postDfs(Aig, c2, k, univQuantify);

		if ((c1->iData == -1) || (c2->iData == -1)) {
			pObj->iData = -1;
		}
		else {
			pObj->iData = max(c1->iData, c2->iData) + 1;
		}
	}
	else {
		assert(false);
	}
}

vector<Aig_Obj_t*> chooseCut(Aig_Man_t* Aig, Aig_Obj_t* pRoot, int k, int depth, bool univQuantify) {
	vector<Aig_Obj_t*> savedObjs, retObjs;
	Aig_ManCleanData(Aig);

	Aig_Obj_t* pObj = Aig_Regular(pRoot);
	postDfs(Aig, pObj, k, univQuantify);


	int i;
	Aig_ManForEachObj(Aig, pObj, i) {
		if ((pObj->iData > 0) && (pObj->iData - 1 <= depth)) {
			savedObjs.push_back(pObj);
		}
	}

	sort(savedObjs.begin(), savedObjs.end(), [](const Aig_Obj_t* a, const Aig_Obj_t* b) -> bool { return a->iData > b->iData;	});

	for (auto i : savedObjs) {
		i->iData = i->nRefs;
	}

	for (auto i : savedObjs) {
		if (i->iData > 0) {
			retObjs.push_back(i);
		}

		if (Aig_ObjIsAnd(i)) {
			Aig_ObjFanin0(i)->iData -= 1;
			Aig_ObjFanin1(i)->iData -= 1;
		}
	}

	assert(retObjs.size() > 0);

	return retObjs;
}

void Rectify2(Aig_Man_t* SAig, int k, int depth, bool allowUnivQuantify) {
	Aig_Obj_t *pObj;
	Aig_Man_t* G = Aig_ManDupSimpleDfs(SAig);
	
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (int i = 0; i < k; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_ManConst1(G));
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_ManConst1(G));
	}

	for (int i = k+1; i < numY; i++) {
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(G, varsYS[i])));
	}

	pObj = Aig_ManCo(G, 0)->pFanin0;
	pObj = Aig_ComposeVec(G, pObj, funcs, vars);
	assert(!Aig_ObjIsCo(Aig_Regular(pObj)));
	Aig_ObjPatchFanin0(G, Aig_ManCo(G, 0), pObj);
	
	vector<Aig_Obj_t*> savedObjs = chooseCut(G, pObj, k, depth, allowUnivQuantify);

	assert(savedObjs.size() > 0);

	vars.clear();
	funcs.clear();

	for (int i = 0; i < numX; i++) {
		vars.push_back(varsXS[i]);
		funcs.push_back(Aig_NotCond(Aig_ManConst0(G), pi.X[i]));
	}
	for (int i = k+1; i < numY; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_NotCond(Aig_ManConst0(G), pi.Y[i]));
	}

	// wrong check again
	Aig_ComposeVec(G, Aig_ManCo(G, 0)->pFanin0, funcs, vars);
	// assert(Aig_ObjIsAnd(pObj));

	pObj = Aig_ManConst1(G);

	for (auto &pObj2 : savedObjs) {
		Aig_Obj_t* pVal = (Aig_Obj_t*) pObj2->pData;
		#ifdef DEBUG
			// assert(Aig_SupportSize(G, Aig_Regular(pVal)) <= 2);
		#endif

		pObj = Aig_And(G, pObj, Aig_Not(Aig_XOR(G, pVal, pObj2)));
	}

	assert(!Aig_ObjIsCo(Aig_Regular(pObj)));

	// only three copies are made now, instead of original four

	if (allowUnivQuantify)
	{
		vars = {varsYS[k], varsYS[k] + numOrigInputs};
		funcs = {Aig_ManConst1(G), Aig_ManConst1(G)};
		auto pObj2 = Aig_ComposeVec(G, pObj, funcs, vars);

		funcs = {Aig_ManConst1(G), Aig_ManConst0(G)};
		pObj2 = Aig_And(G, pObj2, Aig_Not(Aig_ComposeVec(G, pObj, funcs, vars)));

		funcs = {Aig_ManConst0(G), Aig_ManConst1(G)};
		pObj = Aig_And(G, pObj2, Aig_Not(Aig_ComposeVec(G, pObj, funcs, vars)));
	}

	Aig_ObjPatchFanin0(G, Aig_ManCo(G, 0), pObj);
	Aig_ManCleanup(G);

	auto generalize = coreAndIntersect(SAig, G);

	pObj = Aig_ManCo(SAig, 0)->pFanin0;
  	assert(!Aig_ObjIsCo(Aig_Regular(pObj)));
	patchCo(SAig, Aig_And(SAig, pObj, generalize));

	Aig_ManStop(G);
}

void Rectify3(Aig_Man_t* SAig, int k, int depth, bool allowUnivQuantify) {
	Aig_Obj_t *pObj;
	Aig_Man_t* F = Aig_ManDupSimpleDfs(SAig);

	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (int i = 0; i < k; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_ManConst1(F));
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_ManConst1(F));
	}

	for (int i = k+1; i < numY; i++) {
		vars.push_back(varsYS[i] + numOrigInputs);
		funcs.push_back(Aig_Not(Aig_ManCi(F, varsYS[i])));
	}

	pObj = Aig_ComposeVec(F, Aig_ManCo(F, 0)->pFanin0, funcs, vars);
	vector<Aig_Obj_t*> savedObjs = chooseCut(F, pObj, k, depth, false);

	vars.clear();
	funcs.clear();

	for (int i = 0; i < numX; i++) {
		vars.push_back(varsXS[i]);
		funcs.push_back(Aig_NotCond(Aig_ManConst0(F), pi.X[i]));
	}
	for (int i = k+1; i < numY; i++) {
		vars.push_back(varsYS[i]);
		funcs.push_back(Aig_NotCond(Aig_ManConst0(F), pi.Y[i]));
	}

	// wrong check, need to evaluate aig and semantic check if y_k and ybar_k
	Aig_ComposeVec(F, pObj, funcs, vars);

	pObj = Aig_ManConst1(F);

	int extraCnt = savedObjs.size();
	for (int i = 0; i < extraCnt; i++) {
		Aig_Obj_t* g = Aig_ObjCreateCi(F);
		Aig_Obj_t* pVal = (Aig_Obj_t*) savedObjs[i]->pData;

		pObj = Aig_And(F, pObj, Aig_Not(Aig_XOR(F, g, savedObjs[i])));
		assert(Aig_ObjIsConst1(Aig_Regular(pVal)));

		Aig_ObjCreateCo(F, Aig_NotCond(g, Aig_IsComplement(pVal)));
	}

	Aig_ObjPatchFanin0(F, Aig_ManCo(F, 0), pObj);

	Aig_ManCleanup(F);

	auto Fnew = Aig_ManDupSimpleDfs(F);

	TIMED(rectifyCnfTime, auto Cnf = Cnf_DeriveWithF(Fnew);)

	unordered_map<int, int> invMap;
	for (int i = 0; i < savedObjs.size(); i++) {
		invMap[Cnf->pVarNums[Aig_ObjId(Aig_ObjFanin0(Aig_ManCo(Fnew, i + 1)))]] = i;
	}

	TIMED(
		rectifyUnsatCoreTime,
		auto solver = sat_solver_new();
		sat_solver_store_alloc(solver);

		for (int i = 0; i < Cnf->nClauses; i++) {
			sat_solver_addclause(solver, Cnf->pClauses[i], Cnf->pClauses[i + 1]);
		}

		sat_solver_store_mark_roots(solver);

		lbool result = sat_solver_solve(solver, NULL, NULL, 0, 0, 0, 0);

		assert(result == l_False);

		auto pSatCnf = (Sto_Man_t *)sat_solver_store_release(solver);
		auto proof = Intp_ManAlloc();
		auto core = (Vec_Int_t *) Intp_ManUnsatCore(proof, pSatCnf, 0, 0);)

	sort(core->pArray, core->pArray+core->nSize);

	pObj = Aig_ManConst0(SAig);

	auto cls = pSatCnf->pHead;
	int i = 0, idx = 0;

	unordered_set<int> idsX;
	for (auto i : varsXS) {
		idsX.insert(Aig_ManCi(SAig, i)->Id);
	}

	unordered_map<Aig_Obj_t*, Aig_Obj_t*> memoiz;

	while ((i < core->nSize) && (cls != pSatCnf->pTail)) {
		if (core->pArray[i] == idx) {
			// work with pObj
			if ((cls->nLits == 1) && (invMap.find(Abc_Lit2Var(cls->pLits[0])) != invMap.end())) {
				auto pObj2 = Aig_NotCond( Aig_Transfer(F, SAig, savedObjs[invMap[Abc_Lit2Var(cls->pLits[0])]], 2*numOrigInputs),
											 !Abc_LitIsCompl(cls->pLits[0]));
				pObj = Aig_Or(SAig, pObj, Aig_ObjConvertToPos(SAig, pObj2, memoiz, idsX));
			}

			i++;
		}
		cls = cls->pNext;
		idx++;
	}
	
	pObj = Aig_And(SAig, Aig_ManCo(SAig, 0)->pFanin0, pObj);
	patchCo(SAig, pObj);
	Aig_ManCleanData(SAig);

	assert(Aig_ManCheck(SAig));

	Aig_ManStop(F);
	Aig_ManStop(Fnew);
	Cnf_DataFree(Cnf);
	Intp_ManFree(proof);
	sat_solver_delete(solver);
	Vec_IntFree(core);
	Sto_ManFree(pSatCnf);
}

void repair(Aig_Man_t* SAig) {
	int cnt = 0;
	int prev = pi.idx;
	// while (true) {
	do {
		int k = prev;

		for (; k < numY; k++) {
			if (isConflict(SAig, k)) {
				prev = k;
				break;
			}
		}

		// TODO : better increment than k++ ....

		if (k == numY) {
			// #ifdef DEBUG
			// 	vector<int> assgn(2*numOrigInputs, 0);
			// 	for (int i = 0; i < numX; i++) {
			// 		assgn[i] = pi.X[i];
			// 		assgn[i + numOrigInputs] = 1 - pi.X[i];
			// 	}

			// 	for (int i = 0; i < numY; i++) {
			// 		assgn[i + numX] = pi.Y[i];
			// 		assgn[i + numX + numOrigInputs] = 1 - pi.Y[i];
			// 	}
			// 	evaluateAig(SAig, assgn);

			// 	assert(Aig_ManCo(SAig, 0)->iData == 0);
			// #endif
			// assert(cnt > 0);
			return;
		}

		cout << "rectify : " << it << " counter : " << cnt << " - index : " << k << endl;
		#ifdef DEBUG
			assert(k < numY);
			Aig_Man_t* FAig = PositiveToNormal(SAig);
			assert(Aig_IsPositive(SAig));
			assert(isConflict(SAig, k));
		#endif


		if (options.rectifyProc == 1) {
			Rectify(SAig, k);
		}
		else if (options.rectifyProc == 2) {
			assert(false);
			// rectify2 is buggy!
			Rectify2(SAig, k, options.depth, options.allowUnivQuantify);
		}
		else if (options.rectifyProc == 3) {
			Rectify3(SAig, k, options.depth, options.allowUnivQuantify);
		}
		else {
			assert(false);
		}

		#ifdef DEBUG
			assert(Aig_IsPositive(SAig));
			assert(!isConflict(SAig, k));
			Aig_Man_t* FAig2 = PositiveToNormal(SAig);
			auto miter = Aig_ManCreateMiter(FAig, FAig2, 0);
			auto Ntk = Abc_NtkFromAigPhase(miter);
			auto out = Abc_NtkMiterSat(Ntk, 0, 0, 0, NULL, NULL);
			assert(out == 1);

			Aig_ManStop(FAig);
			Aig_ManStop(FAig2);
			Aig_ManStop(miter);
			Abc_NtkDelete(Ntk);
		#endif

		cnt += 1;
		it += 1;
		Aig_ManCleanup(SAig);
	}
	while (options.fixAllIndices);
}

int checkUnate(Aig_Man_t* SAig, vector<int> &unates, int repairedIndex) {
	Aig_Man_t* quant_SAig = Aig_ManDupSimpleDfs(SAig);

	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	for (int i = 0; i < repairedIndex; i++) {
		vars.push_back(varsYS[i]);
		vars.push_back(varsYS[i] + numOrigInputs);

		funcs.push_back(Aig_ManConst1(quant_SAig));
		funcs.push_back(Aig_ManConst1(quant_SAig));
	}

	patchCo(quant_SAig, Aig_ComposeVec(quant_SAig, Aig_ManCo(quant_SAig, 0)->pFanin0, funcs, vars));

	auto FAig = PositiveToNormal(quant_SAig);

	int x = 0;
	while(true) {
		int cnt = checkUnateSyntacticAll(FAig, unates, repairedIndex);
		if (cnt > 0) {
			x += cnt;
			auto temp = NormalToPositive(FAig);
			setUnatesSaig(temp, unates);
			Aig_ManStop(FAig);
			FAig = PositiveToNormal(temp);
			Aig_ManStop(temp);
			// cout << "Syntactic : " << cnt << endl;
		}
		else {
			break;
		}
	}

	int y;
	int ans = x;
	while ((y = checkUnateSemAll(FAig, unates, repairedIndex)) > 0) {
		// cout << "Semantic : " << y << endl;
		ans += y;
		auto temp = NormalToPositive(FAig);
		setUnatesSaig(temp, unates);
		Aig_ManStop(FAig);
		FAig = PositiveToNormal(temp);
		Aig_ManStop(temp);
	}

	Aig_ManStop(FAig);
	Aig_ManStop(quant_SAig);
	return ans;
}

void setUnatesSaig(Aig_Man_t* SAig, vector<int> &unates) {
	vector<int> vars;
	vector<Aig_Obj_t*> funcs;

	auto pObj = Aig_ManCo(SAig, 0)->pFanin0;

	for (int i = 0; i < numY; i++) {
		if (unates[i] != -1) {
			vars.push_back(varsYS[i]);
			vars.push_back(varsYS[i] + numOrigInputs);

			funcs.push_back(Aig_NotCond(Aig_ManConst1(SAig), unates[i] == 0));
			funcs.push_back(Aig_NotCond(Aig_ManConst1(SAig), unates[i] == 1));
		}
	}

	pObj = Aig_ComposeVec(SAig, pObj, funcs, vars);
	patchCo(SAig, pObj);
}

// k here i input variable id?
void doShannonExp(Aig_Man_t* SAig, int idx) {
	assert(idx <= Aig_ManCiNum(SAig));

	#ifdef DEBUG
		Aig_Man_t* F1 = PositiveToNormal(SAig);
	#endif

	vector<int> vars = {varsYS[idx], varsYS[idx] + numOrigInputs};
	vector<Aig_Obj_t*> funcs = {Aig_ManConst1(SAig), Aig_ManConst0(SAig)};
	vector<Aig_Obj_t*> funcs2 = {Aig_ManConst0(SAig), Aig_ManConst1(SAig)};

	auto pObj = Aig_ManCo(SAig, 0)->pFanin0;

	pObj = Aig_Or(SAig, Aig_ComposeVec(SAig, pObj, funcs, vars), Aig_ComposeVec(SAig, pObj, funcs2, vars));

	patchCo(SAig, pObj);
	Aig_ManCleanup(SAig);

	#ifdef DEBUG
		Aig_Man_t* F2 = PositiveToNormal(SAig);
		auto miter = Aig_ManCreateMiter(F1, F2, 0);
		auto Ntk = Abc_NtkFromAigPhase(miter);
		auto out = Abc_NtkMiterSat(Ntk, 0, 0, 0, NULL, NULL);
		assert(out == 1);

		Aig_ManStop(F1);
		Aig_ManStop(F2);
		Aig_ManStop(miter);
		Abc_NtkDelete(Ntk);
	#endif

	it += 1;
}

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
	auto err_cnf = Cnf_Derive_Wrapper(Formula, 0);

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
	auto ans = Cnf_Derive_Wrapper(formula, 0);
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
	auto ans = Cnf_Derive_Wrapper(formula, 0);
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

	lbool status = sat_solver_solve(solver, NULL, NULL, (ABC_INT64_T)0, (ABC_INT64_T)0,
						(ABC_INT64_T)0, (ABC_INT64_T)0);

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

//assumes DFS Ordering! Ensure patchCo before this!
//fixes varsYS such that correct ordering will follow!
void calcLeastOccurrenceSAig(Aig_Man_t* SAig, int minIdx) {
	if (minIdx == numY)
		return;
	Aig_Obj_t *pObj;
	int i;
	int cnt = (numY - minIdx);
	unordered_map<int, vector<bool>> nodeSupport;
	vector<bool> initVec(cnt, false);
	vector<int> ranks(cnt, 0);

	for (int i = 0; i < numX; ++i)
	{
		nodeSupport[Aig_ManCi(SAig, varsXS[i])->Id] = initVec;
	}
	for (int i = 0; i < numY; ++i)
	{
		vector<bool> updateVec(cnt, false);
		if (i >= minIdx) {
			updateVec[i - minIdx] = true;
		}
		nodeSupport[Aig_ManCi(SAig, varsYS[i])->Id] = updateVec;
		nodeSupport[Aig_ManCi(SAig, varsYS[i] + numOrigInputs)->Id] = updateVec;
	}
	Aig_ManForEachObj(SAig, pObj, i)
	{
		if (Aig_ObjId(pObj) > 2*numOrigInputs)
		{
			vector<bool> updateVec(cnt, false), lVec(cnt, false), rVec(cnt, false);
			if (Aig_ObjFanin0(pObj) != NULL)
				lVec = nodeSupport[Aig_ObjFanin0(pObj)->Id];
			if (Aig_ObjFanin1(pObj) != NULL)
				rVec = nodeSupport[Aig_ObjFanin1(pObj)->Id];
			for (int i = 0; i < numY - minIdx; ++i)
			{
				updateVec[i] = lVec[i] || rVec[i];
				if (updateVec[i])
				{
					ranks[i]++;
				}
			}
			nodeSupport[pObj->Id] = updateVec;
		}
	}
	
	auto it = min_element(ranks.begin(), ranks.end());
	assert(it != ranks.end());

	// update all relevant maps

	swap(varsYS[minIdx + it - ranks.begin()], varsYS[minIdx]);
	swap(varsYF[minIdx + it - ranks.begin()], varsYF[minIdx]);
	// swap(varsCNF[numX + minIdx + it - ranks.begin()], varsCNF[numX + minIdx]);
	swap(unates[minIdx + it - ranks.begin()], unates[minIdx]);
}

Cnf_Dat_t *Cnf_DeriveFast_Wrapper(Aig_Man_t *p, int nOutputs)
{
	auto pCnf = Cnf_DeriveFast(p, nOutputs);

	// 2 passes, one to determine memory to alloc, another to copy things
	// not using any dynamic data structures
	int *beg, *end, i;
	unordered_set<int> excludeClauses;
	unordered_map<int, unordered_set<int *>> excludeLits;

	int newClauses = pCnf->nClauses;
	int newLiterals = pCnf->nLiterals;

	Cnf_CnfForClause(pCnf, beg, end, i)
	{
		bool top = false;
		int redCnt = 0;
		unordered_map<int, bool> presentVars; // map from vars to their signs

		for (auto ptr = beg; ptr != end; ptr++)
		{
			auto var = Lit(*ptr).getVar();
			auto sign = Lit(*ptr).isCompl();
			auto it = presentVars.find(var);
			if (it != presentVars.end())
			{
				if (it->second == sign)
				{
					redCnt++;
					excludeLits[i].insert(ptr);
				}
				else
				{
					top = true;
					excludeClauses.insert(i);
					break;
				}
			}
			else
			{
				presentVars[var] = sign;
			}
		}

		if (top)
		{
			newClauses--;
			newLiterals -= (end - beg);
		}
		else
		{
			newLiterals -= (redCnt);
		}
	}

	auto v = ABC_ALLOC(int *, newClauses + 1);
	v[0] = ABC_ALLOC(int, newLiterals);
	int j = 0;

	Cnf_CnfForClause(pCnf, beg, end, i)
	{
		if (excludeClauses.count(i) == 0)
		{
			auto litsSet = excludeLits[i];
			vector<int> lits;
			for (auto ptr = beg; ptr != end; ptr++)
			{
				if (litsSet.count(ptr) == 0)
				{
					lits.push_back(*ptr);
				}
			}
			memcpy(v[j], lits.data(), sizeof(int) * lits.size());
			v[j + 1] = v[j] + lits.size();
			j++;
		}
	}
	pCnf->nClauses = newClauses;
	pCnf->nLiterals = newLiterals;
	free(pCnf->pClauses[0]);
	free(pCnf->pClauses);
	pCnf->pClauses = v;

	// pVarNums doesn't need to be updated

	// TODO : put debug assertion

	return pCnf;
}

Cnf_Dat_t *Cnf_Derive_Wrapper(Aig_Man_t *p, int nOutputs)
{
	Cnf_Dat_t *ans;

	TIMED(
		overallCnfTime,
		if (options.useFastCnf) {
			ans = Cnf_DeriveFast_Wrapper(p, nOutputs);
		} else {
			ans = Cnf_Derive(p, nOutputs);
		})
	return ans;
}

void dumpResults(Aig_Man_t* SAig, map<int, string> id2NameF) {
	// outFolderPath is the top level path, ends with a / always
	auto unatesFile = options.outFolderPath + "/Unates/" + getFileName(options.benchmark);
	auto pUnates = ofstream(unatesFile + ".pUnates"), nUnates = ofstream(unatesFile + ".nUnates");

	for (int i = 0; i < numY; i++) {
		if (unates[i] == 0) {
			nUnates << id2NameF[Aig_ManCi(SAig, varsYS[i])->Id] << endl;
		}
		else if (unates[i] == 1) {
			pUnates << id2NameF[Aig_ManCi(SAig, varsYS[i])->Id] << endl;
		}
	}

	pUnates.close();
	nUnates.close();

	auto orderingFile = ofstream(options.outFolderPath + "/OrderFiles/" + getFileName(options.varsOrder) + ".final.txt");
	
	for (int i = 0; i < numY; i++) {
		orderingFile << id2NameF[Aig_ManCi(SAig, varsYS[i])->Id] << endl;
	}
	orderingFile.close();

	auto verilogPath = options.outFolderPath + "/Verilogs/" + getFileName(options.benchmark) + ".result.v";

	Aig_Obj_t* pObj;
	int i;
	Aig_ManForEachCi(SAig, pObj, i) {
		
	}

	Aig_ManDumpVerilog(SAig, verilogPath.data());

	// auto aigPath = options.outFolderPath + "/Aigs/" + getFileName(options.benchmark) + ".result.aig";

	// auto vPoNames = Vec_PtrAlloc(2);
	// Vec_PtrPush(vPoNames, "")

	// Aig_ManDumpBlif(SAig, aigPath, )
}
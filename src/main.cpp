#include <iostream>
#include <fstream>
#include <chrono>
#include <cassert>
#include <sstream>
#include <map>
#include <iomanip>

#ifdef WITH_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#endif


#include "preprocessorinterface.hpp"
#include "parserinterface.hpp"
#include "inputreader.hpp"
#include "outputreader.hpp"
#include "timer.hpp"
#include "utility.hpp"
using namespace std;

#include "parseflags.cpp"

int main(int argc, char* argv[]){
	ios_base::sync_with_stdio(0);
	cin.tie(0);

	map<string, int> intVars;
	map<string, bool> boolVars;
	map<string, double> doubleVars;
	map<string, uint64_t> uint64Vars;

	intVars["BBTMS_maxVars"]=200;
	boolVars["hardenInModelSearch"]=false;
	intVars["modelSearchAsearchType"]=1;
	intVars["modelSearchIterLimit"]=-1;
	intVars["MRED_minLitsInClause"]=3;
	intVars["MRED_maxLitsInClause"]=100;
	boolVars["MRED_trimBefore"]=false;
	intVars["MRED_randomizedTries"]=0;
	intVars["LRED_minPartitions"]=1;

	intVars["FLE_redTechniques"]=0;
	doubleVars["FLE_redTechniquesActivate"]=2;


	if ((argc > 1 && isHelp(argv[1])) || (argc > 2 && isHelp(argv[2])) || (argc > 3 && isHelp(argv[3]))) {
		printHelp(cout, intVars, boolVars, doubleVars, uint64Vars, false, false);
		return 0;
	}
	if (argc < 2) {
		printHelp(cout, intVars, boolVars, doubleVars, uint64Vars, true, true);
		cout<<"Use -help for more detailed information"<<endl;
		return 0;
	}
	auto flags = getFlags(argc, argv);

	if (flags.count("h") || flags.count("help")) {
		printHelp(cout, intVars, boolVars, doubleVars, uint64Vars, true, false);
	}

	int verb = parseVerb(flags);
	bool iverb = parseVerbInstance(flags);
	
	bool use_parsing_interface = parseParsingInterface(flags);
	if (use_parsing_interface) cerr << "Testing parse interface" << endl;

	string type(argc==2?"-":argv[2]);
	if (type[0]=='-'){
	    if (verb>0) cerr << "No mode given, defaulting to preprocess." << endl;
		if (iverb>0) cout<<"c No mode given, defaulting to preprocess."<<endl;
	    type = "preprocess";
	}

	assert(type == "solve" || type == "preprocess" || type == "reconstruct");


	string file(argv[1]);
	if (type == "reconstruct") {
		int solutionFormat = parseSolutionFormat(flags, maxPreprocessor::INPUT_FORMAT_WPMS22, -1, verb, iverb);
		int solverSolutionFormat = parseSolverSolutionFormat(flags, maxPreprocessor::INPUT_FORMAT_WPMS22, -1, verb, iverb);
		if (!flags.count("mapfile")) {
			cout<<"Give mapfile with -mapfile= flag"<<endl;
			cerr<<"Give mapfile with -mapfile= flag"<<endl;
			return 0;
		}


		string mapFile = flags["mapfile"];
		maxPreprocessor::OutputReader opr;
		ifstream in(file);
		int readStatus = opr.readSolution(in, solverSolutionFormat);
		in.close();
		if (readStatus > 0) {
			cout<<"Failed to parse solution"<<endl;
			cerr<<"Failed to parse solution"<<endl;
			return 0;
		}
		if (opr.status == 2) {
			cout<<"s UNSATISFIABLE"<<endl;
		}
		else {
			ifstream mapF(mapFile);
			int vars, ppVars, originalVars;
			mapF>>vars>>ppVars>>originalVars;
			vector<int> varMap(vars);
			for (int i = 0; i < vars; i++) mapF>>varMap[i];
			vector<int> trueLits;
			for (int lit : opr.trueLits) {
				if (abs(lit) > vars) continue;
				if (lit > 0) lit = varMap[abs(lit)-1];
				else lit = -varMap[abs(lit)-1];
				trueLits.push_back(lit);
			}
			maxPreprocessor::Trace trace;
			int traceLines;
			mapF>>traceLines;
			trace.operations.resize(traceLines);
			trace.data.resize(traceLines);
			for (int i = 0; i < traceLines; i++) {
				mapF>>trace.operations[i];
				int sz;
				mapF>>sz;
				trace.data[i].resize(sz);
				for (int j = 0; j < sz; j++) {
					mapF>>trace.data[i][j];
				}
			}
			trace.printSolution(cout, trueLits, opr.ansW, ppVars, originalVars, solutionFormat);
		}
		return 0;
	}

	if (type == "solve") {
		// just check that -solver flag is used
		if (!(flags.count("solver") && flags["solver"].size() > 0)) {
			cout<<"Please specify the solver"<<endl;
			cerr<<"Please specify the solver"<<endl;
			return 0;
		}
	}


	string techniques = parseTechniques(flags, verb, iverb);
	double timeLimit = parseTimeLimit(flags, verb, iverb);
	bool BVEgate = parseBVEgate(flags, verb, iverb);
	bool labelMatching = parseLabelMatching(flags, verb, iverb);
	int problemType = parseProblemType(flags, verb, iverb);
	int skipTechnique = parseSkipTechnique(flags, verb, iverb);
	bool BVEsortMaxFirst = parseBVEsortMaxFirst(flags, verb, iverb);
	int BVElocalGrow = parseBVElocalGrow(flags, verb, iverb);
	int BVEglobalGrow = parseBVEglobalGrow(flags, verb, iverb);
	bool proofNoOutput = parseProofNoOutput(flags, verb, iverb);



	pair<unsigned, unsigned> sizeLimit = parseSizeLimit(flags, verb, iverb);
	bool ignoreExitCode = flags.count("ignore-exit-code");
	string prepFile = parsePrepFilename(flags, verb, iverb);
	string solFile = parseSolFilename(flags, verb, iverb);
	string proofFile = parseProofFilename(flags, verb, iverb);
	int proofDebugLevel = parseProofDebugLevel(flags, verb, iverb);

	parseIntVars(flags, intVars, verb, iverb);
	parseBoolVars(flags, boolVars, verb, iverb);
	parseDoubleVars(flags, doubleVars, verb, iverb);
	parseUint64Vars(flags, uint64Vars, verb, iverb);

	maxPreprocessor::InputReader inputReader;
	maxPreprocessor::ParserInterface parser_if;

	int readStatus = 0;
	int readStatus_inf = 0;

	bool fo = 0;
#ifdef WITH_ZLIB
	if (file.size()>3 && file[file.size()-3]=='.' && (file[file.size()-2]=='g' || file[file.size()-2]=='x' )&& file[file.size()-1]=='z') {
		std::ifstream f(file, std::ios::binary);
		std::ifstream f2(file, std::ios::binary);
		fo = 1;
		if (f.fail()) {
			if (iverb>0) cout<<"c Failed to read the input file"<<endl;
			cerr<<"Failed to read the input file"<<endl;
			return 0;
		}
		if (f2.fail()) {
			if (iverb>0) cout<<"c Failed to read the input file2"<<endl;
			cerr<<"Failed to read the input file2"<<endl;
			return 0;
		}

		boost::iostreams::filtering_istreambuf inbf;
		boost::iostreams::filtering_istreambuf inbf2;
		if (file[file.size()-2]=='g') {
			inbf.push(boost::iostreams::gzip_decompressor());
			inbf2.push(boost::iostreams::gzip_decompressor());
		}
		if (file[file.size()-2]=='x') { 
			inbf.push(boost::iostreams::lzma_decompressor()); 
			inbf2.push(boost::iostreams::lzma_decompressor()); 
		}
		inbf.push(f);
		inbf2.push(f2);
    	istream in(&inbf);
		istream in2(&inbf2);
		readStatus = inputReader.readClauses(in, problemType);
		if (use_parsing_interface) {
			assert(problemType == 2);
			readStatus_inf = parser_if.read_file_init_interface(in2);
		}
			
	}
#endif
	if (!fo) {
		ifstream instanceFile(file);
		if (instanceFile.fail()) {
			if (iverb>0) cout<<"c Failed to read the input file"<<endl;
			cerr<<"Failed to read the input file"<<endl;
			return 0;
		}
		readStatus = inputReader.readClauses(instanceFile, problemType);
		instanceFile.close();
		if (use_parsing_interface) {
			ifstream instanceFile_inf(file);
			assert(problemType == 1);
			readStatus = parser_if.read_file_init_interface(instanceFile_inf);
			instanceFile_inf.close();
		}	
	}

	if (readStatus > 0) {
		if (iverb>0) cout<<"c Failed to parse input instance: "<<inputReader.readError<<endl;
		cerr<<"Failed to parse input instance: "<<inputReader.readError<<endl;
		return 0;
	}

	if (readStatus_inf > 0) {
		if (iverb>0) cout<<"c Failed to parse input instance 2: " <<endl;
		cerr<<"Failed to parse input instance 2: " <<endl;
		return 0;
	}

	int outputFormat = parseOutputFormat(flags, inputReader.inputFormat, inputReader.inputFormat, verb, iverb);

	
	maxPreprocessor::PreprocessorInterface pif(inputReader.clauses, inputReader.weights, inputReader.top);
	pif.logProof(proofFile, proofDebugLevel, proofNoOutput);
	pif.setBVEGateExtraction(BVEgate);
	pif.setLabelMatching(labelMatching);
	pif.setSkipTechnique(skipTechnique);
	pif.setBVEsortMaxFirst(BVEsortMaxFirst);
	pif.setBVElocalGrowLimit(BVElocalGrow);
	pif.setBVEglobalGrowLimit(BVEglobalGrow);
	if (intVars["BBTMS_maxVars"]<0) {
		intVars["BBTMS_maxVars"] = (int) ((long long) pif.getOriginalVariables() * (long long)(-intVars["BBTMS_maxVars"]) / 1000000);
	}
	pif.setOptionVariables(intVars, boolVars, doubleVars, uint64Vars);

	if (use_parsing_interface) {
		parser_if.setBVEGateExtraction(BVEgate);	
		parser_if.setLabelMatching(labelMatching);
		parser_if.setSkipTechnique(skipTechnique);
	}

	maxPreprocessor::Timer preprocessTimer;
	preprocessTimer.start();

	// TODO: better size limiting...
	if (inputReader.clauses.size() < sizeLimit.first || sizeLimit.second < inputReader.clauses.size()) timeLimit=0;
	pif.preprocess(techniques, verb, timeLimit);

	if (use_parsing_interface) {
		cerr << "##########PARSER_INTERFACE_PREPROCESS###########" << endl;
		parser_if.preprocess(techniques, verb, timeLimit);
		cerr << "###############################################" << endl;
	}

	preprocessTimer.stop();
	if (verb > 0) pif.printPreprocessorStats(cerr);
	if (verb > 0 && use_parsing_interface) {
		cerr << "##########PARSER_INTERFACE_PREPROCESS###########" << endl;
		parser_if.printPreprocessorStats(cerr);
		cerr << "###############################################" << endl;

	}

	if (verb>0) cerr<<"Preprocess time: "<<preprocessTimer.getTime().count()<<endl;
	if (verb > 0) pif.printTimeLog(cerr);
	if (type == "preprocess") {
		if (verb > 0) pif.printTechniqueLog(cerr);
		if (verb > 0) cerr << "LB " << pif.getRemovedWeight()[0] << endl;
		if (iverb>0) pif.printTechniqueLog(cout);
		if (iverb>0) pif.printTimeLog(cout);
		if (iverb>0) pif.printInfoLog(cout);
		pif.printInstance(cout, outputFormat);

		if (use_parsing_interface) {
			cerr << "##########PARSER_INTERFACE_LOGS###########" << endl;
			if (verb > 0) parser_if.printTechniqueLog(cerr);
			if (verb > 0) cerr << "LB " << parser_if.get_lb() << endl;
			if (iverb>0) parser_if.printTechniqueLog(cout);
			if (iverb>0) parser_if.printTimeLog(cout);
			if (iverb>0) parser_if.printInfoLog(cout);
			cerr << "################################################" << endl;
		}

		if (flags.count("mapfile")) {
			string mapFile = flags["mapfile"];
			if (iverb>0) cout<<"c Outputting reconstruction map to "<<mapFile<<endl;
			if (verb>0)  cerr<<"Outputting reconstruction map to "<<mapFile<<endl;
			ofstream out(mapFile);
			pif.printMap(out);
			out.close();
		}
		else {
			if (iverb>0) cout<<"c No -mapfile= given, will not ouput reconstruction map"<<endl;
			if (verb>0)  cerr<<"No -mapfile= given, will not ouput reconstruction map"<<endl;
		}
	}
	if (type == "solve") {
		int solutionFormat = parseSolutionFormat(flags, outputFormat, outputFormat, verb, iverb);
		int solverSolutionFormat = parseSolverSolutionFormat(flags, outputFormat, outputFormat, verb, iverb);
		string solver;
		if (flags.count("solver") && flags["solver"].size() > 0) {
			solver = flags["solver"];
			if (iverb>0) cout<<"c Using solver "<<solver<<endl;
			if (verb>0)  cerr<<"Using solver "<<solver<<endl;
			if (iverb>0) cout<<"c Solver flags "<<flags["solverflags"]<<endl;
			if (verb>0)  cerr<<"Solver flags "<<flags["solverflags"]<<endl;
		}
		else {
			if (iverb>0) cout<<"c Please specify the solver"<<endl;
			cerr<<"Please specify the solver"<<endl;
			return 0;
		}

		if (iverb>0) cout << "c Saving preprocessed instance into file " << prepFile << endl;
		if (verb>0)  cerr << "Saving preprocessed instance into file " << prepFile << endl;
		ofstream out(prepFile);
		pif.printInstance(out, outputFormat);
		out.close();

		if (use_parsing_interface) {
			string parse_prepFile = "parse_" + prepFile;
			if (iverb>0) cout << "c Saving preprocessed instance into file " << parse_prepFile << endl;
			if (verb>0)  cerr << "Saving preprocessed instance into file " << parse_prepFile << endl;
			ofstream out2(parse_prepFile);
			parser_if.printInstance(out2, outputFormat);
			out2.close();
		}

		string command = (solver + " " + prepFile +" " + flags["solverflags"] + " > " + solFile);
		if (iverb>0) cout << "c Invoking solver... command: "  << command << endl;
		if (verb>0)  cerr << "Invoking solver... command: " << command << endl;

		maxPreprocessor::Timer solveTimer;
		solveTimer.start();
		int rv = system(command.c_str());
		int exit_status = (WEXITSTATUS(rv));
		if (exit_status && !ignoreExitCode) {
			if (iverb>0) cout << "c Solver error, returned nonzero value (" << exit_status << ")" << endl;
			cerr << "Solver error, returned nonzero value (" << exit_status << ")" << endl;
			return 0;
		}
		solveTimer.stop();

		if (use_parsing_interface) {
			string parse_prepFile = "parse_" + prepFile;
			string parse_solFile = "parse_" + solFile;
			string command = (solver + " " + parse_prepFile +" " + flags["solverflags"] + " > " + parse_solFile);
			if (iverb>0) cout << "c Invoking solver... command: "  << command << endl;
			if (verb>0)  cerr << "Invoking solver... command: " << command << endl;

			maxPreprocessor::Timer solveTimer;
			solveTimer.start();
			int rv = system(command.c_str());
			int exit_status = (WEXITSTATUS(rv));
			if (exit_status && !ignoreExitCode) {
				if (iverb>0) cout << "c Solver error on second round, returned nonzero value (" << exit_status << ")" << endl;
				cerr << "Solver error, returned nonzero value (" << exit_status << ")" << endl;
				return 0;
			}
			solveTimer.stop();
		}


		maxPreprocessor::OutputReader opr;
		ifstream in(solFile);
		readStatus = opr.readSolution(in, solverSolutionFormat);
		in.close();
		if (readStatus > 0) {
			if (iverb>0) cout<<"c Failed to parse solution"<<endl;
			cerr<<"Failed to parse solution"<<endl;
			return 0;
		}

		if (opr.status == 2) {
			cout<<"s UNSATISFIABLE"<<endl;
		}
		else {
			pif.printSolution(opr.trueLits, cout, opr.ansW, solutionFormat);
		}
		if (verb > 0) cerr<<"Preprocess time: "<<preprocessTimer.getTime().count()<<", Solve time: "<<solveTimer.getTime().count()<<endl;
		if (verb > 0) pif.printTimeLog(cerr);

		if (use_parsing_interface) {
			maxPreprocessor::OutputReader opr2;
			string parse_solFile = "parse_" + solFile;
			ifstream in2(parse_solFile);
			readStatus = opr2.readSolution(in2, solverSolutionFormat);
			in2.close();
			if (readStatus > 0) {
				if (iverb>0) cout<<"c Failed to parse solution second round"<<endl;
				cerr<<"Failed to parse solution round 2"<<endl;
				return 0;
			}

			if (opr2.status == 2) {
				cout<<"s UNSATISFIABLE"<<endl;
			}
			else {
				cout<<"s SECOND ROUND SAT"<<endl;
			}
		}
	}
}

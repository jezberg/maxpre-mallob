#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>

#include "inputreader.hpp"
#include "parserinterface.hpp"
#include "global.hpp"
#include "utility.hpp"

using namespace std;
namespace maxPreprocessor {
	ParserInterface::ParserInterface() {
		pif = NULL;
		has_preprocessed = false;
	}

	int ParserInterface::read_file_init_interface(istream& input_file) {
		maxPreprocessor::InputReader inputReader;
		int readStatus = inputReader.readClauses(input_file, 1);
		
		if (readStatus > 0) {
			cerr<<"Failed to parse input instance: "<<inputReader.readError<<endl;
			return readStatus;
		}
		pif = new maxPreprocessor::PreprocessorInterface (inputReader.clauses, inputReader.weights, inputReader.top, false);
		return readStatus;
	}

	/*
	* Many of the functions require the interface to not be null, auxiliary method to check this
	*/
	bool ParserInterface::pif_ok(string calling_func) {
		if (pif == NULL) {
			cerr << "ERROR calling " << calling_func << " when preprocessor interface is NULL. Call read_file_init_interface first "  << endl; 
			return false;
		}
		return true; 
	}

	uint64_t ParserInterface::get_ub() {
		if (!pif_ok("get_lb")) {
			return 0; 
		}
		return pif->getUpperBound(); 
	}


	uint64_t ParserInterface::get_lb() {
		if (!pif_ok("get_lb")) {
			return 0; 
		}
		// 0 index here because maxpre supports multiobjective. 
		return (pif->getRemovedWeight())[0]; 
	}

	std::vector<int> ParserInterface::reconstruct(const std::vector<int>& trueLiterals, bool convertLits)  {
		if (pif_ok("reconstruct")) 
			return pif->reconstruct(trueLiterals, convertLits); 
		else {
			vector<int> dummy;
			return dummy;
		}
	}


	void ParserInterface::preprocess(string techniques, int logLevel, double timeLimit) {
		if (!pif_ok("preprocess")) {
			has_preprocessed = false;
			return; 
		}
		pif->preprocess(techniques,logLevel, timeLimit);
		has_preprocessed = true; 
		return; 
	}


	void ParserInterface::getInstance(std::vector<int>& ret_compressed_clauses, std::vector<std::pair<uint64_t, int>> & ret_objective, bool addRemovedWeight) {
		if (!has_preprocessed) {
			cerr << "Warning, calling get initialize without preprocessing, output structures not altered " << endl;
			return;
		}
		assert(ret_compressed_clauses.size() == 0);
		assert(ret_objective.size() == 0);

		vector<std::vector<int> > retClauses; 
		vector<uint64_t> retWeights;
		vector<int> retLabels;
		pif->getInstance(retClauses, retWeights, retLabels, addRemovedWeight, false);

		uint64_t top = pif->getTopWeight();

    	assert(retClauses.size() == retWeights.size());
  
		for (size_t i = 0; i < retClauses.size(); i++) {
			uint64_t weight = retWeights[i];
			if (weight < top) {
				//SOFT 
				assert(retClauses[i].size() == 1);
				assert(weight > 0);
				ret_objective.push_back(make_pair(weight, (-1) * retClauses[i][0]));
        	}
			else {
				for (size_t cl_ind = 0 ; cl_ind < retClauses[i].size(); i++) {
					ret_compressed_clauses.push_back(retClauses[i][cl_ind]);
				}
				ret_compressed_clauses.push_back(0);
			}			
		}
		return;
	}
}

	

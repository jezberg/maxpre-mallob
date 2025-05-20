#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>

#include "inputreader.hpp"
#include "parserinterface.hpp"
#include "global.hpp"
#include "utility.hpp"
#include "tmpdir.hpp"

using namespace std;
namespace maxPreprocessor {
	ParserInterface::ParserInterface() {
		has_preprocessed = false;
	}

	int ParserInterface::read_file_init_interface(istream& input_file) {
		maxPreprocessor::InputReader inputReader;
		int readStatus = inputReader.readClauses(input_file, 1);
		
		if (readStatus > 0) {
			cerr<<"Failed to parse input instance: "<<inputReader.readError<<endl;
			return readStatus;
		}

		assert(inputReader.weights.size() == inputReader.clauses.size());
		for (size_t i = 0; i < inputReader.clauses.size(); i++) {
			auto& clauseVec = inputReader.clauses[i];
			auto& weights = inputReader.weights[i];
			assert(weights.size() <= 1);
			if (weights.size() > 0 && weights[0] < inputReader.top) {
				// SOFT
				originalSoftClauses.push_back({weights[0], clauseVec});
			}
		}

		pif.reset(new maxPreprocessor::PreprocessorInterface (inputReader.clauses, inputReader.weights, inputReader.top, false));
		return readStatus;
	}

	void ParserInterface::setTmpDirectory(const std::string& tmpDirectory) {
		TmpDir::directory = tmpDirectory;
	}

	/*
	* Many of the functions require the interface to not be null, auxiliary method to check this
	*/
	bool ParserInterface::pif_ok(string calling_func) {
		if (pif == NULL) {
			cerr << "ERROR calling " << calling_func << " when preprocessor interface is NULL. Call read_file_init_interface first. "  << endl; 
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
		// Can also be empty for pathological instances without soft clauses.
		auto removedWeight = pif->getRemovedWeight();
		return removedWeight.empty() ? 0 : removedWeight[0];
	}

	std::vector<int> ParserInterface::reconstruct(const std::vector<int>& trueLiterals, size_t* outCostOrNullptr,
			int preprocessingLayer, bool convertLits, int leadingZeroes)  {
		if (pif_ok("reconstruct")) {
			// Construct a full coherent trace using all of the saved incremental traces
			// up until the specified preprocessing layer the solution is coming from
			mtx_reconstruction.lock();
			Trace trace;
			for (int layer = 0; layer <= preprocessingLayer; layer++) {
				// Append all incremental operations and data to our full trace
				auto& img = reconstruction_images.at(layer);
				trace.operations.insert(trace.operations.end(), img.trace.operations.begin(), img.trace.operations.end());
				trace.data.insert(trace.data.end(), img.trace.data.begin(), img.trace.data.end());
			}
			// Copy the preprocessing image of the specified layer and insert the full trace
			PreprocessorInterface::PPImage img = reconstruction_images.at(preprocessingLayer);
			mtx_reconstruction.unlock();
			trace.removedWeight = std::move(img.trace.removedWeight);
			img.trace = std::move(trace);
			// Perform the actual reconstruction using our modified image
			auto solution = pif->reconstruct(trueLiterals, convertLits, &img, leadingZeroes);

			if (outCostOrNullptr) {
				// Compute the solution's cost w.r.t. the original problem instance
				size_t cost = 0;
				for (auto& [weight, lits] : originalSoftClauses) {
					bool satisfied = false;
					for (int lit : lits) {
						int var = std::abs(lit);
						assert(var > 0);
						assert(var < solution.size());
						int solutionLit = solution[var];
						assert(solutionLit == var || solutionLit == -var);
						if (solutionLit == lit) {
							satisfied = true;
							break;
						}
					}
					if (!satisfied) cost += weight;
				}
				*outCostOrNullptr = cost;
			}

			return solution;
		} else {
			vector<int> dummy;
			return dummy;
		}
	}


	void ParserInterface::preprocess(string techniques, int logLevel, double timeLimit) {
		if (pif_ok("preprocess")) {
			pif->preprocess(techniques,logLevel, timeLimit);
			has_preprocessed = true;
		} else {
			has_preprocessed = false;
		}
	}

	bool ParserInterface::lastCallInterrupted() {
		if (!pif_ok("last_call_interrupted")) {
			return false;
		}
		return pif->lastCallInterrupted();
	}


	void ParserInterface::getInstance(std::vector<int>& ret_compressed_clauses, std::vector<std::pair<uint64_t, int>> & ret_objective,
			int& ret_nbVars, int& ret_nbClauses, bool addRemovedWeight) {
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
  
		ret_nbVars = 0;
		ret_nbClauses = 0;

		for (size_t i = 0; i < retClauses.size(); i++) {
			uint64_t weight = retWeights[i];

			if (weight < top) {
				//SOFT 
				assert(retClauses[i].size() == 1);
				assert(weight > 0);
				ret_objective.push_back(make_pair(weight, (-1) * retClauses[i][0]));
				ret_nbVars = std::max(ret_nbVars, std::abs(retClauses[i][0]));
        	}
			else {
				for (size_t cl_ind = 0 ; cl_ind < retClauses[i].size(); cl_ind++) {
					ret_compressed_clauses.push_back(retClauses[i][cl_ind]);
					ret_nbVars = std::max(ret_nbVars, std::abs(retClauses[i][cl_ind]));
				}
				ret_compressed_clauses.push_back(0);
				ret_nbClauses++;
			}			
		}

		// Store the operations and data needed to reconstruct a solution at this preprocessing stage
		mtx_reconstruction.lock();
		auto img = pif->getImageForIncrementalReconstruction(
			reconstruction_images.empty() ? PreprocessorInterface::PPImage() : reconstruction_images.back());
		reconstruction_images.push_back(std::move(img));
		mtx_reconstruction.unlock();
	}
	/*
	* Print the preprocessed instance, mainly used for testing the "getInstance method of this class."
	*/
	void ParserInterface::printInstance(std::ostream& output, int outputFormat) {
		if (!pif_ok("printInstance")) return;
		
		std::vector<int> clauses;
		std::vector<std::pair<uint64_t, int>> objective;
		int nbVars, nbClauses;
		getInstance(clauses, objective, nbVars, nbClauses, true);


		assert(outputFormat == INPUT_FORMAT_WPMS22);

		if (get_ub() !=  HARDWEIGHT) output << "c UB " << get_ub() << "\n";

		output<<"c objective ";
		for (unsigned i = 0; i < objective.size(); i++) {
			int lit = objective[i].second;
			uint64_t coeff = objective[i].first;
			output << coeff << "*" << lit;
			if (i + 1 < objective.size()) {
				output<<" ";
			}
		}
		
		output<<"\n";
		
		output<<"h  ";
		for (unsigned i = 0 ; i < clauses.size(); i++) {
			int lit = clauses[i];
			output<<lit;
			if (lit == 0) {
				output<<"\n";
				if (i + 1 < clauses.size()) {
					output<<"h  ";
				}
			}
			else {
				output << " ";
			}
				
		}
		

		for (auto term: objective) {
			int lit = term.second;
			uint64_t coeff = term.first;
			output<<coeff<< " " << (-1)*lit << " 0\n";
		}
		
		output.flush();
	};
}

	

#ifndef MAXPP_PARSERINTERFACE_HPP
#define MAXPP_PARSERINTERFACE_HPP

#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>

#include "preprocessorinterface.hpp"

namespace maxPreprocessor {
class ParserInterface {
private:
	std::unique_ptr<maxPreprocessor::PreprocessorInterface> pif;
public:
	/*
	* the constructor simply initializez the reader and the preprocessor interface. 
	*/
	ParserInterface();
	~ParserInterface() {}

private: 
	// Check if the interface has been initialized. 
	bool pif_ok(string error_msg);
	bool has_preprocessed;

	// Guards reconstruction_images to allow solution reconstruction concurrently
	// to preprocessing.
	std::mutex mtx_reconstruction;
	std::vector<PreprocessorInterface::PPImage> reconstruction_images;

	std::vector<std::pair<size_t, std::vector<int>>> originalSoftClauses;

public:
	/*
	* read the file into the input reader,
	*/
	int read_file_init_interface(istream& input);

	void setTmpDirectory(const std::string& tmpDirectory);

	// Returns the current instance, give the data structures as parameters. 
	// Specifically, ret_objective will contain pairs of uint_64 and int, representing the coefficient and the literal. 
	// If the literal represented by the int is true, a cost equal to the coefficient is incurred.
	// If "addRemovedWeight = 1" the returned objective will contain a fresh variable x with a coefficient exactly equal 
	// to the lower bound proved by the preprocessor, and the clauses will contain a unit clause (not x). Essentially, x is then a trivial core
	// If addRemovedWeight = 0, any cost computed by the objective should be increased by get_lb() in order to get the cost of the original instance. 
	void getInstance(std::vector<int>& ret_compressed_clauses, std::vector<std::pair<uint64_t, int>> & ret_objective, int& ret_nbVars, int& ret_nbClauses, bool addRemovedWeight= 1);
	
	/* Preprocesses the current maxsat instance with the given techniques
	 * string, loglevel and timelimit.
	 */
	void preprocess(std::string techniques, int logLevel = 0, double timeLimit = 1e9);

	/* Permanently interrupts preprocessing for this instance. Can be called asynchronously to an ongoing call.
	 * Preprocessing will then be interrupted at the earliest possible point.
	 */
	void interruptAsynchronously() {
		pif->interruptAsynchronously();
	}

	bool lastCallInterrupted();

	uint64_t get_lb();
	uint64_t get_ub();

	// Functions for enabling/disabling some functionality
	void setBVEGateExtraction(bool use) {if (pif_ok("setBVEGateExtraction")) pif->setBVEGateExtraction(use);};
	void setLabelMatching(bool use) {if (pif_ok("setLabelMatching")) pif->setLabelMatching(use);};
	// Set value=0 to disable skiptechnique
	void setSkipTechnique(int value) {if (pif_ok("setSkipTechnique")) pif->setSkipTechnique(value);};

	// lazy way of setting option variables...
	void setOptionVariables(map<string, int>& intVars, map<string, bool>& boolVars, map<string, double>& doubleVars, map<string, uint64_t>& uint64Vars)
		 {if (pif_ok("setOptionVariables")) pif->setOptionVariables(intVars, boolVars,doubleVars, uint64Vars);};

	/* Returns the assignment of the original variables given the assignment of
	 * variable in the solution of the preprocessed nistance
	 */
	std::vector<int> reconstruct(const std::vector<int>& trueLiterals, size_t* outCostOrNullptr, int preprocessingLayer, bool convertLits = true, int leadingZeroes = 0);


	void printInstance(std::ostream& output, int outputFormat = 0);
	void printSolution(const std::vector<int>& trueLiterals, std::ostream& output, uint64_t ansWeight, int outputFormat) {if (pif_ok("printSolution")) pif->printSolution(trueLiterals, output, ansWeight, outputFormat);};
	void printMap(std::ostream& output)  {if (pif_ok("printMap")) pif->printMap(output);};
	void printTechniqueLog(std::ostream& output)  {if (pif_ok("printTechniqueLog")) pif->printTechniqueLog(output);};
	void printTimeLog(std::ostream& output)  {if (pif_ok("printTimeLog")) pif->printTimeLog(output);};
	void printInfoLog(std::ostream& output)  {if (pif_ok("printInfoLog")) pif->printInfoLog(output);};
	void printPreprocessorStats(std::ostream& output) {if (pif_ok("printPreprocessorStats")) pif->printPreprocessorStats(output);};
};
}
#endif

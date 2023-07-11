// Failed and redundant literal detection and elimination
// equivalence detection and redundant equivalence detection
//  (x->y and neg(x)->neg(y))
// forced value detection and redundant forced value detection
//  (x->y and neg(x)->y)
inline bool commonLitExists(const vector<int>& a, const vector<int>& b, int ignore1 = -1, int ignore2 = -1) {
	// assumes a and b are sorted
	if (a.size() < b.size()) {
		for (int l : a) {
			if (l==ignore1 || l==ignore2) continue;
			if (binary_search(b.begin(), b.end(), l)) return 1;
		}
	} else {
		for (int l : b) {
			if (l==ignore1 || l==ignore2) continue;
			if (binary_search(a.begin(), a.end(), l)) return 1;
		}
	}
	return 0;
}


inline void push_sorted(vector<int>& vec, int v) {
	vec.push_back(0);
	for (int i = vec.size() - 1; i>0; --i) {
		if (vec[i-1] > v) vec[i] = vec[i-1];
		else {
			vec[i] = v;
			return;
		}
	}
	vec[0] = v;
}

bool Preprocessor::testBinaryFPR(int x, int y, const vector<int>& up_neg_x, const vector<int>& up_neg_y, bool fullFilter) {
	//test if {x, y} is redunant clause
	if (binary_search(up_neg_x.begin(), up_neg_x.end(), y)) return 1;
	if (binary_search(up_neg_y.begin(), up_neg_y.end(), x)) return 1;

	bool negX=0;
	bool negXY = 0;
	bool XnegY = 0;
	bool negXnegY = 0;
	for (int c : pi.litClauses[litNegation(x)]) {
		if (commonLitExists(up_neg_y, pi.clauses[c].lit, litNegation(x), litNegation(y))) continue;
		if (commonLitExists(up_neg_x, pi.clauses[c].lit, litNegation(x), litNegation(y))) continue;

		if (fullFilter) {
			vector<int> a(pi.clauses[c].lit);
			bool fy = 0;
			for (int& l : a) {
				if (litVariable(l) == litVariable(y)) {
					fy = 1;
					l = litNegation(y);
				} else if (l != litNegation(x)) l = litNegation(l);
			}
			if (!fy) a.push_back(litNegation(y));
			if (!satSolver->testUPConflict(a)) continue; // was unsat, untouched(c) is implied, clause can be filtered
		}
		if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), y)) {
			negXY = 1;
			if (negXnegY) negX = 1;
		} else if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), litNegation(y))) {
			negXnegY = 1;
			if (negXY) negX = 1;
		} else {
			negX = 1;
		}
	}
	if (!negXY && !negX) return 1; // {not x, y} satisfies

	for (int c : pi.litClauses[litNegation(y)]) {
		if (commonLitExists(up_neg_x, pi.clauses[c].lit, litNegation(x), litNegation(y))) continue;
		if (commonLitExists(up_neg_y, pi.clauses[c].lit, litNegation(x), litNegation(y))) continue;

		if (fullFilter) {
			vector<int> a(pi.clauses[c].lit);
			bool fx = 0;
			for (int& l : a) {
				if (litVariable(l) == litVariable(x)) {
					fx = 1;
					l = litNegation(x);
				} else if (l != litNegation(y)) l = litNegation(l);
			}
			if (!fx) a.push_back(litNegation(x));
			if (!satSolver->testUPConflict(a)) continue; // was unsat, untouched(c) is implied, clause can be filtered
		}
		if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), x)) {
			if (negX) return 0;
			XnegY = 1;
		} else if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), litNegation(x))) {
			if (negXY && XnegY) return 0;
			if (negXY) negX = 1;
		} else {
			if (negX || negXY) return 0;
		}
	}
	return 1;
}


bool Preprocessor::testBinaryRedundancy(int lit, int l, const vector<int>& up_neg_lit, const vector<int>& eqLits, int redTechniques) {
	assert(!pi.isVarRemoved(litVariable(lit)));
	assert(!pi.isVarRemoved(litVariable(l)));

	bool allsat=1;
	for (int c : pi.litClauses[litNegation(l)]) {
		if (!commonLitExists(up_neg_lit, pi.clauses[c].lit, litNegation(l))) {
			allsat=0;
			break;
		}
	}
	if (allsat) {
		stats["FLE_binRED_UP"]+=1;
		return 1;
	}
	if (!redTechniques) return 0;
	if (redTechniques&1) {
		vector<int> up_neg_l;
		if (redTechniques&2) {
			vector<int> a={litNegation(l)};
			if (!satSolver->propagate(a, up_neg_l)) {
				stats["FLE_success_accidental"]+=1;
				rLog.removeVariable(1);
				satSolver->addClause(l);
				rLog.removeClause(setVariable(litVariable(l), litValue(l)));
				return 0;
			}
			sort(up_neg_l.begin(), up_neg_l.end());
		}
		if (testBinaryFPR(lit, l, up_neg_lit, up_neg_l, redTechniques&4)) {
			stats["FLE_binRED_FPR"]+=1;
			return 1;
		}
	}
	if (!(redTechniques&8)) return 0;
	if (!rLog.requestTime(Log::Technique::FLE)) return 0;
	vector<int> tmp;
	vector<int> cl={lit, l};
	if (cl[0]>cl[1]) swap(cl[0], cl[1]);
	if (checkExtendedPositiveReduct(cl, tmp, eqLits)) {
		stats["FLE_binRED_extFPR"]+=1;
		return 1;
	}
	return 0;
}


int Preprocessor::tryFLE(int lit, vector<int>& up, bool doRLE) {
	vector<int> a={lit};
	if (!satSolver->propagate(a, up)) {
		stats["FLE_FLE_success"]+=1;
		if (pi.isLabelVar(litVariable(lit))) {
			rLog.removeLabel(1);
		} else {
			rLog.removeVariable(1);
		}
		satSolver->addClause(litNegation(lit));
		int removed = setVariable(litVariable(lit), !litValue(lit));
		rLog.removeClause(removed);
		return 1;
	}
	bool flit=0;
	for (unsigned i=0; i<up.size(); ++i) {
		if (pi.isVarRemoved(litVariable(up[i]))) {
			up[i--]=up.back();
			up.pop_back();
			continue;
		}
		if (up[i]==lit) flit=1;
	}
	if (!flit) up.push_back(lit);

	sort(up.begin(), up.end());

	assert(binary_search(up.begin(), up.end(), lit));

	if (doRLE && !pi.isLitLabel(lit)) {
		bool allsat = 1;
		for (int c : pi.litClauses[lit]) {
			if (!commonLitExists(up, pi.clauses[c].lit, lit)) {
				allsat=0;
				break;
			}
		}
		if (allsat) {
			stats["FLE_RLE_success"]+=1;
			if (pi.isLabelVar(litVariable(lit))) rLog.removeLabel(1);
			else                                 rLog.removeVariable(1);
			satSolver->addClause(litNegation(lit));
			int removed = setVariable(litVariable(lit), !litValue(lit));
			rLog.removeClause(removed);
			return 1;
		}
	}
	return 0;
}

void Preprocessor::replaceLit(int l, int lit) {
	// replace l with lit on every clause
	// handles also labels
	vector<int> clauses = pi.litClauses[l];
	for (int c : clauses) {
		if (!pi.clauses[c].isHard()) {
			assert(pi.clauses[c].lit.size() == 1);
			// handling labels
			if (pi.isLabelVar(litVariable(lit))) {
				if (pi.isLitLabel(litNegation(lit))) {
					int cl = pi.litClauses[litNegation(lit)][0];
					trace.removeWeight(pi.substractWeights(cl, c));
					if (pi.clauses[cl].isWeightless()) {
						if (!pi.isLitLabel(lit) && !pi.clauses[c].isWeightless()) {
							// reuse clause
							pi.replaceLiteralInClause(litNegation(lit), lit, cl);
							swap(pi.litClauses[lit][0], pi.litClauses[lit].back());
							pi.updateLabelMask(litVariable(lit));
						} else pi.removeClause(cl);
					}
				}
				if (!pi.clauses[c].isWeightless()) {
					if (!pi.isLitLabel(lit)) {
						pi.addClause({lit}, {0});
						swap(pi.litClauses[lit][0], pi.litClauses[lit].back());
					}

					int cl = pi.litClauses[lit][0];
					pi.pourAllWeight(c, cl);
				}
				pi.updateLabelMask(litVariable(lit));
				pi.removeClause(c);
			} else {
				// make lit a label
				pi.replaceLiteralInClause(l, lit, c);
				swap(pi.litClauses[lit][0], pi.litClauses[lit].back());
				pi.updateLabelMask(litVariable(lit));
			}
			pi.unLabel(litVariable(l));
			continue;
		}

		// handling nonlabels

		if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), litNegation(lit))) {
			pi.removeClause(c);
		} else if (binary_search(pi.clauses[c].lit.begin(), pi.clauses[c].lit.end(), lit)) {
			pi.removeLiteralFromClause(l, c);
		} else {
			pi.replaceLiteralInClause(l, lit, c);
		}
	}
}

void Preprocessor::handleEqLits(vector<int>& lits) {
	// select from lits the variable that has most clauses
	int liti=0;
	int nofcl=pi.litClauses[lits[0]].size()+pi.litClauses[litNegation(lits[0])].size();
	for (unsigned i=1; i<lits.size(); ++i) {
		if (pi.litClauses[lits[i]].size() + pi.litClauses[litNegation(lits[i])].size() > (unsigned)nofcl) {
			nofcl=pi.litClauses[lits[i]].size() + pi.litClauses[litNegation(lits[i])].size();
			liti=i;
		}
	}

	int lit=lits[liti];
	lits[liti]=lits.back(); lits.pop_back();

	// replace all other variables with the selected one
	vector<int> clauses;
	for (int l : lits) {
		if (pi.isLabelVar(litVariable(l))) {
			rLog.removeLabel(1);
		} else {
			rLog.removeVariable(1);
		}

		replaceLit(l, lit);
		replaceLit(litNegation(l), litNegation(lit));
		// on sat solver, only add l->lit and neg(l)->neg(lit)

		trace.setEqual(lit, l);
		assert(pi.isVarRemoved(litVariable(l)));
	}
}


int Preprocessor::tryFLE(int var, bool doRLE, bool findEqs, bool findRedEqs, bool findForced, bool findRedForced, int redTechniques) {
	assert(findEqs || !findRedEqs);
	assert(findForced || !findRedForced);

	vector<int> up1;
	vector<int> up2;
	if (tryFLE(posLit(var), up1, doRLE)) return 1;
	if (tryFLE(negLit(var), up2, doRLE)) return 1;
	vector<int> eqs;
	set<int> uvars;

	assert(binary_search(up2.begin(), up2.end(), negLit(var)));
	assert(binary_search(up1.begin(), up1.end(), posLit(var)));

	int removed=0;
	if (up1.size() < up2.size() && (findEqs || findForced)) {
		for (int l : up1) {
			if (findEqs && binary_search(up2.begin(), up2.end(), litNegation(l))) {
				eqs.push_back(l);
				uvars.insert(litVariable(l));
				if (l!=posLit(var)) stats["FLE_Eqs"]+=1;
			} else if (findForced && binary_search(up2.begin(), up2.end(), l)) {
				satSolver->addClause(l);
				if (pi.isLabelVar(litVariable(l))) rLog.removeLabel(1);
				else                               rLog.removeVariable(1);
				removed+=setVariable(litVariable(l), litValue(l));
				uvars.insert(litVariable(l));
				stats["FLE_ForcedValues"]+=1;
			}
		}
	} else if (findEqs || findForced) {
		for (int l : up2) {
			if (findEqs && binary_search(up1.begin(), up1.end(), litNegation(l))) {
				eqs.push_back(litNegation(l));
				uvars.insert(litVariable(l));
				if (l!=negLit(var)) stats["FLE_Eqs"]+=1;
			}
			if (findForced && binary_search(up1.begin(), up1.end(), l)) {
				satSolver->addClause(l);
				if (pi.isLabelVar(litVariable(l))) rLog.removeLabel(1);
				else                               rLog.removeVariable(1);
				removed+=setVariable(litVariable(l), litValue(l));
				uvars.insert(litVariable(l));
				stats["FLE_ForcedValues"]+=1;
			}
		}
	}
	if ((findRedEqs || findRedForced) && !pi.isLabelVar(var)) {
		for (int l : up1) {
			if (l==posLit(var)) continue;
			if (uvars.count(litVariable(l))) continue;
			if (pi.isLabelVar(litVariable(l))) continue;
			if (pi.isVarRemoved(litVariable(l))) continue;
			if (findRedForced) {
				// lit -> l   is known
				// check if neg(lit) -> l is redundant
				if (testBinaryRedundancy(posLit(var), l, up2, eqs, redTechniques)) {
					satSolver->addClause(l);
					uvars.insert(litVariable(l));
					rLog.removeVariable(1);
					removed+=setVariable(litVariable(l), litValue(l));
					continue;
				}
			}
			if (pi.isVarRemoved(litVariable(l))) continue;
			if (findRedEqs) {
				// lit -> l is known
				// check if neg(lit) -> neg(l) is redundant
				if (testBinaryRedundancy(posLit(var), litNegation(l), up2, eqs, redTechniques)) {
					satSolver->addClause(posLit(var), litNegation(l));
					if (posLit(var)<litNegation(l))	pi.addClause({posLit(var), litNegation(l)});
					else                            pi.addClause({litNegation(l), posLit(var)});
					push_sorted(up2, litNegation(l));
					uvars.insert(litVariable(l));
					eqs.push_back(l);
					stats["FLE_RedEqs"]+=1;
				}
			}
		}
		for (int l : up2) {
			if (l==negLit(var)) continue;
			if (uvars.count(litVariable(l))) continue;
			if (pi.isLabelVar(litVariable(l))) continue;
			if (pi.isVarRemoved(litVariable(l))) continue;
			if (findRedForced) {
				// neg(lit) -> l is known
				// check if lit -> l is redundant
				if (testBinaryRedundancy(negLit(var), l, up1, eqs, redTechniques)) {
					satSolver->addClause(l);
					uvars.insert(litVariable(l));
					rLog.removeVariable(1);
					removed+=setVariable(litVariable(l), litValue(l));
					continue;
				}
			}
			if (pi.isVarRemoved(litVariable(l))) continue;
			if (findRedEqs) {
				// neg(lit) -> l is known
				// check if lit -> neg(l) is redundant
				if (testBinaryRedundancy(negLit(var), litNegation(l), up1, eqs, redTechniques)) {
					satSolver->addClause(negLit(var), litNegation(l));
					if (negLit(var)<litNegation(l))	pi.addClause({negLit(var), litNegation(l)});
					else                            pi.addClause({litNegation(l), negLit(var)});
					push_sorted(up1, litNegation(l));
					eqs.push_back(litNegation(l));
					uvars.insert(litVariable(l));
				}
			}
		}
	}

	rLog.removeClause(removed);
	bool returnValue = removed;
	if (eqs.size()>1) {
		handleEqLits(eqs); // Similar technique proposed in Probing-Based Preprocessing Techniquesfor Propositional Satisfiability, Inˆes Lynce and Jo ̃ao Marques-Silva, 2003
		eqs.clear();
		returnValue = 1;
	}



	// neighbourhood PR clause technique
	if (redTechniques>>4 && !pi.isVarRemoved(var)) {
		set<int> neighbourhood;
		for (int l : up1) {
			if (pi.isVarRemoved(litVariable(l))) continue;
			for (int c : pi.litClauses[l]) {
				for (int lit : pi.clauses[c].lit) {
					if (pi.isLabelVar(litVariable(lit))) continue;
					neighbourhood.insert(litVariable(lit));
				}
			}
		}
		for (int l : up1) neighbourhood.erase(litVariable(l));
		neighbourhood.erase(var);

		for (int v : neighbourhood) {
			if (!rLog.requestTime(Log::Technique::FLE)) break;
			if (testBinaryRedundancy(negLit(var), negLit(v), up1, eqs, redTechniques>>4)) {
				satSolver->addClause(negLit(var), negLit(v));
				if (var<v) pi.addClause({negLit(var), negLit(v)});
				else		   pi.addClause({negLit(v), negLit(var)});

				push_sorted(up1, negLit(v));

				stats["FLE_RED"]+=1;
				returnValue = 1;
			}
			if (pi.isVarRemoved(v)) continue;
			if (testBinaryRedundancy(negLit(var), posLit(v), up1, eqs, redTechniques>>4)) {
				satSolver->addClause(negLit(var), posLit(v));
				if (var<v) pi.addClause({negLit(var), posLit(v)});
				else		   pi.addClause({posLit(v), negLit(var)});

				push_sorted(up1, posLit(v));
				stats["FLE_RED"]+=1;
				returnValue = 1;
			}
		}

		neighbourhood.clear();
		for (int l : up2) {
			if (pi.isVarRemoved(litVariable(l))) continue;
			for (int c : pi.litClauses[l]) {
				if (!pi.clauses[c].isHard()) continue;
				for (int lit : pi.clauses[c].lit) {
					if (pi.isLabelVar(litVariable(l))) continue;
					neighbourhood.insert(litVariable(lit));
				}
			}
		}
		for (int l : up2) neighbourhood.erase(litVariable(l));
		neighbourhood.erase(var);

		for (int v : neighbourhood) {
			if (!rLog.requestTime(Log::Technique::FLE)) break;
			if (testBinaryRedundancy(posLit(var), negLit(v), up2, eqs, redTechniques>>4)) {
				satSolver->addClause(posLit(var), negLit(v));
				if (var<v) pi.addClause({posLit(var), negLit(v)});
				else       pi.addClause({negLit(v), posLit(var)});

				push_sorted(up2, negLit(v));

				stats["FLE_RED"]+=1;
				returnValue = 1;
			}
			if (pi.isVarRemoved(v)) continue;
			if (testBinaryRedundancy(posLit(var), posLit(v), up2, eqs, redTechniques>>4)) {
				satSolver->addClause(posLit(var), posLit(v));
				if (var<v)	pi.addClause({posLit(var), posLit(v)});
				else 				pi.addClause({posLit(v), posLit(var)});

				push_sorted(up2, posLit(v));

				stats["FLE_RED"]+=1;
				returnValue = 1;
			}
		}
	}

	return returnValue;
}





int Preprocessor::doFLE(bool doRLE, bool findEqs, bool findRedEqs, bool findForced, bool findRedForced, int redTechniques) {
	rLog.startTechnique(Log::Technique::FLE);
	if (!rLog.requestTime(Log::Technique::FLE)) {
		rLog.stopTechnique(Log::Technique::FLE);
		return 0;
	}
	while (fleActiveTechniques != redTechniques && (stats["FLE_stop_position"]+1)*opt.FLE_redTechniquesActivate > __builtin_popcount(fleActiveTechniques)) {
		fleActiveTechniques |= (((redTechniques^fleActiveTechniques)-1) & redTechniques) ^ redTechniques;
	}
	redTechniques = fleActiveTechniques;

	prepareSatSolver();

	stats["FLE_FLE_success"]+=0;
	stats["FLE_success_accidental"]+=0;
	stats["FLE_RLE_success"]+=0;
	stats["FLE_Eqs"]+=0;
	stats["FLE_RedEqs"]+=0;
	stats["FLE_ForcedValues"]+=0;
	stats["FLE_RedForcedValues"]+=0;
	stats["FLE_binRED_UP"]+=0;
	stats["FLE_binRED_FPR"]+=0;
	stats["FLE_binRED_extFPR"]+=0;
	stats["FLE_RED"]+=0;



	int removed=0;
	stats["doFLE"]+=1;
	vector<int> tvars =  pi.tl.getTouchedVariables("FLE");
	sort(tvars.begin(), tvars.end());
	vector<int> svars;
	for (int vvar=0; vvar<pi.vars; ++vvar) {
		int var=vvar+flePos;
		if (var>=pi.vars) var-=pi.vars;
		if (pi.isVarRemoved(var)) continue;
		if (!binary_search(tvars.begin(), tvars.end(), var)) {
			svars.push_back(var);
			continue;
		}

		if (!rLog.requestTime(Log::Technique::FLE)) {
			flePos=var;
			stats["FLE_stop_position"]+=float(vvar-svars.size())/pi.vars;
			rLog.stopTechnique(Log::Technique::FLE);
			return removed;
		}
		removed += tryFLE(var, doRLE, findEqs, findRedEqs, findForced, findRedForced, redTechniques);
	}

	for (unsigned i=0; i<svars.size(); ++i) {
		int var=svars[i];
		if (pi.isVarRemoved(var)) continue;
		if (!rLog.requestTime(Log::Technique::FLE)) {
			flePos=var;
			stats["FLE_stop_position"]+=float(pi.vars-svars.size()-i)/pi.vars;
			rLog.stopTechnique(Log::Technique::FLE);
			return removed;
		}
		removed += tryFLE(var, doRLE, findEqs, findRedEqs, findForced, findRedForced, redTechniques);
	}

	stats["FLE_stop_position"]+=1;
	rLog.stopTechnique(Log::Technique::FLE);
	return removed;
}

// 15-745 Assignment 2: liveness.cpp
// Group:
////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>

#include "dataflow.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace llvm {

	

	// Simple struct to wrap Value* with customized print
	struct Var{
		Value* v;
		Var():v(nullptr) {}
		explicit Var(Value* v) : v(v) {}
		explicit Var(Instruction* v) : v(static_cast<Value*>(v)) {}
		std::string toString() const {
			return getShortValueName(v);
		}

		
		std::string getShortValueName(Value * v) const {
			if (v->getName().str().length() > 0) {
				return "%" + v->getName().str();
			}
			else if (isa<Instruction>(v)) {
				std::string s = "";
				raw_string_ostream * strm = new raw_string_ostream(s);
				v->print(*strm);
				std::string inst = strm->str();
				size_t idx1 = inst.find("%");
				size_t idx2 = inst.find(" ",idx1);
				if (idx1 != std::string::npos && idx2 != std::string::npos) {
					return inst.substr(idx1,idx2-idx1);
				}
				else {
					return "\"" + inst + "\"";
				}
			}
			else if (Argument *arg = dyn_cast<Argument>(v)) {
				std::string s = "";
				raw_string_ostream * strm = new raw_string_ostream(s);
				v->print(*strm);
				std::string inst = strm->str();
				size_t idx1 = inst.find("%");
				if (idx1 != std::string::npos) {
					return inst.substr(idx1);
				}
				else {
					return "\"" + inst + "\"";
				}

			}
			else {
				std::string s = "";
				raw_string_ostream * strm = new raw_string_ostream(s);
				v->print(*strm);
				std::string inst = strm->str();
				return "\"" + inst + "\"";
			}
		}

		

	};

	template<> struct DenseMapInfo<Var> {
	static inline Var getEmptyKey() {
		return Var((Instruction*)-1); 
	}
	static inline Var getTombstoneKey() {
		return Var((Instruction*)-2);
	}
	static unsigned getHashValue(const Var &var) {
		return (uintptr_t)var.v;
	}
	static bool isEqual(const Var &LHS, const Var &RHS) {
		return LHS.v == RHS.v;
	}
	};


	// Run the pass with: 
	// opt -enable-new-pm=0 -load ../Dataflow/liveness.so -liveness liveness-test-m2r.bc -o liveness.out
	class Liveness : public FunctionPass {
		public:
			static char ID;

			using LivenessAnalysis = DataflowAnalysis<Var, /** Forward = */ false>;


			Liveness() : FunctionPass(ID) { 
				outs()<<"Running liveness pass.";
				outs().flush();
			}

			virtual bool runOnFunction(Function& F) override {

				DenseMap<Value*, Value*> aliasMap;

				// A helper to find canonical representative
				std::function<Value*(Value*)> findRepresentative = [&](Value *v) -> Value* {
					while (aliasMap.count(v)) {
						v = aliasMap[v];
					}
					return v;
				};

				// Build alias sets by scanning phi instructions
				for (auto &B : F) {
					for (auto &I : B) {
						if (auto *phi = dyn_cast<PHINode>(&I)) {
							Value *lhs = &I; // phi result
							for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
								Value *rhs = phi->getIncomingValue(i);
								// Union: map rhs -> lhs
								aliasMap[rhs] = lhs;
							}
						}
					}
				}


				// All Elements to involve in the analysis.
				LivenessAnalysis::InstToElementFunc instToElementFunc = [&findRepresentative](Instruction* inst)->std::vector<Var>{
					// return and branch are not variables, so they should not be involved.
					if(isa<ReturnInst>(inst)||isa<BranchInst>(inst)){
						return {};
					}
					return {Var(findRepresentative(inst))};
				};

				auto offsetMap = LivenessAnalysis::createBitVectorOffsetMap(F, instToElementFunc);
				
				LivenessAnalysis::OffsetToElementMap offsetToElementMap;
				for(auto&[k,v]:offsetMap){
					offsetToElementMap.insert({v,k});
				}

				LivenessAnalysis::TransferFunction transferFunction = [&findRepresentative,&offsetMap,&offsetToElementMap](const BitVector& out, Instruction* inst){
					BitVector in = out;
					Instruction& instruction = *inst;
					
					Var var(findRepresentative(&instruction));
					// If the current instruction in a variable, it will have an entry in the offset map.
					// Place the variable in killset if it is defined.
					auto offsetMapIter = offsetMap.find(var);
					if(offsetMapIter != offsetMap.end()){
						int offset = offsetMapIter->second;
						in.reset(offset);
					}
					
					// Special processing for PHI node.
					if(isa<PHINode>(&instruction)){
						// Customized iteration over PHINode
						PHINode* phi = dyn_cast<PHINode>(&instruction);
						for(unsigned i = 0; i < phi->getNumIncomingValues(); i++){
							Value* val = phi->getIncomingValue(i);
							Var rhsVar(findRepresentative(val));
							auto usedIter = offsetMap.find(rhsVar);
							if(usedIter != offsetMap.end()){
								int usedOffset = usedIter->second;
								in.set(usedOffset);
							}
						}
						printBitVector(in, offsetToElementMap);
						return in;
					}
					
					// A conditional branch requires the branching variable to be live.
					BranchInst* br = dyn_cast<BranchInst>(&instruction);
					if(br){
						if (br->isConditional()) {
							Value* condition = br->getCondition();  // Direct method
							Var rhsVar(findRepresentative(condition));
							auto usedIter = offsetMap.find(rhsVar);
							if(usedIter != offsetMap.end()){
								int usedOffset = usedIter->second;
								in.set(usedOffset);
							}
						}
					}

					// Iterating over all operands; add uses to the set
					for(auto oi = instruction.op_begin();oi!=instruction.op_end();++oi){
						Value* val = *oi;
						if(!val){
							continue;
						}
						Var rhsVar(findRepresentative(val));

						auto usedIter = offsetMap.find(rhsVar);
						if(usedIter != offsetMap.end()){
							int usedOffset = usedIter->second;
							in.set(usedOffset);
						}
					}
					printBitVector(in, offsetToElementMap);
					return in;
				};

				LivenessAnalysis::MeetOperator setUnion = [](const BitVector& a, const BitVector& b) {
					BitVector res = a;
					res |= b;
					return res;
				};

				LivenessAnalysis analysis(setUnion,transferFunction,offsetMap.size(),false,false);
				std::pair<LivenessAnalysis::ResultMap,LivenessAnalysis::BlockResultMap> results = analysis.analyze(F, offsetToElementMap);
				auto& instructionResults = results.first;
				auto& blockResults = results.second;

				// Iterating over all instructions in the basic blocks, fetch the IN set for each instruction.
				outs()<<"-------Result Start----------\n";
				outs()<<"----Basic Block Boundary----\n";
				for(auto& bb : F){
					
					for(auto& inst : bb){
						auto in = instructionResults.find(&inst);
						if(in != instructionResults.end()){
							printBitVector(in->second, offsetToElementMap);
						}
						outs()<<inst<<"\n";
					}
					//printBitVector(blockResults.lookup(&bb),offsetToElementMap);
					outs()<<"----Basic Block Boundary----\n";
				}

				// Did not modify the incoming Function.
				return false;
			}

			virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
				AU.setPreservesAll();
			}

		private:
	};

	char Liveness::ID = 1;
	static RegisterPass<Liveness> X("liveness", "15745 Liveness");
}

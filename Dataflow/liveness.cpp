// 15-745 Assignment 2: liveness.cpp
// Group:
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/ADT/BitVector.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <memory>
#include <vector>

#include "dataflow.h"

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
			else if (ConstantInt * cint = dyn_cast<ConstantInt>(v)) {
				std::string s = "";
				raw_string_ostream * strm = new raw_string_ostream(s);
				cint->getValue().print(*strm,true);
				return strm->str();
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

				// All Elements to involve in the analysis.
				LivenessAnalysis::InstToElementFunc instToElementFunc = [](Instruction* inst)->std::vector<Var>{
					// return and branch are not variables, so they should not be involved.
					if(isa<ReturnInst>(inst)||isa<BranchInst>(inst)){
						return {};
					}
					return {Var(inst)};
				};

				auto offsetMap = LivenessAnalysis::createBitVectorOffsetMap(F, instToElementFunc);
				
				LivenessAnalysis::OffsetToElementMap offsetToElementMap;
				for(auto&[k,v]:offsetMap){
					offsetToElementMap.insert({v,k});
				}

				// All variables:
				outs()<<"All variables:";
				printBitVector(BitVector(offsetMap.size(),true), offsetToElementMap);

				LivenessAnalysis::TransferFunction transferFunction = [&offsetMap,&offsetToElementMap](const BitVector& out, Instruction* inst){
					BitVector in = out;
					Instruction& instruction = *inst;

					Var var(&instruction);
					// Some instructions are not variables, skip them but keep current state
					auto offsetMapIter = offsetMap.find(var);
					if(offsetMapIter == offsetMap.end()){
						return in;
					}
					
					int offset = offsetMapIter->second;
					in.reset(offset);

					if(isa<PHINode>(&instruction)){
						// Customized iteration over PHINode
						PHINode* phi = dyn_cast<PHINode>(&instruction);
						for(unsigned i = 0; i < phi->getNumIncomingValues(); i++){
							Value* val = phi->getIncomingValue(i);
							Var rhsVar(val);
							auto usedIter = offsetMap.find(rhsVar);
							if(usedIter != offsetMap.end()){
								int usedOffset = usedIter->second;
								in.set(usedOffset);
							}
						}
						return in;
					}

					if(BranchInst* br = dyn_cast<BranchInst>(&instruction)){
						if (br->isConditional()) {
							Value* condition = br->getCondition();  // Direct method
							Var rhsVar(condition);
							auto usedIter = offsetMap.find(rhsVar);
							if(usedIter != offsetMap.end()){
								outs()<<"Setting location of "<<rhsVar.toString()<<" at instruction "<<Var(&instruction).toString()<<"\n";
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
						Var rhsVar(val);

						auto usedIter = offsetMap.find(rhsVar);
						if(usedIter != offsetMap.end()){
							outs()<<"Setting location of "<<rhsVar.toString()<<" at instruction "<<Var(&instruction).toString()<<"\n";
							int usedOffset = usedIter->second;
							in.set(usedOffset);
						}
					}
					return in;
				};

				LivenessAnalysis::MeetOperator setUnion = [](const BitVector& a, const BitVector& b) {
					BitVector res = a;
					res |= b;
					return res;
				};

				LivenessAnalysis analysis(setUnion,transferFunction,offsetMap.size(),false,false);
				LivenessAnalysis::ResultMap result = analysis.analyze(F, offsetToElementMap);
				
				// Iterating over all instructions in the basic blocks, fetch the IN set for each instruction.
				outs()<<"-------Result Start----------\n";
				for(auto& bb : F){
					for(auto& inst : bb){
						outs()<<inst<<"\n";
						auto in = result.find(&inst);
						if(in != result.end()){
							printBitVector(in->second, offsetToElementMap);
						}
					}
					outs()<<"----Basic Block Boundry----\n";
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

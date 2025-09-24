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
				std::string instructionString;
				llvm::raw_string_ostream rso(instructionString);
				v->print(rso); // Print the instruction to the raw_string_ostream
				rso.flush();  
				return instructionString;
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

				LivenessAnalysis::InstToElementFunc instToElementFunc = [](Instruction* inst)->std::vector<Var>{
					if(isa<ReturnInst>(inst)){
						return {};
					}
					return {Var(inst)};
				};

				auto offsetMap = LivenessAnalysis::createBitVectorOffsetMap(F, instToElementFunc);
				
				LivenessAnalysis::OffsetToElementMap offsetToElementMap;
				for(auto&[k,v]:offsetMap){
					offsetToElementMap.insert({v,k});
				}

				outs()<<"Number of variables: "<<offsetMap.size();

				LivenessAnalysis::TransferFunction transferFunction = [&offsetMap,&offsetToElementMap](const BitVector& out, BasicBlock* bb){
					BitVector in = out;
					for(auto& instruction: *bb){

						if(isa<PHINode>(instruction)){
							continue;
						}

						Var var(&instruction);
						

						// The variable on LHS is defined, placed in killset.
						// With LLVM, LHS is just the instruction itself.

						// Some instructions are not variables, skip them.
						auto offsetMapIter = offsetMap.find(var);
						if(offsetMapIter == offsetMap.end()){
							continue;
						}
						
						int offset = offsetMapIter->second;
						in.reset(offset);

						// Iterating over all oprands. Due to SSA, a variable will never appear on both LHS and RHS.
						// So we don't need to consider operations like A=A+B.
						for(auto oi = instruction.op_begin();oi!=instruction.op_end();++oi){
							// For each operand, it is put in Gen Set.
							
							Value* val = *oi;
							if(!val){
								continue;
							}
							if (isa<Instruction>(val)) {
								int offset = offsetMap.find(var)->second;
								in.set(offset);
							}

							
						}

						printBitVector(in, offsetToElementMap);
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
				outs()<<"Performed analysis for "<<result.size()<<" elements.";

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

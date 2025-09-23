// 15-745 Assignment 2: liveness.cpp
// Group:
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm-14/llvm/ADT/BitVector.h>
#include <llvm-14/llvm/IR/BasicBlock.h>
#include <llvm-14/llvm/IR/Instruction.h>
#include <memory>
#include <vector>

#include "dataflow.h"

using namespace llvm;

namespace {

	// Run the pass with: 
	// opt -enable-new-pm=0 -load ../Dataflow/liveness.so -liveness liveness-test-m2r.bc -o liveness.out
	class Liveness : public FunctionPass {
		public:
			static char ID;

			using LivenessAnalysis = DataflowAnalysis<Instruction*, /** Forward = */ false>;


			Liveness() : FunctionPass(ID) { }

			virtual bool runOnFunction(Function& F) {

				LivenessAnalysis::InstToElementFunc instToElementFunc = [](Instruction* inst)->std::vector<Instruction*>{
					return {inst};
				};

				auto offsetMap = LivenessAnalysis::createBitVectorOffsetMap(F, instToElementFunc);
				
				LivenessAnalysis::OffsetToElementMap offsetToElementMap;
				for(auto&[k,v]:offsetMap){
					offsetToElementMap.insert({v,k});
				}

				LivenessAnalysis::TransferFunction transferFunction = [&offsetMap,&offsetToElementMap](const BitVector& out, const BasicBlock* bb){
					BitVector in = out;
					for(auto& instruction: *bb){

						// The variable on LHS is defined, placed in killset.
						// With LLVM, LHS is just the instruction itself.

						// find should never return end.
						int offset = offsetMap.find(&instruction)->second;
						in.reset(offset);

						// Iterating over all oprands. Due to SSA, a variable will never appear on both LHS and RHS.
						// So we don't need to consider operations like A=A+B.
						for(auto oi = instruction.op_begin();oi!=instruction.op_end();++oi){
							// For each operand, it is put in Gen Set.
							Value& val = *oi->get();
							int offset = offsetMap.find(&instruction)->second;
							in.set(offset);
						}

						printBitVector(in, offsetToElementMap);
					}


					return in;
				};

				LivenessAnalysis analysis();


				// Did not modify the incoming Function.
				return false;
			}

			virtual void getAnalysisUsage(AnalysisUsage& AU) const {
				AU.setPreservesAll();
			}

		private:
	};

	char Liveness::ID = 0;
	RegisterPass<Liveness> X("liveness", "15745 Liveness");
}

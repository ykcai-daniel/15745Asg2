// 15-745 Assignment 2: available.cpp
// Group: Haojia Sun (haojias), Yikang Cai (dcai)
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

#include "dataflow.h"
#include "available-support.h"

using namespace llvm;
using namespace std;

namespace {
	class AvailableExpressions : public FunctionPass {

		public:
			static char ID;

			using ExpressionAnalysis = DataflowAnalysis<Expression>;


			AvailableExpressions() : FunctionPass(ID) { }

			virtual bool runOnFunction(Function& F) {
				// Map each Value* to its representative in the alias set
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

				std::vector<Expression> expressions;
				// Here's some code to familarize you with the Expression
				// class and pretty printing code we've provided:
				for (auto &B : F)
					for (auto &I : B)
						// We only care about available expressions for BinaryOperators
						if (BinaryOperator *BI = dyn_cast<BinaryOperator>(&I))
							// Create a new Expression to capture the RHS of the BinaryOperator
							expressions.push_back(Expression(BI));

				// Print out the expressions used in the function
				outs() << "Expressions used by this function:\n";
				printSet(&expressions);

				// get the expressions -> index mapping by calling createBitVectorOffsetMap
				DenseMap<Expression, int> elementToOffset =
					DataflowAnalysis<Expression>::createBitVectorOffsetMap(F,
						[&](const Instruction* I) -> std::vector<Expression> {
							std::vector<Expression> elems;
							if (auto *BI = dyn_cast<BinaryOperator>(I)) {
								Expression e((Instruction*)I);

								e.v1 = findRepresentative(e.v1);
								e.v2 = findRepresentative(e.v2);

								elems.push_back(e);
							}
							return elems;
						});
				int bitVectorSize = elementToOffset.size();
				DataflowAnalysis<Expression>::OffsetToElementMap offsetToElement; // for printing
				for (auto& [e, idx] : elementToOffset) {
					offsetToElement[idx] = e;
				}

				// define meet operator for available expression analysis: intersection
				DataflowAnalysis<Expression>::MeetOperator meetOperator = [](const BitVector& a, const BitVector& b) {
					BitVector res = a;
					res &= b;
					return res;
				};

				// define GEN function for each instruction in a basic block
				auto computeGen = [&](Instruction &I) {
					BitVector gen(bitVectorSize, false);
					if (auto *BI = dyn_cast<BinaryOperator>(&I)) {
						Expression e(&I);
						e.v1 = findRepresentative(e.v1);
        				e.v2 = findRepresentative(e.v2);
						
						int idx = elementToOffset.lookup(e); // find all expressions in the universal set E

						bool killedLater = false;
						Value* lhs_var = findRepresentative(&I);
						// Check if the current LHS variable kills expr, e.g., B = B + C
						if (findRepresentative(e.v1) == lhs_var || findRepresentative(e.v2) == lhs_var) {
							killedLater = true;
						}
						
						// Check if the following LHS variables kill expr, e.g., A = B + C; B = E + D
						for (auto it = std::next(I.getIterator()); it != I.getParent()->end(); ++it) {
							Value *lhs2 = findRepresentative(&*it);
							if (findRepresentative(e.v1) == lhs2 || findRepresentative(e.v2) == lhs2) {
								killedLater = true;
								break;
							}
						}

						if (!killedLater) {
							gen.set(idx);
						}
					}
					return gen;
				};

				// define KILL function for each instruction in a basic block
				auto computeKill = [&](Instruction &I) {
					BitVector kill(bitVectorSize, false);
					Value *lhs_var = findRepresentative(&I);
					// Find all expressions in the universal set E that depend on lhs and kill them
					for (auto &expr_idx : elementToOffset) {
						const Expression &expr = expr_idx.first;
						int idx = expr_idx.second;
						if (findRepresentative(expr.v1) == lhs_var || findRepresentative(expr.v2) == lhs_var) {
							kill.set(idx);
						}
					}
					return kill;
				};

				// define Transfer function for each basic block
				ExpressionAnalysis::BlockTransferFunction transferFunc = 
				[&](BitVector in, BasicBlock* BB, ExpressionAnalysis::ResultMap& resultMap) {
					BitVector out = in; // Initialize OUT[BB] = IN[BB]
					for (Instruction &I : *BB) {
						BitVector gen = computeGen(I);
						BitVector kill = computeKill(I);

						// OUT = (IN - KILL) ∪ GEN
						out = (out - kill) + gen;
						resultMap[&I] = out;
					}
					return out;
				};

				// create dataflow analysis object
				ExpressionAnalysis analysis(
					meetOperator,
					transferFunc,
					bitVectorSize,
					false, // entryInitValue_ = empty set
					true   // outInitValue_ = universal set
				);

				auto result = analysis.analyze(F, offsetToElement);
				ExpressionAnalysis::ResultMap instructionResults = result.first;

				// After convergence, print the IN and OUT sets of each instruction
				// Iterating over all instructions in the basic blocks, fetch the IN set for each instruction.
				outs()<<"-------Result Start----------\n";
				outs()<<"----Basic Block Boundary----\n";
				for(auto& bb : F){
					for(auto& inst : bb){
						auto out = instructionResults.find(&inst);
						outs()<<inst<<"\n";
						if(out != instructionResults.end()){
							printBitVector(out->second, offsetToElement);
						}
					}
					outs()<<"----Basic Block Boundary----\n";
				}

				// Did not modify the incoming Function.
				return false;
			}

			virtual void getAnalysisUsage(AnalysisUsage& AU) const {
				AU.setPreservesAll();
			}

		private:
	};

	char AvailableExpressions::ID = 0;
	RegisterPass<AvailableExpressions> X("available",
			"15745 Available Expressions");
}

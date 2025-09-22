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

			AvailableExpressions() : FunctionPass(ID) { }

			virtual bool runOnFunction(Function& F) {
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
						[](const Instruction* I) -> std::vector<Expression> {
							std::vector<Expression> elems;
							if (auto *BI = dyn_cast<BinaryOperator>(I)) {
								elems.push_back(Expression((Instruction*)I));
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
						int idx = elementToOffset.lookup(e); // find all expressions in the universal set E

						bool killedLater = false;
						// Check if the current LHS variable kills expr, e.g., B = B + C
						Value* lhs_var = &I;
						if (e.v1 == lhs_var || e.v2 == lhs_var) {
							killedLater = true;
						}
						// Check if the following LHS variables kill expr, e.g., A = B + C; B = E + D
						for (auto it = std::next(I.getIterator()); it != I.getParent()->end(); ++it) {
							Value *lhs_var = &*it;
							if (e.v1 == lhs_var || e.v2 == lhs_var) {
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
					Value *lhs_var = &I;
					// Find all expressions in the universal set E that depend on lhs and kill them
					for (auto &expr_idx : elementToOffset) {
						const Expression &expr = expr_idx.first;
						int idx = expr_idx.second;
						if (expr.v1 == lhs_var || expr.v2 == lhs_var) {
							kill.set(idx);
						}
					}
					return kill;
				};

				// define Transfer function for each basic block
				auto transferFunc = [&](BitVector in, BasicBlock* BB) {
					BitVector out = in; // Initialize OUT[BB] = IN[BB]
					for (Instruction &I : *BB) {
						BitVector gen = computeGen(I);
						BitVector kill = computeKill(I);

						// OUT = (IN - KILL) ∪ GEN
						out = (out - kill) + gen;
					}
					return out;
				};

				// create dataflow analysis object
				DataflowAnalysis<Expression> analysis(
					meetOperator,
					transferFunc,
					bitVectorSize,
					false, // entryInitValue_ = empty set
					true   // outInitValue_ = universal set
				);

				auto result = analysis.analyze(F, offsetToElement);

				// After convergence, print the IN and OUT sets of each instruction
				for (auto &B : F) {
					BitVector in(bitVectorSize, true); // Top
					if (pred_empty(&B)) {
						in = BitVector(bitVectorSize, false); // entry = empty set
					} else {
						for (auto *Pred : predecessors(&B)) {
							in &= result.lookup(Pred); // meet of all preds' OUT
						}
					}

					printBitVector(in, offsetToElement);

					BitVector out = in;
					for (Instruction &I : B) {
						BitVector gen = computeGen(I);
						BitVector kill = computeKill(I);
						out = (out - kill) + gen;
						printBitVector(out, offsetToElement);
					}
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

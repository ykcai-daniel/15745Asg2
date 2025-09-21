// 15-745 Assignment 2: available.cpp
// Group:
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

			using ExprAnalysis = DataflowAnalysis<Expression>;

			AvailableExpressions() : FunctionPass(ID) { }

			virtual bool runOnFunction(Function& F) {

				// Step 1: Create BitVectorOffsetMap by iterating over all instructions in F and applying getElementsFromInstruction to each instruction.
				auto getElementsFromInstruction = [](const Instruction* inst) -> Expression {
					// TODO
					if (const BinaryOperator* binOp = dyn_cast<BinaryOperator>(inst)) {
						return Expression(binOp);
					}
					return Expression(nullptr);
				};
				ExprAnalysis::BitVectorOffsetMap elementToOffset = ExprAnalysis::createBitVectorOffsetMap(F, getElementsFromInstruction);

				// Step 2: Define meet operator, gen function and kill function.
				auto meetOperator = [](const BitVector& a, const BitVector& b) -> BitVector {
					// TODO
					// Suppose we are using union as the meet operator.
					return a + b;
				};

				// Capture the elementToOffset map by value to use inside the lambda.
				auto genFunc = [&elementToOffset]( BasicBlock* bb) -> BitVector {
					// TODO
				};

				auto killFunc = [&elementToOffset](BasicBlock* bb) -> BitVector {
					// TODO

				};

				// Init and Run the analysis


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

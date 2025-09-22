// 15-745 Assignment 2: dataflow.h
// Group: Haojia Sun (haojias), Yikang Cai (dcai)
////////////////////////////////////////////////////////////////////////////////

#ifndef __CLASSICAL_DATAFLOW_H__
#define __CLASSICAL_DATAFLOW_H__

#include <functional>
#include <llvm/ADT/BitVector.h>
#include <llvm/IR/BasicBlock.h>
#include <stdio.h>
#include <iostream>
#include <queue>
#include <vector>

#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/PostOrderIterator.h"

namespace llvm {

	// Difference operator for BitVector
	BitVector operator-(const BitVector& a, const BitVector& b) {
		BitVector result = a;
		for (int i = 0; i < b.size(); ++i) {
			if (b.test(i)) {
				result.reset(i);
			}
		}
		return result;
	}

	// Union operator for BitVector
	BitVector operator+(const BitVector& a, const BitVector& b) {
		BitVector result = a;
		for (int i = 0; i < b.size(); ++i) {
			if (b.test(i)) {
				result.set(i);
			}
		}
		return result;
	}


	// class Element is the element being analyzed. For instance, in reaching definition, Element is a definition.
	// In available expressions, element is an expression. To use this class, you need to:
	// (1) Define your own Element class for this analysis, and provide std::hash and equals operator for it.
	// 		Element is constructed from Instruction* or Value*, so you can store Instruction* or Value* inside Element.
	// (2) Provide lambda MeetOperator, Gen and Kill Function.
	template <class Element, bool Forward = true>
	class DataflowAnalysis{

		public:
			// Result of Dataflow Analysis.
			// ResultMap maps each BasicBlock to OUT[BB].
			using ResultMap = DenseMap<BasicBlock*, BitVector>;
			// Meet operator.
			using MeetOperator = std::function<BitVector(const BitVector&, const BitVector&)>;

			// Map from Element to its offset in BitVector, should be captured by GenFunc and KillFunc.
			using BitVectorOffsetMap = DenseMap<Element, int>;
			using OffsetToElementMap = DenseMap<int, Element>;
			// Transfer function: OUT[BB] = transferFunc(IN[BB], BB), need to print result after each instruction.
			using TransferFunction = std::function<BitVector(BitVector,BasicBlock*)>;

		


		// @param meetOperator: function to compute meet of two BitVectors.
		// @param genFunc: function to compute gen set of a basic block. Should return a BitVector.
		// @param killFunc: function to compute kill set of a basic block. Should return a BitVector.
		// @param numElements: number of elements in the analysis. Used to initialize BitVectors.
		// @param entryInit: initial IN[BB] value for entry block (the block without predecessors).
		// @param outInit: initial value for OUT[BB] for all other blocks.
		DataflowAnalysis(
			const MeetOperator& meetOperator,
			const TransferFunction& transferFunction,
			int numElements,
			bool entryInit,
			// TODO(optional): make outInit type trait of MeetOperator
			bool outInit
		):  
			meetOperator_(meetOperator),
			transferFunc_(transferFunction),
			bitVectorSize_(numElements),
			entryInitValue_(entryInit),
			outInitValue_(outInit){}

		
		// Create BitVectorOffsetMap by iterating over all instructions in func and applying getElementsFromInstruction to each instruction.
		// The returned BitVectorOffsetMap maps each Element to a unique offset in the BitVector.
		// BitVectorOffsetMap should be captured by genFunc and killFunc to create BitVectors for each basic block.
		static BitVectorOffsetMap createBitVectorOffsetMap(const Function& func, const std::function<std::vector<Element>(Instruction*)>& getElementsFromInstruction) {
			// Initialize bitVectorSize_ and elementToOffset_ by iterating over all instructions in func.
			int bitVectorSize = 0;
			BitVectorOffsetMap elementToOffset;
			for (const BasicBlock& bb : func) {
				for (const Instruction& inst : bb) {
					std::vector<Element> elements = getElementsFromInstruction(&inst);
					for (const Element& elem : elements) {
						if (elementToOffset.find(elem) == elementToOffset.end()) {
							elementToOffset[elem] = bitVectorSize;
							bitVectorSize++;
						}
					}
				}
			}
			return std::move(elementToOffset);
		}

		// Perform forward dataflow analysis on the given function.
		ResultMap analyze(Function& func, const OffsetToElementMap& map){
			std::vector<BasicBlock*> PostOrder(po_begin(&func), po_end(&func));
			// Now iterate in reverse (which gives you RPO)
			if(!Forward){
				std::reverse(PostOrder.begin(), PostOrder.end());
			}

			ResultMap resultMap;
			bool changed;
        do {
            changed = false;
            for (auto *BB : PostOrder) {

				auto basicBlockResultIter = resultMap.find(BB);
				BitVector& oldOut = basicBlockResultIter->second;
				pred_range parentBBs = predecessors(BB);
				if(!Forward){
					parentBBs = successors(BB);
				}

				// Initialize to TOP: TOP meet X = X
				BitVector newIn(bitVectorSize_,outInitValue_);
				// If it is the entry block.
				if(parentBBs.empty()){
					newIn = BitVector(bitVectorSize_,entryInitValue_);

				} else{
					for (auto *pred : predecessors(BB)) {
						// Initialize to Top.
						auto& [iter, predNotInitiliazed] = resultMap.try_emplace(pred,BitVector(bitVectorSize_,outInitValue_));
						BitVector& predOut = iter;
                    	newIn = meetOperator_(newIn, predOut);
                	}
				};

				printBitVector(newIn, map);
				BitVector newOut = transferFunc_(newIn,BB);
				printBitVector(newOut, map);
				
				if(newOut!=oldOut){
					changed = true;
					resultMap[BB] = std::move(newOut);
				}
            }
        } while (changed);
			return resultMap;
		}

		private:
			void printBitVector(const BitVector& vec,  const OffsetToElementMap& map) const {
				// TODO
			}

			MeetOperator meetOperator_;
			TransferFunction transferFunc_;
			int bitVectorSize_;
			bool entryInitValue_;
			bool outInitValue_;
	};

}

#endif
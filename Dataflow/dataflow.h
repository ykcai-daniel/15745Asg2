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
	llvm::BitVector operator-(const llvm::BitVector& a, const llvm::BitVector& b);
	// Union operator for BitVector
	llvm::BitVector operator+(const llvm::BitVector& a, const llvm::BitVector& b);

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
			using OffsetToElementMap = DenseMap<int, Element>; // for printing
			// Transfer function: OUT[BB] = transferFunc(IN[BB], BB), need to print result after each instruction.
			using TransferFunction = std::function<BitVector(BitVector, BasicBlock*)>;

			using InstToElementFunc = std::function<std::vector<Element>(Instruction*)>;

		


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
		static BitVectorOffsetMap createBitVectorOffsetMap(Function& func, const InstToElementFunc& getElementsFromInstruction) {
			// Initialize bitVectorSize_ and elementToOffset_ by iterating over all instructions in func.
			int bitVectorSize = 0;
			BitVectorOffsetMap elementToOffset;
			for (BasicBlock& bb : func) {
				for (Instruction& inst : bb) {
					std::vector<Element> elements = getElementsFromInstruction(&inst);
					for (Element& elem : elements) {
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

				BitVector& oldOut = resultMap.try_emplace(
					BB, BitVector(bitVectorSize_, outInitValue_)
				).first->second;
				// Initialize to TOP: TOP meet X = X
				BitVector newIn;
				if (Forward) {
					if (pred_empty(BB)) {
						// entry block init
						newIn = BitVector(bitVectorSize_, entryInitValue_);
					} else {
						newIn = BitVector(bitVectorSize_, outInitValue_);
						for (auto *Pred : predecessors(BB)) {
							auto [it, inserted] = resultMap.try_emplace(Pred, BitVector(bitVectorSize_, outInitValue_));
							BitVector &predOut = it->second;
							newIn = meetOperator_(newIn, predOut);
						}
					}
				} else {
					if (succ_empty(BB)) {
						// exit block init
						newIn = BitVector(bitVectorSize_, entryInitValue_);
					} else {
						newIn = BitVector(bitVectorSize_, outInitValue_);
						for (auto *Succ : successors(BB)) {
							auto [it, inserted] = resultMap.try_emplace(Succ, BitVector(bitVectorSize_, outInitValue_));
							BitVector &succOut = it->second;
							newIn = meetOperator_(newIn, succOut);
						}
					}
				}
				BitVector newOut = transferFunc_(newIn,BB);
				
				if(newOut!=oldOut){
					changed = true;
					resultMap[BB] = std::move(newOut);
				}
            }
        } while (changed);
			return resultMap;
		}
		
		private:
			MeetOperator meetOperator_;
			TransferFunction transferFunc_;
			int bitVectorSize_;
			bool entryInitValue_;
			bool outInitValue_;
	};

	template <class Element>
	void printBitVector(const BitVector &vec,
						const DenseMap<int, Element> &map) {
		outs() << "{";
		bool first = true;
		for (int i = 0; i < vec.size(); i++) {
			if (vec.test(i)) {
				if (!first) outs() << ", ";
				 if constexpr (std::is_pointer<Element>()) {
					outs() << *map.lookup(i);
				} else {
					outs() << map.lookup(i).toString();
				}

				first = false;
			}
		}
		outs() << "}\n";
	}
}

#endif
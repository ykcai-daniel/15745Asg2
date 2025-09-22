// 15-745 Assignment 2: available-support.h
// Group: Haojia Sun (haojias), Yikang Cai (dcai)
////////////////////////////////////////////////////////////////////////////////

#ifndef __AVAILABLE_SUPPORT_H__
#define __AVAILABLE_SUPPORT_H__

#include <string>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
	std::string getShortValueName(Value * v);

	class Expression {
		public:
			Value * v1;
			Value * v2;
			Instruction::BinaryOps op;
			Expression() : v1(nullptr), v2(nullptr), op(Instruction::BinaryOps::Add) {}
			Expression (Instruction * I);
			bool operator== (const Expression &e2) const;
			bool operator< (const Expression &e2) const;
			std::string toString() const;
	};

	template<> struct DenseMapInfo<Expression> {
		static inline Expression getEmptyKey() {
			return Expression((Instruction*)-1); 
		}
		static inline Expression getTombstoneKey() {
			return Expression((Instruction*)-2);
		}
		static unsigned getHashValue(const Expression &E) {
			return (uintptr_t)E.v1 ^ ((uintptr_t)E.v2 << 4) ^ E.op;
		}
		static bool isEqual(const Expression &LHS, const Expression &RHS) {
			return LHS == RHS;
		}
	};

	void printSet(std::vector<Expression> * x);
}

#endif

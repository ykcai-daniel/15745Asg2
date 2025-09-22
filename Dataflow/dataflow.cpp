// 15-745 Assignment 2: dataflow.cpp
// Group: Haojia Sun (haojias), Yikang Cai (dcai)
////////////////////////////////////////////////////////////////////////////////

#include "dataflow.h"

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
}

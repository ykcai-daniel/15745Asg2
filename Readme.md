# Dataflow Analysis Framework
## Running the passes
To compile the pass,
```
cd Dataflow
make
```
Run the passes from `tests` dir (`cd tests`) if needed.
- Run the Available 
```
opt -enable-new-pm=0 -load ../Dataflow/available.so -available available-test-m2r.bc -o available.out
```
- Run the Liveness Pass with
```
opt -enable-new-pm=0 -load ../Dataflow/liveness.so -liveness liveness-test-m2r.bc -o liveness.out
```

## Framework  
We implemented a generic **iterative dataflow analysis framework** in LLVM as a templated class `DataflowAnalysis<Element, bool Forward>`. It abstracts the fixed-point iteration while letting clients define the analysis-specific **Element type**, **meet operator**, and **transfer function**. Each unique element is mapped to a compact bitvector offset via `createBitVectorOffsetMap`, and PHI-node aliasing is handled by unifying SSA names through an alias map and a helper `findRepresentative`.  

## Available Expressions  
This pass is a **forward analysis** with meet operator **intersection**. GEN sets contain expressions computed by `BinaryOperator` instructions (after canonicalization), while KILL sets remove expressions that depend on the instruction’s defined variable. The transfer function applies `OUT = (IN - KILL) ∪ GEN` at the instruction level, allowing availability to be updated and printed after each instruction. Entry is initialized to the empty set, and all other OUT sets start as the universal set.  

## Liveness  
This pass is a **backward analysis** with meet operator **union**. GEN collects variables used by an instruction, and KILL removes variables defined by it. The transfer function is `IN = (OUT - KILL) ∪ GEN`, applied in reverse order until convergence. PHI nodes are handled specially by linking incoming values with predecessors, and branch conditions are marked live. SSA form simplifies the analysis since redefinitions like `a = a+1` require no extra handling.  

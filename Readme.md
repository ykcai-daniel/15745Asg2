# Dataflow Framework
## Running the passes
- Run the Available 
```
opt -enable-new-pm=0 -load ../Dataflow/available.so -available available-test-m2r.bc -o available.out
```
- Run the Liveness Pass with
```
opt -enable-new-pm=0 -load ../Dataflow/liveness.so -liveness liveness-test-m2r.bc -o liveness.out
```

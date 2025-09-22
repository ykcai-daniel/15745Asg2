all: liveness.so available.so

CXXFLAGS = -rdynamic -I/usr/lib/llvm-15/include -std=c++17 -g -O0 -fPIC $(shell llvm-config --cxxflags) 

dataflow.o: dataflow.cpp dataflow.h

liveness.o: liveness.cpp

available-support.o: available-support.cpp available-support.h	

%.so: %.o dataflow.o available-support.o
	$(CXX) -shared $^ -o $@

clean:
	rm -f *.o *~ *.so

.PHONY: clean all

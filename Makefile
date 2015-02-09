CXX = mpicxx
CXXFLAGS = --std=c++11 -fno-PIE
LDFLAGS = -fno-PIE

# maximum number of function parameters to support
NUM_PARAMS_GEN=10

# auto-generated header files
GEN = gen.async_task_data.hpp gen.async_task_run.hpp gen.async_task_invoke.hpp

all : example.x

example.x : example.o async.o
	$(CXX) $(LDFLAGS) example.o async.o -o $@

example.o : async.hpp $(GEN)

async.o : async.hpp $(GEN)

$(GEN) : auto_gen.sh
	@echo "generating header files ..."
	@./auto_gen.sh $(NUM_PARAMS_GEN)

.PHONY : clean
clean:
	rm -f async.o example.o

.PHONY: distclean
distclean: clean
	rm -f example.x $(GEN)

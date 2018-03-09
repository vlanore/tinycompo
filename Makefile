TEST_FILES = test/core.cpp test/arrays.cpp
MPI_TEST_FILES = test/mpi_context.cpp
EXAMPLE_FILES = $(shell ls -d -1 $$PWD/example/*.*pp)
FLAGS = --std=gnu++11 -Wall -Wextra -Wfatal-errors

all: test_bin example/text_process_bin example/perf_test_bin mpi #example/poisson_gamma_bin
mpi: example/mpi_example_mpibin test/mpi_context_mpibin

#======================================================================================================================
test_bin: test.cpp tinycompo.hpp $(TEST_FILES)
	$(CXX) $< -o $@ $(FLAGS) $(TINYCOMPO_FLAGS)

example/poisson_gamma_bin: example/poisson_gamma.cpp example/poisson_gamma_connectors.hpp example/graphical_model.hpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_bin: %.cpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_mpibin: %.cpp tinycompo.hpp tinycompo_mpi.hpp
	mpic++ $< -o $@ -I. $(FLAGS)


#======================================================================================================================
.PHONY: test clean format ready

test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./test_bin

test_mpi: test/mpi_context_mpibin
	mpirun -np 4 ./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw example/*_bin example/*_mpibin

format:
	clang-format -i test.cpp tinycompo.hpp tinycompo_mpi.hpp $(TEST_FILES) $(EXAMPLE_FILES) $(MPI_TEST_FILES)

ready: all test test_mpi format
	git status

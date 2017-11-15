TEST_FILES = $(shell ls -d -1 $$PWD/test/*.*pp)
EXAMPLE_FILES = $(shell ls -d -1 $$PWD/example/*.*pp)
FLAGS = --std=gnu++11 -Wall -Wextra -Wfatal-errors

all: test_bin example/text_process_bin example/poisson_gamma_bin example/perf_test_bin

mpi: example/mpi_test_mpibin example/mpi_overhaul_mpibin


#======================================================================================================================
test_bin: test.cpp tinycompo.hpp $(TEST_FILES)
	$(CXX) $< -o $@ $(FLAGS) $(TINYCOMPO_FLAGS)

example/poisson_gamma_bin: example/poisson_gamma.cpp example/poisson_gamma_connectors.hpp example/graphical_model.hpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_bin: %.cpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_mpibin: %.cpp tinycompo.hpp
	mpic++ $< -o $@ -I. $(FLAGS)


#======================================================================================================================
.PHONY: test clean format ready

test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw example/*_bin example/*_mpibin

format:
	clang-format -i test.cpp tinycompo.hpp $(TEST_FILES) $(EXAMPLE_FILES)

ready: all test format
	git status

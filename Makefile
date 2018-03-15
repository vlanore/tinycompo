TEST_FILES = test/core.cpp test/arrays.cpp test/introspection.cpp
MPI_TEST_FILES = test/mpi_context.cpp
EXAMPLE_FILES = $(shell ls -d -1 $$PWD/example/*.*pp)
FLAGS = --std=gnu++11 -Wall -Wextra -Wfatal-errors -g

.PHONY: all
all: test_bin example/text_process_bin example/perf_test_bin mpi #example/poisson_gamma_bin

.PHONY: mpi
mpi: example/mpi_example_mpibin test/mpi_context_mpibin

#======================================================================================================================
test_bin: test.cpp tinycompo.hpp $(TEST_FILES)
	$(CXX) $< -o $@ $(FLAGS) $(TINYCOMPO_FLAGS)

example/poisson_gamma_bin: example/poisson_gamma.cpp example/poisson_gamma_connectors.hpp example/graphical_model.hpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_bin: %.cpp tinycompo.hpp
	$(CXX) $< -o $@ -I. $(FLAGS)

%_mpibin: %.cpp tinycompo.hpp tinycompo_mpi.hpp
	mpic++ $< -o $@ -I. $(FLAGS) $(TINYCOMPO_FLAGS)

#======================================================================================================================
.PHONY: test
test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./test_bin

.PHONY: test_mpi
test_mpi: test/mpi_context_mpibin
	mpirun -np 4 ./$<

.PHONY: clean
clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw example/*_bin example/*_mpibin

.PHONY: format
format:
	clang-format -i test.cpp tinycompo.hpp tinycompo_mpi.hpp $(TEST_FILES) $(EXAMPLE_FILES) $(MPI_TEST_FILES)

.PHONY: ready
ready:
	@echo "\033[1m\033[95mFormatting with clang-format...\033[0m"
	@make format --no-print-directory
	@echo "\033[1m\033[95m\nCompiling if necessary...\033[0m"
	@make -j6 --no-print-directory all mpi
	@echo "\033[1m\033[95m\nLaunching test...\033[0m"
	@make test --no-print-directory
	@make test_mpi --no-print-directory
	@echo "\033[1m\033[95m\nAll done, git status is:\033[0m"
	@git status

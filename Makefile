.PHONY: test clean format ready

all: test_bin

test_bin: test.cpp model.hpp
	$(CXX) $< -o $@ --std=gnu++11 -fprofile-arcs -ftest-coverage -g -Wall -Wextra

test: test_bin
	./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw

format:
	clang-format -i test.cpp model.hpp

ready: test format clean
	git status

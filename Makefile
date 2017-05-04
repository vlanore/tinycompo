.PHONY: test clean format ready

all: test_bin

test_bin: test.cpp tinycompo.hpp
	$(CXX) $< -o $@ --std=gnu++11 $(TINYCOMPO_FLAGS) -Wall -Wextra

test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw

format:
	clang-format -i test.cpp tinycompo.hpp arrays.hpp

ready: test format
	git status

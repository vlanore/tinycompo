TEST_FILES = $(shell ls -d -1 $$PWD/test/*.*)

.PHONY: test clean format ready

all: test_bin

test_bin: test.cpp tinycompo.hpp $(TEST_FILES)
	$(CXX) $< -o $@ --std=gnu++11 $(TINYCOMPO_FLAGS) -Wall -Wextra

test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw

format:
	clang-format -i test.cpp tinycompo.hpp $(TEST_FILES)

ready: test format
	git status

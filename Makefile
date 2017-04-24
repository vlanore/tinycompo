.PHONY: test clean format ready

all: test_bin

test_bin: test.cpp model.hpp
	$(CXX) $< -o $@ --std=gnu++11 $(TINYCOMPO_FLAGS) -Wall -Wextra

test: test_bin
	rm -f *.profraw *.gcov *.gcno *.gcda
	./$<

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw

format:
	clang-format -i test.cpp model.hpp

ready: test format
	git status

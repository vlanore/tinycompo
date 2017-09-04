TEST_FILES = $(shell ls -d -1 $$PWD/test/*.*)
EXAMPLE_FILES = $(shell ls -d -1 $$PWD/example/*.cpp)

.PHONY: test clean format ready

all: test_bin example/hello_bin example/text_process_bin example/myarray_bin

test_bin: test.cpp tinycompo.hpp $(TEST_FILES)
	$(CXX) $< -o $@ --std=gnu++11 $(TINYCOMPO_FLAGS) -Wall -Wextra

test: test_bin
	rm -f *.profraw *.gcov *.gcda
	./$<

%_bin: %.cpp tinycompo.hpp
	$(CXX) $< -o $@ -I. --std=gnu++11 -Wall -Wextra

clean:
	rm -f *.o *_bin *.gcov *.gcno *.gcda *.profraw example/*_bin

format:
	clang-format -i test.cpp tinycompo.hpp $(TEST_FILES) $(EXAMPLE_FILES)

ready: test format
	git status

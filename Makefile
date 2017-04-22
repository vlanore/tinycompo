.PHONY: test clean format

all: test_bin

test_bin: test.cpp model.hpp
	$(CXX) $< -o $@ --std=gnu++11

test: test_bin
	./$<

clean:
	rm -f *.o *_bin

format:
	clang-format -i test.cpp model.hpp

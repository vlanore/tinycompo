all: test_bin

test_bin: test.cpp
	$(CXX) $< -o $@ --std=gnu++11

all: test_bin

test_bin: test.cpp
	$(CXX) $< -o $@ --std=gnu++11

test: test_bin
	./$<

clean:
	rm -f *.o *_bin

CXX = g++
CFLAGS = -std=c++17 -Wall -O3
TESTFLAGS = -o $@ $^
OBJFLAGS = -o $@ -c $<
TARGET = basicxml.o
TESTS = tests/bin/test1 tests/bin/test_config

all: $(TARGET)

tests: $(TESTS)

tests/bin/test1: tests/test1.cpp basicxml.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

tests/bin/test_config: tests/test_config.cpp basicxml.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

basicxml.o: basicxml.cpp basicxml.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

clean:
	-rm *.o
	-rm tests/bin/*
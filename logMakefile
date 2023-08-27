CXX = g++
CFLAGS = -std=c++17 -fPIC -Wall
TESTFLAGS = -o $@ $^
OBJFLAGS = -o $@ -c $<
TARGET = logcpp.o
TESTS = tests/bin/test1

all: $(TARGET)

tests: $(TESTS)

tests/bin/test1: tests/test1.cpp logcpp.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

logcpp.o: logcpp.cpp logcpp.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

clean:
	-rm *.o
	-rm tests/bin/*
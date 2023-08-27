CXX = g++
CFLAGS = -std=c++17 -Wall -O3
TESTFLAGS = -o $@ $^
OBJFLAGS = -o $@ -c $<
TARGET = libQ.so
TESTS = tests/bin/log_test1 tests/bin/log_test_config tests/bin/xml_test1

all: $(TARGET)

tests: $(TESTS)

# XML
tests/bin/test1: tests/test1.cpp basicxml.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

tests/bin/test_config: tests/test_config.cpp basicxml.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

xml.o: basicxml.cpp basicxml.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)


# Log
tests/bin/test1: tests/test1.cpp logcpp.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

log.o: logcpp.cpp logcpp.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

clean:
	-rm *.o
	-rm tests/bin/*

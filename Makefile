CXX = clang++
CFLAGS = -std=c++17 -Wall -O3 -I include/
OBJFLAGS = -o $@ -c $< -fPIC
SOFLAGS = -o $@ $^ -shared
TARGET = libQ.so
CLASSES = xml.o log.o cycle_timer.o

.PHONY: all tests
all: $(TARGET)
tests:
	$(MAKE) -C tests/

libQ.so: $(CLASSES)
	$(CXX) $(SOFLAGS) $(CFLAGS)

# XML
xml.o: src/xml.cpp include/xml.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

# Log
log.o: src/log.cpp include/log.h include/colorcodes.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

# Cycle Timer
cycle_timer.o: src/cycle_timer.cpp include/cycle_timer.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

.PHONY: clean
clean:
	-rm *.o $(TARGET)
	-$(MAKE) clean -C tests/

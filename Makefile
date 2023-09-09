CXX = clang++
CFLAGS = -std=c++17 -Wall -O3 -I include/
OBJFLAGS = -o $@ -c $< -fPIC
SOFLAGS = -o $@ $^ -shared
TARGET = lib/libQ.so
CLASSES = lib/xml.o lib/log.o lib/cycle_timer.o lib/event_timer.o

BUILDABLETYPES = %. %.cpp %.s %.o %.a
TESTFLAGS = -o $@ $(filter $(BUILDABLETYPES),$^)
CPDATA = cp $(filter-out $(BUILDABLETYPES),$^) $(dir $@)

.PHONY: all
all: $(TARGET)

#------Shared Objects------
lib/libQ.so: $(CLASSES)
	$(CXX) $(SOFLAGS) $(CFLAGS)


#------Objects------
# XML
lib/xml.o: src/xml.cpp include/xml.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

# Log
lib/log.o: src/log.cpp include/log.h include/colorcodes.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

# Cycle Timer
lib/cycle_timer.o: src/cycle_timer.cpp include/cycle_timer.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)

#Event Timer
lib/event_timer.o: src/event_timer.cpp include/event_timer.h
	$(CXX) $(OBJFLAGS) $(CFLAGS)


#------Tests------
.PHONY: tests
tests: tests/bin/event_timer_test1 #bin/log_test1 bin/xml_test1 bin/xml_test_config

# XML
tests/bin/xml_test1: tests/xml/test1.cpp lib/xml.o tests/xml/test1.xml
	$(CXX) $(TESTFLAGS) $(CFLAGS)
	$(CPDATA)

tests/bin/xml_test_config: tests/xml/test_config.cpp lib/xml.o tests/xml/test_config.xml
	$(CXX) $(TESTFLAGS) $(CFLAGS)
	$(CPDATA)

# Log
tests/bin/log_test1: tests/log/test1.cpp lib/log.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

# Event Timer
tests/bin/event_timer_test1: tests/event_timer/test1.cpp lib/event_timer.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)


#------Clean------
.PHONY: clean
clean:
	-rm lib/* tests/bin/*
	-$(MAKE) clean -C tests/

#------Install------
.PHONY: install
install: libQ.so
	-cp libQ.so /usr/local/lib
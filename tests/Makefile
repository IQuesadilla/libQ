BUILDABLETYPES = %.c %.cpp %.s %.o %.a
TESTFLAGS = -o $@ $(filter $(BUILDABLETYPES),$^)
CPDATA = cp $(filter-out $(BUILDABLETYPES),$^) $(dir $@)
CFLAGS = -std=c++17 -Wall -O3 -I ../include/

.PHONY: all
all: bin/log_test1 bin/xml_test1 bin/xml_test_config

# XML
bin/xml_test1: xml/test1.cpp ../xml.o xml/test1.xml
	$(CXX) $(TESTFLAGS) $(CFLAGS)
	$(CPDATA)

bin/xml_test_config: xml/test_config.cpp ../xml.o xml/test_config.xml
	$(CXX) $(TESTFLAGS) $(CFLAGS)
	$(CPDATA)

# Log
bin/log_test1: log/test1.cpp ../log.o
	$(CXX) $(TESTFLAGS) $(CFLAGS)

OBJECTS = ../log.o ../xml.o

.PHONY: $(OBJECTS)
$(OBJECTS):
	$(MAKE) $(notdir $@) -C ../

.PHONY: clean
clean:
	-rm bin/*
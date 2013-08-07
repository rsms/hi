all: program
include $(shell HI_DEBUG="$(DEBUG)" ../../build_config.sh)
objects := $(patsubst %.cc,%.o,$(wildcard *.cc))
program: libhi $(objects)
	$(LD) $(HI_LDFLAGS) -o $@ $(objects)
.cc.o:
	$(CXX) $(HI_COMMON_FLAGS) $(HI_CXXFLAGS) -MMD -c $< -o $@
clean:
	rm -rf $(objects) $(objects:.o=.d) program
-include ${objects:.o=.d}
.PHONY: clean

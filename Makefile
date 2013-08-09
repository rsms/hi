project_id := hi
all: $(project_id)
HI_INTERNAL := 1
include $(shell HI_DEBUG="$(DEBUG)" ./build_config.sh)

# Sources
lib_sources  := $(wildcard src/*.cc)
lib_headers  := $(patsubst src/%,%,$(wildcard src/common*.h)) hi.h rusage.h
test_sources := $(wildcard test/*.cc)

# --- conf ---------------------------------------------------------------------

# Compiler and linker flags
c_flags         := $(HI_BUILD_CFLAGS) -MMD
cxx_flags       := $(HI_BUILD_CXXFLAGS) -MMD

# Source objects
object_dir      := $(HI_BUILD_OBJDIR)
lib_objects     := $(lib_sources:%.c=%.o)
lib_objects     := $(lib_objects:%.cc=%.o)
lib_objects     := $(lib_objects:%.mm=%.o)
lib_objects     := $(patsubst %,$(object_dir)/%,$(lib_objects))
test_programs   := $(sort $(patsubst test/%.cc,test/%,$(test_sources)))
test_objects    := $(patsubst %,$(object_dir)/%,$(test_sources:.cc=.o))
test_suit_dir   := $(object_dir)/.testsuit
test_objects2   := $(patsubst %,$(test_suit_dir)/%,$(test_sources:.cc=.o))
test_programs2  := $(sort $(patsubst test/%.cc,$(test_suit_dir)/%,$(test_sources)))
object_dirs     := $(call hi_uniquedirs,$(lib_objects)) \
                   $(call hi_uniquedirs,$(test_objects)) \
                   $(call hi_uniquedirs,$(test_objects2)) \
                   $(call hi_uniquedirs,$(test_programs2))

# Library
static_library 	:= "$(HI_LIB_PREFIX)"/lib$(project_id).a
lib_headers_dir := $(HI_INCLUDE_PREFIX)/$(project_id)
lib_headers     := $(patsubst %,$(lib_headers_dir)/%,$(lib_headers))
lib_header_dirs := $(call hi_uniquedirs,$(lib_headers))

DEPS_ROOT := $(HI_SRCROOT)/deps

# --- targets ---------------------------------------------------------------------

clean:
	rm -rf "$(HI_PREFIX)"
	rm -rf $(test_programs)

common_pre:
	@mkdir -p $(object_dirs)
	@mkdir -p $(lib_header_dirs)
	@mkdir -p "$(HI_LIB_PREFIX)"
	@mkdir -p "$(HI_INCLUDE_PREFIX)"

$(project_id): common_pre lib$(project_id)

# Create library archive
lib$(project_id): common_pre \
	                openssl \
	                leveldb \
	                libuv \
									$(lib_headers) \
                  $(static_library)
$(static_library): $(lib_objects)
	@mkdir -p $(dir $(static_library))
	@$(AR) -rcL $@ $^

# Copy public library headers
$(lib_headers_dir)/%.h: src/%.h
	@cp $^ $@

# Build and run tests
#   To run all tests:
#      make test
#   To run a specific test:
#      make test/foo
#   Test name-to-source maps: test/<name> -> src/<name>.test.cc
test: c_flags   += -DHI_TEST_SUIT_RUNNING=1 -DHI_IS_TEST=1
test: cxx_flags += -DHI_TEST_SUIT_RUNNING=1 -DHI_IS_TEST=1
test: $(test_programs2)
	@(cd test; \
		for t in $^; do \
		  n=`echo "$$t" | sed 's,$(test_suit_dir)/,test/,'`; \
		  printf "\033[1;36;40mRunning $$n\033[0m ... "; \
		  if ("$$t" >/dev/null); then \
		    echo "\033[1;32;40mPASS\033[0m"; \
	    else \
	      echo "\033[1;41;37m FAIL \033[0m"; \
	      exit $$?; \
	    fi; \
	  done)
$(test_suit_dir)/%: lib$(project_id) $(test_suit_dir)/%.o
	$(LD) $(HI_LDFLAGS) $(word 2,$^) -o $@
test/%: c_flags   += -DHI_IS_TEST=1 -g
test/%: cxx_flags += -DHI_IS_TEST=1 -g
test/%: lib$(project_id) $(object_dir)/test/%.o
	$(LD) $(HI_LDFLAGS) $(word 2,$^) -o $@
	@echo "\033[1;36;40mRunning $@\033[0m"
	@($@ ; exit 0)

$(test_suit_dir)/%.o: test/%.cc
	$(CXX) $(cxx_flags) -c -o $@ $<

-include ${test_objects:.o=.d}
-include ${test_objects2:.o=.d}

# Dependencies
ifneq ($(HI_WITH_OPENSSL),)
openssl:
	@rm -f "$(HI_INCLUDE_PREFIX)"/openssl
	@ln -fs $(HI_WITH_OPENSSL)/include/openssl "$(HI_INCLUDE_PREFIX)"/openssl
	@ln -fs $(HI_WITH_OPENSSL)/lib/libssl.* "$(HI_LIB_PREFIX)"/
	@ln -fs $(HI_WITH_OPENSSL)/lib/libcrypto.* "$(HI_LIB_PREFIX)"/
else
openssl: "$(HI_INCLUDE_PREFIX)"/openssl "$(HI_LIB_PREFIX)"/libssl.a "$(HI_LIB_PREFIX)"/libcrypto.a
"$(HI_INCLUDE_PREFIX)"/openssl:
	@rm -f $@ && ln -fs "$(DEPS_ROOT)"/openssl/include/openssl $@
"$(HI_LIB_PREFIX)"/libssl.a:
	@rm -f "$(HI_LIB_PREFIX)"/libssl.*
	@ln -fs "$(DEPS_ROOT)"/openssl/libssl.a $@
"$(HI_LIB_PREFIX)"/libcrypto.a:
	@rm -f "$(HI_LIB_PREFIX)"/libcrypto.*
	@ln -fs "$(DEPS_ROOT)"/openssl/libcrypto.a $@
endif

leveldb: "$(HI_INCLUDE_PREFIX)"/leveldb "$(HI_LIB_PREFIX)"/libleveldb.a
"$(HI_INCLUDE_PREFIX)"/leveldb:
	@rm -f $@ && ln -fs "$(DEPS_ROOT)"/leveldb/include/leveldb $@
"$(HI_LIB_PREFIX)"/libleveldb.a:
	@[ -f $@ ] || CXXFLAGS='-stdlib=libc++' LDFLAGS='-lc++' \
	             $(MAKE) CC="$(CC)" CXX="$(CXX)" LD="$(LD)" TARGET_OS="$(HI_TARGET_OS)" \
	             -C "$(DEPS_ROOT)"/leveldb
	@mkdir -p "$(HI_LIB_PREFIX)"
	@ln -fs "$(DEPS_ROOT)"/leveldb/libleveldb.a $@

libuv: "$(HI_INCLUDE_PREFIX)"/uv.h "$(HI_INCLUDE_PREFIX)"/uv-private "$(HI_LIB_PREFIX)"/libuv.a
"$(HI_INCLUDE_PREFIX)"/uv.h:
	@ln -fs "$(DEPS_ROOT)"/uv/include/uv.h $@
"$(HI_INCLUDE_PREFIX)"/uv-private:
	@rm -f $@ && ln -fs "$(DEPS_ROOT)"/uv/include/uv-private $@
"$(HI_LIB_PREFIX)"/libuv.a:
	@[ -f $@ ] || $(MAKE) -C "$(DEPS_ROOT)"/uv
	@mkdir -p "$(HI_LIB_PREFIX)"
	@ln -fs "$(DEPS_ROOT)"/uv/libuv.a $@

# source -> object
$(object_dir)/%.o: %.cc
	$(CXX) $(cxx_flags) -c -o $@ $<
$(object_dir)/%.o: %.mm
	$(CXX) $(cxx_flags) -c -o $@ $<
$(object_dir)/%.o: %.c
	$(CC) $(c_flags) -c -o $@ $<
$(object_dir)/%.o: %.m
	$(CC) $(c_flags) -c -o $@ $<

# header dependencies
-include ${lib_objects:.o=.d}

.PHONY: clean common_pre $(project_id) lib$(project_id) test

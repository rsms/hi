#!/bin/bash
#
# Call and include from your project's makefile and depend on "libhi". Example makefile:
#
#   all: mything
#   include $(shell HI_DEBUG="$(DEBUG)" deps/hi/build_config.sh)
#   objects := $(patsubst %.cc,%.o,$(wildcard src/*.cc))
#   mything: libhi $(objects)
#     $(LD) $(HI_LDFLAGS) -o $@ $(objects)
#   .cc.o:
#     $(CXX) $(HI_COMMON_FLAGS) $(HI_CXXFLAGS) -MMD -c $< -o $@
#   clean:
#     rm -rf $(objects) $(objects:.o=.d) mything
#     $(MAKE) -C deps/hi $@
#   -include $(objects:.o=.d)
#   .PHONY: clean
#
# The following variables will be available in your makefile:
#
#   NAME                EXAMPLE VALUE
#   ------------------  -------------------------------------------------------------------
#   CC                  cc
#   CXX                 c++
#   LD                  clang
#   HI_TARGET_OS        Darwin
#   HI_TARGET_ARCH      x86_64
#   HI_SRCROOT          /mything/deps/hi
#   HI_PREFIX           /mything/deps/hi/build
#   HI_LIB_PREFIX       /mything/deps/hi/build/lib-x86_64
#   HI_INCLUDE_PREFIX   /mything/deps/hi/build/include
#   HI_COMMON_FLAGS     -Wall -g -O3 -DNDEBUG -arch x86_64
#   HI_CFLAGS           -std=c11 -I/mything/deps/hi/build/include
#   HI_CXXFLAGS         -std=c++11 -stdlib=libc++ -I/mything/deps/hi/build/include
#   HI_LDFLAGS          -lc++ -L/mything/deps/hi/build/lib-x86_64 -luv -lleveldb -lssl -lhi
#
# The actual values can be listed through the convenience make target "hiprintvars":
#
#   make hiprintvars
#
HI_SRCROOT="$(cd "$(dirname "$0")" && pwd)"
HI_HOST_ARCH=$(uname -m | sed s,i[3456789]86,ia32,)

if test -z "$CC";           then CC=cc; fi
if test -z "$CXX";          then CXX=c++; fi
if test -z "$LD";           then LD=clang; fi
if test -z "$TMPDIR";       then TMPDIR=/tmp; fi
if test -z "$TARGET_OS";    then TARGET_OS=$(uname -s); fi
if test -z "$TARGET_ARCH";  then TARGET_ARCH="$HI_HOST_ARCH"; fi

OUTPUT="$HI_SRCROOT/.build_config.mk"
rm -f $OUTPUT
touch $OUTPUT

DEBUG_SUFFIX=; if test -n "$HI_DEBUG"; then DEBUG_SUFFIX=-debug; fi
HI_PREFIX="$HI_SRCROOT/build"
HI_INCLUDE_PREFIX="$HI_PREFIX/include"
HI_LIB_PREFIX="$HI_PREFIX/lib-${TARGET_ARCH}${DEBUG_SUFFIX}"

HI_CFLAGS="-std=c11 -I$HI_INCLUDE_PREFIX"
HI_CXXFLAGS="-std=c++11 -stdlib=libc++ -I$HI_INCLUDE_PREFIX"
HI_LDFLAGS="-lc++ -L$HI_LIB_PREFIX -luv -lleveldb -lssl"

COMMON_FLAGS="-Wall -g -fno-common"
if test -n "$HI_DEBUG"; then
  COMMON_FLAGS="$COMMON_FLAGS -O0"
  HI_CFLAGS="$HI_CFLAGS -DHI_DEBUG=1"
  HI_CXXFLAGS="$HI_CXXFLAGS -DHI_DEBUG=1"
else
  COMMON_FLAGS="$COMMON_FLAGS -O3 -DNDEBUG"
fi

HI_BUILD_CFLAGS=""
HI_BUILD_CXXFLAGS="-fno-rtti -fno-exceptions"
HI_BUILD_LDFLAGS=
HI_BUILD_OBJDIR="$HI_PREFIX/.objs-${TARGET_ARCH}${DEBUG_SUFFIX}"

case "$TARGET_OS" in
  Darwin)
    HI_PLATFORM=OS_MACOSX
    COMMON_FLAGS="$COMMON_FLAGS -arch $TARGET_ARCH -fobjc-arc"
    HI_LDFLAGS="$HI_LDFLAGS -framework CoreFoundation -framework CoreServices"
    HI_LDFLAGS="$HI_LDFLAGS -lcrypto"
    [ -z "$INSTALL_PATH" ] && INSTALL_PATH=`pwd`
    ;;
  *)
    echo "Unsupported platform '$TARGET_OS'" >&2
    exit 1
esac

HI_BUILD_CFLAGS="$COMMON_FLAGS $HI_BUILD_CFLAGS $HI_CFLAGS"
HI_BUILD_CXXFLAGS="$COMMON_FLAGS $HI_BUILD_CXXFLAGS $HI_CXXFLAGS"
HI_BUILD_LDFLAGS="$HI_BUILD_LDFLAGS $HI_LDFLAGS"

HI_LDFLAGS="$HI_LDFLAGS -lhi"


# CXXOUTPUT="${TMPDIR}/hi-config-detect-cxx.$$"
# check_for_cxx_header() {
#   ($CXX $HI_BUILD_CXXFLAGS -x c++ - -o $CXXOUTPUT 2>/dev/null <<EOF
#     #include <$1>
#     int main() {}
# EOF
#   ) && HI_CXXFLAGS="$HI_CXXFLAGS -DHI_TARGET_HAS_$2"
# }
# check_for_cxx_header 'atomic' 'STDATOMIC'


echo "CC=$CC" >> $OUTPUT
echo "CXX=$CXX" >> $OUTPUT
echo "LD=$LD" >> $OUTPUT
echo "HI_TARGET_OS=$TARGET_OS" >> $OUTPUT
echo "HI_TARGET_ARCH=$TARGET_ARCH" >> $OUTPUT
echo "HI_PLATFORM=$HI_PLATFORM" >> $OUTPUT
echo "HI_BUILD_OBJDIR=$HI_BUILD_OBJDIR" >> $OUTPUT
echo "HI_BUILD_CFLAGS=$HI_BUILD_CFLAGS" >> $OUTPUT
echo "HI_BUILD_CXXFLAGS=$HI_BUILD_CXXFLAGS" >> $OUTPUT
echo "HI_BUILD_LDFLAGS=$HI_BUILD_LDFLAGS" >> $OUTPUT
echo "HI_PREFIX=$HI_PREFIX" >> $OUTPUT
echo "HI_LIB_PREFIX=$HI_LIB_PREFIX" >> $OUTPUT
echo "HI_INCLUDE_PREFIX=$HI_INCLUDE_PREFIX" >> $OUTPUT
echo "HI_SRCROOT=$HI_SRCROOT" >> $OUTPUT
echo "HI_COMMON_FLAGS=$COMMON_FLAGS" >> $OUTPUT
echo "HI_CFLAGS=$HI_CFLAGS" >> $OUTPUT
echo "HI_CXXFLAGS=$HI_CXXFLAGS" >> $OUTPUT
echo "HI_LDFLAGS=$HI_LDFLAGS" >> $OUTPUT

echo '' >> $OUTPUT
echo '# $(call hi_uniquedirs,<list of files>) -> <unique list of dirs>' >> $OUTPUT
echo 'hi_uniquedirs = $(sort $(foreach fn,$(1),$(dir $(fn))))' >> $OUTPUT

echo '' >> $OUTPUT
echo 'ifneq ($(HI_INTERNAL),1)' >> $OUTPUT

# If you are using libhi (only calls make hi when library is out of date compared to source files)
echo 'libhi: $(HI_LIB_PREFIX)/libhi.a' >> $OUTPUT
echo '$(HI_LIB_PREFIX)/libhi.a:' >> $OUTPUT
echo -en "\t" >> $OUTPUT
echo '$(MAKE) TARGET_OS="$(HI_TARGET_OS)" -C "$(HI_SRCROOT)" hi' >> $OUTPUT
echo '$(HI_LIB_PREFIX)/libhi.a: '"$(find $HI_SRCROOT/src -type f -name '*.cc' -or -name '*.h' | xargs)" >> $OUTPUT

# If you are hacking on libhi (always calls make hi)
echo 'libhidev:' >> $OUTPUT
echo -en "\t" >> $OUTPUT
echo '$(MAKE) TARGET_OS="$(HI_TARGET_OS)" -C "$(HI_SRCROOT)" hi' >> $OUTPUT

echo '.PHONY: libhi libhidev' >> $OUTPUT
echo 'endif' >> $OUTPUT

echo 'hiprintvars:' >> $OUTPUT
echo -e "\t@echo "'"CC                = $(CC)"' >> $OUTPUT
echo -e "\t@echo "'"CXX               = $(CXX)"' >> $OUTPUT
echo -e "\t@echo "'"LD                = $(LD)"' >> $OUTPUT
echo -e "\t@echo "'"HI_TARGET_OS      = $(HI_TARGET_OS)"' >> $OUTPUT
echo -e "\t@echo "'"HI_TARGET_ARCH    = $(HI_TARGET_ARCH)"' >> $OUTPUT
echo -e "\t@echo "'"HI_SRCROOT        = $(HI_SRCROOT)"' >> $OUTPUT
echo -e "\t@echo "'"HI_PREFIX         = $(HI_PREFIX)"' >> $OUTPUT
echo -e "\t@echo "'"HI_LIB_PREFIX     = $(HI_LIB_PREFIX)"' >> $OUTPUT
echo -e "\t@echo "'"HI_INCLUDE_PREFIX = $(HI_INCLUDE_PREFIX)"' >> $OUTPUT
echo -e "\t@echo "'"HI_COMMON_FLAGS   = $(HI_COMMON_FLAGS)"' >> $OUTPUT
echo -e "\t@echo "'"HI_CFLAGS         = $(HI_CFLAGS)"' >> $OUTPUT
echo -e "\t@echo "'"HI_CXXFLAGS       = $(HI_CXXFLAGS)"' >> $OUTPUT
echo -e "\t@echo "'"HI_LDFLAGS        = $(HI_LDFLAGS)"' >> $OUTPUT
echo '.PHONY: hiprintvars' >> $OUTPUT

echo $OUTPUT

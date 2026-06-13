CC ?= gcc
CXX ?= g++

VERSION_MAJOR := 1
VERSION := $(VERSION_MAJOR).0.5

# Shared-library naming differs by linker: GNU ld uses -soname and
# libfoo.so.VERSION; Apple ld uses -install_name and libfoo.VERSION.dylib.
# shared_lib(foo) -> output filename; shared_soname(foo) -> soname value.
ifeq ($(shell uname -s),Darwin)
  SHARED_FLAG := -dynamiclib
  SONAME_FLAG := -install_name
  shared_lib = lib$(1).$(VERSION).dylib
  shared_soname = lib$(1).$(VERSION_MAJOR).dylib
else
  SHARED_FLAG := -shared
  SONAME_FLAG := -soname
  shared_lib = lib$(1).so.$(VERSION)
  shared_soname = lib$(1).so.$(VERSION_MAJOR)
endif

override CFLAGS := -W -Wall -Wextra -ansi -pedantic -O3 -Wno-unused-function -fPIC $(CFLAGS)
override CXXFLAGS := -W -Wall -Wextra -ansi -pedantic -O3 -fPIC $(CXXFLAGS)
LDLIBS := -lm $(LDLIBS)

ZOPFLILIB_SRC = src/zopfli/blocksplitter.c src/zopfli/cache.c\
                src/zopfli/deflate.c src/zopfli/gzip_container.c\
                src/zopfli/hash.c src/zopfli/katajainen.c\
                src/zopfli/lz77.c src/zopfli/squeeze.c\
                src/zopfli/tree.c src/zopfli/util.c\
                src/zopfli/zlib_container.c src/zopfli/zopfli_lib.c
ZOPFLILIB_OBJ := $(patsubst %.c,obj/%.o,$(ZOPFLILIB_SRC))
ZOPFLIBIN_SRC := src/zopfli/zopfli_bin.c
ZOPFLIBIN_OBJ := $(patsubst %.c,obj/%.o,$(ZOPFLIBIN_SRC))
LODEPNG_SRC := src/zopflipng/lodepng/lodepng.cpp src/zopflipng/lodepng/lodepng_util.cpp
LODEPNG_OBJ := $(patsubst %.cpp,obj/%.o,$(LODEPNG_SRC))
ZOPFLIPNGLIB_SRC := src/zopflipng/zopflipng_lib.cc
ZOPFLIPNGLIB_OBJ := $(patsubst %.cc,obj/%.o,$(ZOPFLIPNGLIB_SRC))
ZOPFLIPNGBIN_SRC := src/zopflipng/zopflipng_bin.cc
ZOPFLIPNGBIN_OBJ := $(patsubst %.cc,obj/%.o,$(ZOPFLIPNGBIN_SRC))

.PHONY: all libzopfli libzopflipng

all: zopfli libzopfli libzopfli.a zopflipng libzopflipng libzopflipng.a

obj/%.o: %.c
	@mkdir -p `dirname $@`
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

obj/%.o: %.cc
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

obj/%.o: %.cpp
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Zopfli binary
zopfli: $(ZOPFLILIB_OBJ) $(ZOPFLIBIN_OBJ)
	$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS) $(LDLIBS)

# Zopfli shared library
libzopfli: $(ZOPFLILIB_OBJ)
	$(CC) $^ $(CFLAGS) $(SHARED_FLAG) -Wl,$(SONAME_FLAG),$(call shared_soname,zopfli) -o $(call shared_lib,zopfli) $(LDFLAGS) $(LDLIBS)

# Zopfli static library
libzopfli.a: $(ZOPFLILIB_OBJ)
	ar rcs $@ $^

# ZopfliPNG binary
zopflipng: $(ZOPFLILIB_OBJ) $(LODEPNG_OBJ) $(ZOPFLIPNGLIB_OBJ) $(ZOPFLIPNGBIN_OBJ)
	$(CXX) $^ $(CXXFLAGS) -o $@ $(LDFLAGS) $(LDLIBS)

# ZopfliPNG shared library
libzopflipng: $(ZOPFLILIB_OBJ) $(LODEPNG_OBJ) $(ZOPFLIPNGLIB_OBJ)
	$(CXX) $^ $(CXXFLAGS) $(SHARED_FLAG) -Wl,$(SONAME_FLAG),$(call shared_soname,zopflipng) -o $(call shared_lib,zopflipng) $(LDFLAGS) $(LDLIBS)

# ZopfliPNG static library
libzopflipng.a: $(LODEPNG_OBJ) $(ZOPFLIPNGLIB_OBJ)
	ar rcs $@ $^

# Remove all libraries and binaries
clean:
	rm -f zopflipng zopfli $(ZOPFLILIB_OBJ) $(ZOPFLIBIN_OBJ) $(LODEPNG_OBJ) $(ZOPFLIPNGLIB_OBJ) $(ZOPFLIPNGBIN_OBJ) libzopfli*

CC ?= gcc
CXX ?= g++

# Single source of truth: the repo-root VERSION.txt file.
VERSION := $(strip $(shell cat VERSION.txt))
VERSION_MAJOR := $(word 1,$(subst ., ,$(VERSION)))

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

# NDEBUG strips asserts and the ZopfliVerifyLenDist check for the release build.
# User CFLAGS are appended after, so `make CFLAGS=-UNDEBUG` re-enables them.
# C99 (for <stdint.h>) plus the GNU builtins the code uses (__builtin_clz).
override CFLAGS := -W -Wall -Wextra -std=gnu99 -pedantic -O3 -DNDEBUG -fPIC $(CFLAGS)
override CXXFLAGS := -W -Wall -Wextra -std=gnu++11 -pedantic -O3 -DNDEBUG -fPIC $(CXXFLAGS)
LDLIBS := $(LDLIBS)

# Keep `all` the default goal: the version-header rule below would otherwise
# become the first target and steal it.
.DEFAULT_GOAL := all

# Generated version header (ZOPFLI_VERSION), built into the obj/ artifact dir.
GEN_HEADER := obj/version.h
override CPPFLAGS := -Iobj $(CPPFLAGS)

$(GEN_HEADER): src/zopfli/version.h.in VERSION.txt
	@mkdir -p obj
	sed 's/@ZOPFLI_VERSION@/$(VERSION)/g' $< > $@

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

# Objects that include zopfli.h need the generated version header first
# (LodePNG excluded — it doesn't include zopfli.h).
$(ZOPFLILIB_OBJ) $(ZOPFLIBIN_OBJ) $(ZOPFLIPNGLIB_OBJ) $(ZOPFLIPNGBIN_OBJ): \
	$(GEN_HEADER)

.PHONY: all libzopfli libzopflipng

all: zopfli libzopfli libzopfli.a zopflipng libzopflipng libzopflipng.a

obj/%.o: %.c
	@mkdir -p `dirname $@`
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -c $< -o $@

obj/%.o: %.cc
	@mkdir -p `dirname $@`
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -c $< -o $@

# Vendored LodePNG: no -Werror so upstream's warnings don't break the build.
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
	rm -f zopflipng zopfli $(ZOPFLILIB_OBJ) $(ZOPFLIBIN_OBJ) $(LODEPNG_OBJ) $(ZOPFLIPNGLIB_OBJ) $(ZOPFLIPNGBIN_OBJ) $(GEN_HEADER) libzopfli*

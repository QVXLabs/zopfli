cmake_minimum_required(VERSION 3.10)

# Single source of truth for the version: the repo-root VERSION file.
# CONFIGURE_DEPENDS re-runs CMake when it changes (file(READ) isn't auto-tracked).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ZOPFLI_VERSION)
string(STRIP "${ZOPFLI_VERSION}" ZOPFLI_VERSION)

project(Zopfli VERSION ${ZOPFLI_VERSION} LANGUAGES C CXX)

# Generate the public version header into the build tree (not the source tree).
set(ZOPFLI_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/zopfli)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/zopfli/version.h.in
  ${ZOPFLI_GENERATED_DIR}/version.h
  @ONLY)

# C99 for <stdint.h> (fixed-width cost types). The default would usually be new
# enough, but pin it so the dependency is explicit.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

#
# Options
#
# Library type and build config use the standard BUILD_SHARED_LIBS and
# CMAKE_BUILD_TYPE; set those directly.

# Force PIC for static libs too (shared libs are PIC regardless).
option(ZOPFLI_PIC "Build with position-independent code" OFF)

# Line-coverage instrumentation (gcc/clang); use with the test suite.
option(ZOPFLI_COVERAGE "Build with coverage instrumentation" OFF)

# Default ON; gtest is run directly, so include(CTest) is intentionally unused.
if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ON)
endif()

# Read at target creation, so they must precede it.
if(ZOPFLI_PIC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(ZOPFLI_COVERAGE)
  add_compile_options(--coverage -O0 -g)
  link_libraries(--coverage)
endif()

include(GNUInstallDirs)

#
# libzopfli
#
add_library(libzopfli
  src/zopfli/blocksplitter.c
  src/zopfli/cache.c
  src/zopfli/deflate.c
  src/zopfli/gzip_container.c
  src/zopfli/hash.c
  src/zopfli/katajainen.c
  src/zopfli/lz77.c
  src/zopfli/squeeze.c
  src/zopfli/tree.c
  src/zopfli/util.c
  src/zopfli/zlib_container.c
  src/zopfli/zopfli_lib.c
)
target_include_directories(libzopfli
  PUBLIC
    $<BUILD_INTERFACE:${ZOPFLI_GENERATED_DIR}>
  INTERFACE
    # src/zopfli exposes <zopfli.h> (used by the in-tree tests); src exposes
    # <zopfli/zopfli.h> to match the installed/conan layout (include/zopfli/).
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/zopfli>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(libzopfli PROPERTIES
  OUTPUT_NAME zopfli
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
# libzopfli is integer-only now; no libm needed.

#
# libzopflipng
#
add_library(libzopflipng
  src/zopflipng/zopflipng_lib.cc
  src/zopflipng/lodepng/lodepng.cpp
  src/zopflipng/lodepng/lodepng_util.cpp
)
target_link_libraries(libzopflipng PUBLIC libzopfli)
target_include_directories(libzopflipng
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/zopflipng>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(libzopflipng PROPERTIES
  OUTPUT_NAME zopflipng
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

# MSVC does not export symbols by default when building a DLL
if(MSVC AND BUILD_SHARED_LIBS)
  set_target_properties(libzopfli libzopflipng PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
  )
endif()

#
# zopfli
#
add_executable(zopfli src/zopfli/zopfli_bin.c)
target_link_libraries(zopfli PRIVATE libzopfli)

#
# zopflipng
#
add_executable(zopflipng src/zopflipng/zopflipng_bin.cc)
target_link_libraries(zopflipng PRIVATE libzopflipng)

# Silence MSVC warnings on standard C functions like fopen
if(MSVC)
  target_compile_definitions(zopfli PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(zopflipng PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

#
# Warnings as errors (always on; first-party code only, vendored LodePNG is
# exempted). MSVC uses /WX; GCC/Clang use -Werror plus the same warning set as
# the Makefile so the policy matches across build systems.
#
if(MSVC)
  foreach(zopfli_target libzopfli libzopflipng zopfli zopflipng)
    target_compile_options(${zopfli_target} PRIVATE /WX)
  endforeach()
  # Keep LodePNG's warnings non-fatal.
  set_source_files_properties(
    src/zopflipng/lodepng/lodepng.cpp
    src/zopflipng/lodepng/lodepng_util.cpp
    PROPERTIES COMPILE_OPTIONS "/WX-"
  )
else()
  set(zopfli_c_warn -Wall -Wextra -pedantic -Werror)
  set(zopfli_cxx_warn -Wall -Wextra -pedantic -Werror)
  foreach(zopfli_target libzopfli libzopflipng zopfli zopflipng)
    target_compile_options(${zopfli_target} PRIVATE
      $<$<COMPILE_LANGUAGE:C>:${zopfli_c_warn}>
      $<$<COMPILE_LANGUAGE:CXX>:${zopfli_cxx_warn}>
    )
  endforeach()
  # Silence vendored LodePNG entirely (third-party; we don't lint it).
  set_source_files_properties(
    src/zopflipng/lodepng/lodepng.cpp
    src/zopflipng/lodepng/lodepng_util.cpp
    PROPERTIES COMPILE_OPTIONS "-w"
  )
endif()

# Create aliases
#
# Makes targets available to projects using Zopfli as a subproject using the
# same names as in the config file package.
add_library(Zopfli::libzopfli ALIAS libzopfli)
add_library(Zopfli::libzopflipng ALIAS libzopflipng)
add_executable(Zopfli::zopfli ALIAS zopfli)
add_executable(Zopfli::zopflipng ALIAS zopflipng)

#
# Tests
#
if(BUILD_TESTING)
  # Prefer an installed GoogleTest; fall back to the submodule.
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    set(zopfli_gtest_dir ${CMAKE_CURRENT_SOURCE_DIR}/third-party/google-test)
    if(NOT EXISTS ${zopfli_gtest_dir}/CMakeLists.txt)
      message(FATAL_ERROR
        "GoogleTest not found via find_package and the submodule at "
        "third-party/google-test is missing. Run:\n"
        "  git submodule update --init third-party/google-test")
    endif()
    # Match the parent's MSVC runtime choice.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # GoogleTest 1.11.0's cmake_minimum_required is too low for CMake 4.x.
    # 3.10 clears both the hard error and the "< 3.10" deprecation warning.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    add_subdirectory(${zopfli_gtest_dir}
      ${CMAKE_BINARY_DIR}/third-party/google-test EXCLUDE_FROM_ALL)
    unset(CMAKE_POLICY_VERSION_MINIMUM)
  endif()

  add_executable(zopfli_tests
    test/zopfli_public_test.cc
    test/tree_test.cc
    test/katajainen_test.cc
    test/cache_test.cc
    test/hash_test.cc
    test/lz77_test.cc
    test/blocksplitter_test.cc
    test/deflate_test.cc
    test/squeeze_test.cc
    test/fixedpoint_range_test.cc
    test/custom_allocator_test.cc
    test/version_test.cc
  )
  target_link_libraries(zopfli_tests PRIVATE libzopfli GTest::gtest_main)
  set_target_properties(zopfli_tests PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )

  # Optional: link zlib to enable the fixed-point round-trip checks. When
  # absent, those tests skip themselves.
  find_package(ZLIB QUIET)
  if(ZLIB_FOUND)
    target_link_libraries(zopfli_tests PRIVATE ZLIB::ZLIB)
    target_compile_definitions(zopfli_tests PRIVATE ZOPFLI_TEST_HAVE_ZLIB)
  endif()
endif()

#
# Install
#
# Install binaries, libraries, and headers
install(TARGETS libzopfli libzopflipng zopfli zopflipng
  EXPORT ZopfliTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES src/zopfli/zopfli.h src/zopflipng/zopflipng_lib.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zopfli
)
install(FILES ${ZOPFLI_GENERATED_DIR}/version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zopfli
)

# Install config file package
#
# This allows CMake based projects to use the installed libraries with
# find_package(Zopfli).
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ZopfliConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
# Since we have no dependencies, use export file directly as config file
install(EXPORT ZopfliTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Zopfli
  NAMESPACE Zopfli::
  FILE ZopfliConfig.cmake
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ZopfliConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Zopfli
)

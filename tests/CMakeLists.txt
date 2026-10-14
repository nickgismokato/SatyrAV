cmake_minimum_required(VERSION 3.20)

# (1.8.1) This file is usable two ways:
#
#   1. From the root build, when tests are opted in:
#          cmake .. -DSATYR_BUILD_TESTS=ON
#      The root CMakeLists calls add_subdirectory(tests).
#
#   2. Standalone, on a machine with no SDL2 and no FFmpeg:
#          cmake -S tests -B build-tests
#          cmake --build build-tests
#      Everything the suite touches lives under src/core/, which has no
#      SDL or FFmpeg dependency, so the media libraries are not needed to
#      run the core regression net.
#
# Either way the target is `satyr-tests` and it registers with CTest.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(SatyrAVTests LANGUAGES CXX)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	enable_testing()

	set(SATYR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
	file(GLOB SATYR_CORE_SOURCES ${SATYR_ROOT}/src/core/*.cpp)

	include(FetchContent)
	FetchContent_Declare(
		tomlplusplus
		GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
		GIT_TAG        v3.4.0
	)
	FetchContent_MakeAvailable(tomlplusplus)
else()
	# Driven from the root build — SATYR_CORE_SOURCES and the toml++
	# target are already defined there.
	set(SATYR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# ── doctest ──────────────────────────────────────────────────
# Header-only and fetched the same way as toml++, so there is no extra
# system dependency to install. Needs network on the first configure;
# point FETCHCONTENT_SOURCE_DIR_DOCTEST at a local checkout when offline.
include(FetchContent)
FetchContent_Declare(
	doctest
	GIT_REPOSITORY https://github.com/doctest/doctest.git
	GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(satyr-tests ${TEST_SOURCES} ${SATYR_CORE_SOURCES})

target_include_directories(satyr-tests PRIVATE
	${SATYR_ROOT}/src
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(satyr-tests PRIVATE
	tomlplusplus::tomlplusplus
	doctest::doctest
)

# (1.8.2) MinGW: link the C++ runtime into the test binary statically.
#
# doctest keeps the INFO()/CAPTURE() context stack in a `thread_local
# std::vector` (DOCTEST_THREAD_LOCAL in doctest.h). Destroying a
# thread_local with a non-trivial destructor at process exit crashes on
# MinGW when libstdc++ is a DLL. The failure is deeply misleading: every
# assertion passes, doctest prints "Status: SUCCESS!", and *then* the
# process segfaults — so CTest reports a failing test while the output
# above it says everything passed.
#
# Reduced to a five-line program to confirm it is not our code: identical
# source exits 0 with -static-libstdc++ and segfaults when linked against
# libstdc++-6.dll, with a single CAPTURE() as the trigger and no CAPTURE
# meaning no crash. Linking the runtime statically sidesteps it, and has
# the side benefit of making satyr-tests.exe self-contained — no runtime
# DLLs need to sit beside it.
#
# Only applies to the test binary; satyr-av does not use doctest and its
# DLL handling is unchanged.
if(MINGW)
	target_link_options(satyr-tests PRIVATE -static-libgcc -static-libstdc++)
endif()

# One CTest entry running the whole suite. doctest's own runner already
# reports per-case results, so splitting into per-case CTest entries would
# only duplicate that output.
add_test(NAME satyr-core-suite COMMAND satyr-tests)

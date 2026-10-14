cmake_minimum_required(VERSION 3.20)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" SATYR_VERSION)
string(STRIP "${SATYR_VERSION}" SATYR_VERSION)

project(SatyrAV VERSION ${SATYR_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── SDL3 (2.0.0) ──────────────────────────────────────────────
# SDL3 always ships CMake config packages with namespaced targets, so the
# per-distro target-name guessing SDL2 needed is gone. CONFIG is explicit:
# there is no FindSDL3.cmake module, and without it a missing SDL3 fails
# with a confusing "module not found" instead of a clear config error.
#
# Ubuntu 24.04 does not package SDL3. Point CMake at a local build:
#     -DCMAKE_PREFIX_PATH=$HOME/sdl3-prefix
# The MinGW cross-build gets it from MINGW_DEPS_DIR via the toolchain file.
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)

set(SDL3_LINK       SDL3::SDL3)
set(SDL3_TTF_LINK   SDL3_ttf::SDL3_ttf)
set(SDL3_IMAGE_LINK SDL3_image::SDL3_image)

# ── FFmpeg ────────────────────────────────────────────────────
find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE SWRESAMPLE)

# ── toml++ (header-only, fetched automatically) ──────────────
include(FetchContent)
FetchContent_Declare(
	tomlplusplus
	GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
	GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# ── Sources ──────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES src/*.cpp)

# (1.8.1) Everything under src/core/ links without SDL and FFmpeg. The
# unit-test suite reuses exactly this list, which is what lets it build
# and run on a machine that has neither installed.
file(GLOB SATYR_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp)

add_executable(satyr-av ${SOURCES})

# Windows icon
if(WIN32)
	target_sources(satyr-av PRIVATE src/satyr-av.rc)

	# (2.0.0) Build as a GUI-subsystem binary so launching the .exe does not
	# also open a console window.
	#
	# This was implicit under SDL2: the SDL2::SDL2main imported target added
	# `-mwindows` to the link line on MinGW. SDL3::SDL3main deliberately does
	# not — SDL3 leaves the subsystem choice to the application — so without
	# this the exe links as a console binary and Windows opens a terminal
	# alongside it. WIN32_EXECUTABLE is the portable way to say `-mwindows`.
	#
	# Safe with <SDL3/SDL_main.h> in main.cpp: that renames main() to
	# SDL_main(), and SDL3::SDL3main supplies the WinMain entry point that
	# calls it. Note that stdout/stderr go nowhere in this subsystem — the
	# file logger under Documents\SatyrAV\log is the diagnostic channel.
	set_target_properties(satyr-av PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# SDL3's imported targets carry their own include directories, so there is
# no SDL3_INCLUDE_DIRS to add here (SDL2 needed one).
target_include_directories(satyr-av PRIVATE
	src
	${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(satyr-av PRIVATE
	SATYR_AV_VERSION="${SATYR_VERSION}"
)

# (2.0.0) SDL3 replaces the SDL2main/mingw32 link-order dance with a single
# SDL3::SDL3main target, used together with <SDL3/SDL_main.h> in main.cpp.
# It must be linked before SDL3::SDL3.
if(TARGET SDL3::SDL3main)
	target_link_libraries(satyr-av PRIVATE SDL3::SDL3main)
endif()

target_link_libraries(satyr-av PRIVATE
	${SDL3_LINK}
	${SDL3_TTF_LINK}
	${SDL3_IMAGE_LINK}
	tomlplusplus::tomlplusplus
	${FFMPEG_LIBRARIES}
)

# D3D11 for hardware-accelerated video decode (D3D11VA)
if(WIN32)
	target_link_libraries(satyr-av PRIVATE d3d11)
	# (2.0.2) WinHTTP for the backstage screen's POST (core/HttpPost.cpp).
	target_link_libraries(satyr-av PRIVATE winhttp)
endif()

# ── Unit tests (1.8.1) ───────────────────────────────────────
# OFF by default: a normal build of SatyrAV never compiles, downloads, or
# links anything test-related. Turn on explicitly:
#     cmake .. -DSATYR_BUILD_TESTS=ON
# Enabling it adds the `satyr-tests` target and registers it with CTest.
# See tests/README.md — the suite can also be configured on its own,
# without SDL2 / FFmpeg present at all.
option(SATYR_BUILD_TESTS "Build the SDL-independent core unit test suite" OFF)
if(SATYR_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# ── Install ──────────────────────────────────────────────────
install(TARGETS satyr-av DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/satyr-av/assets)

# ── Copy DLLs next to exe (Windows) ─────────────────────────
if(WIN32 AND DEFINED MINGW_DEPS_DIR)
	file(GLOB WIN_DLLS "${MINGW_DEPS_DIR}/bin/*.dll")
	foreach(DLL ${WIN_DLLS})
		add_custom_command(TARGET satyr-av POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:satyr-av>"
		)
	endforeach()
endif()

# Copy MinGW runtime DLLs (libstdc++, libgcc, libwinpthread)
# Must match the compiler's exact threading model (win32 vs posix)
if(WIN32 AND CMAKE_CXX_COMPILER)
	# Ask the compiler where its libstdc++ lives — this guarantees the right variant
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++-6.dll
		OUTPUT_VARIABLE _stdcpp_path
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(_stdcpp_path AND NOT _stdcpp_path STREQUAL "libstdc++-6.dll")
		get_filename_component(_rt_dir "${_stdcpp_path}" DIRECTORY)
		message(STATUS "MinGW runtime dir: ${_rt_dir}")
		foreach(_rtdll libstdc++-6.dll libgcc_s_seh-1.dll)
			if(EXISTS "${_rt_dir}/${_rtdll}")
				add_custom_command(TARGET satyr-av POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_rt_dir}/${_rtdll}" "$<TARGET_FILE_DIR:satyr-av>"
				)
				message(STATUS "Runtime DLL: ${_rt_dir}/${_rtdll}")
			endif()
		endforeach()
	else()
		message(WARNING "Could not locate MinGW runtime dir via compiler")
	endif()

	# libwinpthread is in the sysroot, not the compiler dir
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libwinpthread-1.dll
		OUTPUT_VARIABLE _winpthread_path
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(_winpthread_path AND NOT _winpthread_path STREQUAL "libwinpthread-1.dll" AND EXISTS "${_winpthread_path}")
		add_custom_command(TARGET satyr-av POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_winpthread_path}" "$<TARGET_FILE_DIR:satyr-av>"
		)
		message(STATUS "Runtime DLL: ${_winpthread_path}")
	else()
		# Fallback search
		find_file(_wp libwinpthread-1.dll PATHS /usr/x86_64-w64-mingw32/lib NO_DEFAULT_PATH)
		if(_wp)
			add_custom_command(TARGET satyr-av POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_wp}" "$<TARGET_FILE_DIR:satyr-av>"
			)
			message(STATUS "Runtime DLL: ${_wp}")
		endif()
	endif()
endif()

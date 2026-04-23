cmake_minimum_required(VERSION 3.20)

# Read version from VERSION file
file(READ "${CMAKE_SOURCE_DIR}/VERSION" SATYR_VERSION)
string(STRIP "${SATYR_VERSION}" SATYR_VERSION)

project(SatyrAV VERSION ${SATYR_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── SDL2 ──────────────────────────────────────────────────────
# Try config mode first (works with MinGW dev packages), then module mode
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)

# Normalise SDL2 targets — different distros/packages expose different names
if(TARGET SDL2::SDL2)
	set(SDL2_LINK SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
	set(SDL2_LINK SDL2::SDL2-static)
else()
	set(SDL2_LINK ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
	set(SDL2_TTF_LINK SDL2_ttf::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf-static)
	set(SDL2_TTF_LINK SDL2_ttf::SDL2_ttf-static)
else()
	set(SDL2_TTF_LINK SDL2_ttf)
endif()

if(TARGET SDL2_image::SDL2_image)
	set(SDL2_IMAGE_LINK SDL2_image::SDL2_image)
elseif(TARGET SDL2_image::SDL2_image-static)
	set(SDL2_IMAGE_LINK SDL2_image::SDL2_image-static)
else()
	set(SDL2_IMAGE_LINK SDL2_image)
endif()

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

add_executable(satyr-av ${SOURCES})

# Windows icon
if(WIN32)
	target_sources(satyr-av PRIVATE src/satyr-av.rc)
endif()

target_include_directories(satyr-av PRIVATE
	src
	${SDL2_INCLUDE_DIRS}
	${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(satyr-av PRIVATE
	SATYR_AV_VERSION="${SATYR_VERSION}"
)

# On Windows, SDL2main must come BEFORE SDL2, and mingw32 before both
if(WIN32)
	target_link_libraries(satyr-av PRIVATE mingw32)
	if(TARGET SDL2::SDL2main)
		target_link_libraries(satyr-av PRIVATE SDL2::SDL2main)
	endif()
endif()

target_link_libraries(satyr-av PRIVATE
	${SDL2_LINK}
	${SDL2_TTF_LINK}
	${SDL2_IMAGE_LINK}
	tomlplusplus::tomlplusplus
	${FFMPEG_LIBRARIES}
)

# D3D11 for hardware-accelerated video decode (D3D11VA)
if(WIN32)
	target_link_libraries(satyr-av PRIVATE d3d11)
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

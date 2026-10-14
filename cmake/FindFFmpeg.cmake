# FindFFmpeg.cmake — locate FFmpeg libraries (works with MinGW cross-compile)
# Usage: find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE SWRESAMPLE)

set(FFMPEG_INCLUDE_DIRS "")
set(FFMPEG_LIBRARIES "")

foreach(_comp ${FFmpeg_FIND_COMPONENTS})
	string(TOLOWER ${_comp} _lower)

	find_path(${_comp}_INCLUDE_DIR
		NAMES lib${_lower}/${_lower}.h
		PATH_SUFFIXES ffmpeg include
	)

	# Try finding library in this priority:
	#  1. libavcodec.dll.a  (proper MinGW import lib)
	#  2. libavcodec.a      (static)
	#  3. avcodec.lib       (MSVC import lib — MinGW linker accepts these)
	#  4. avcodec.dll       (direct DLL linking — last resort)
	find_library(${_comp}_LIBRARY
		NAMES
			${_lower}       # finds libavcodec.dll.a or libavcodec.a via MinGW conventions
			lib${_lower}    # explicit prefix
		PATH_SUFFIXES lib
	)

	# If not found via find_library, try the MSVC .lib directly
	if(NOT ${_comp}_LIBRARY)
		find_file(${_comp}_LIBRARY
			NAMES ${_lower}.lib
			PATH_SUFFIXES lib
		)
	endif()

	# Last resort: link against the DLL directly
	if(NOT ${_comp}_LIBRARY)
		file(GLOB _dll_candidates
			"${MINGW_DEPS_DIR}/bin/${_lower}*.dll"
			"${MINGW_DEPS_DIR}/bin/${_lower}-*.dll"
		)
		if(_dll_candidates)
			list(GET _dll_candidates 0 ${_comp}_LIBRARY)
		endif()
	endif()

	if(${_comp}_INCLUDE_DIR AND ${_comp}_LIBRARY)
		list(APPEND FFMPEG_INCLUDE_DIRS ${${_comp}_INCLUDE_DIR})
		list(APPEND FFMPEG_LIBRARIES ${${_comp}_LIBRARY})
		set(FFmpeg_${_comp}_FOUND TRUE)
		message(STATUS "FFmpeg ${_comp}: ${${_comp}_LIBRARY}")
	else()
		set(FFmpeg_${_comp}_FOUND FALSE)
		message(STATUS "FFmpeg ${_comp}: NOT FOUND")
		message(STATUS "  header: ${${_comp}_INCLUDE_DIR}")
		message(STATUS "  lib:    ${${_comp}_LIBRARY}")
	endif()
endforeach()

list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
	REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS
	HANDLE_COMPONENTS)

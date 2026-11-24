# Render tests: the real app — Renderer, SlaveWindow, TextRenderer,
# MediaPlayer, PerformanceScreen, App — run with SDL's off-screen video
# driver and dummy audio driver, so no window opens and no sound plays.
#
# Only from the root build (they need SDL3 and FFmpeg), and only with
# -DSATYR_BUILD_TESTS=ON, like the core suite. They build every app source
# except main.cpp a second time, so they are a separate target that a
# plain test run can skip:
#     cmake --build build --target satyr-render-tests
#     ./build/tests/render/satyr-render-tests
#
# Each test points HOME / USERPROFILE at a scratch folder before SDL
# starts, so config.toml, the logs and every file the app writes stay out
# of the real user's folders.

file(GLOB_RECURSE SATYR_APP_SOURCES ${SATYR_ROOT}/src/*.cpp)
list(FILTER SATYR_APP_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
file(GLOB RENDER_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(satyr-render-tests ${RENDER_TEST_SOURCES} ${SATYR_APP_SOURCES})

target_include_directories(satyr-render-tests PRIVATE
	${SATYR_ROOT}/src
	${SATYR_ROOT}/tests
	${CMAKE_CURRENT_SOURCE_DIR}
	${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(satyr-render-tests PRIVATE
	SATYR_AV_VERSION="${SATYR_VERSION}"
	# FindAsset looks for assets/ from the working directory.
	SATYR_SOURCE_ROOT="${SATYR_ROOT}"
)

target_link_libraries(satyr-render-tests PRIVATE
	${SDL3_LINK}
	${SDL3_TTF_LINK}
	${SDL3_IMAGE_LINK}
	tomlplusplus::tomlplusplus
	${FFMPEG_LIBRARIES}
	doctest::doctest
)
find_package(Threads REQUIRED)
target_link_libraries(satyr-render-tests PRIVATE Threads::Threads)
if(WIN32)
	target_link_libraries(satyr-render-tests PRIVATE d3d11 winhttp)
endif()
if(MINGW)
	# Same doctest / MinGW runtime issue as satyr-tests (see ../CMakeLists.txt).
	target_link_options(satyr-render-tests PRIVATE -static-libgcc -static-libstdc++)
endif()

add_test(NAME satyr-render-suite COMMAND satyr-render-tests
	WORKING_DIRECTORY ${SATYR_ROOT})

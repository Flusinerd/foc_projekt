# Plot benchmark Qwt vs. JKQtPlotter (decision for the Qt GUI, see docs/GUI_Entscheidungen.md).
# Standalone project so the main build stays untouched:
#   cmake -S host/bench/plotbench -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake_minimum_required(VERSION 3.20)
project(plotbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Concurrent)
include(FetchContent)

# --- Qwt 6.3.0 (qmake only upstream -> own minimal target, no SVG/OpenGL) -----------------
FetchContent_Declare(qwt
    GIT_REPOSITORY https://git.code.sf.net/p/qwt/git
    GIT_TAG        v6.3.0
    GIT_SHALLOW    TRUE)
FetchContent_GetProperties(qwt)
if(NOT qwt_POPULATED)
    FetchContent_Populate(qwt)
endif()
file(GLOB QWT_SOURCES ${qwt_SOURCE_DIR}/src/*.cpp)
list(FILTER QWT_SOURCES EXCLUDE REGEX "qwt_(plot_svgitem|plot_glcanvas|plot_opengl_canvas|polar_[a-z_]+)\\.cpp$")
add_library(qwt STATIC ${QWT_SOURCES})
target_include_directories(qwt PUBLIC ${qwt_SOURCE_DIR}/src)
target_compile_definitions(qwt PUBLIC QWT_NO_SVG QWT_NO_OPENGL)
target_link_libraries(qwt PUBLIC Qt6::Widgets Qt6::PrintSupport Qt6::Concurrent)

# --- JKQtPlotter 4.0.3 (native CMake) ----------------------------------------------------
set(JKQtPlotter_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JKQtPlotter_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(JKQtPlotter_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JKQtPlotter_BUILD_DECORATE_LIBNAMES_WITH_BUILDTYPE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jkqtplotter
    GIT_REPOSITORY https://github.com/jkriege2/JKQtPlotter.git
    GIT_TAG        v4.0.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(jkqtplotter)

# --- benchmark programs ------------------------------------------------------------------
add_executable(plotbench_qwt bench_qwt.cpp bench_common.hpp)
target_link_libraries(plotbench_qwt PRIVATE qwt)

add_executable(plotbench_jkqt bench_jkqt.cpp bench_common.hpp)
target_link_libraries(plotbench_jkqt PRIVATE JKQTPlotterLib)

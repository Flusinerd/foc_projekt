# Qwt 6.3.0 as static target `qwt` (LGPL 2.1 with static-linking exception, see docs/LICENSES.md).
# Upstream only ships qmake files, so the library is compiled from its sources here.
# Left out: SVG, OpenGL canvases, polar plots (not used, keeps Qt deps at Widgets/PrintSupport/Concurrent).
include_guard(GLOBAL)
include(FetchContent)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Concurrent)

FetchContent_Declare(qwt
    GIT_REPOSITORY https://git.code.sf.net/p/qwt/git
    GIT_TAG        v6.3.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(qwt)   # no CMakeLists.txt upstream -> only downloads

file(GLOB QWT_SOURCES ${qwt_SOURCE_DIR}/src/*.cpp)
list(FILTER QWT_SOURCES EXCLUDE REGEX "qwt_(plot_svgitem|plot_glcanvas|plot_opengl_canvas|polar_[a-z_]+)\\.cpp$")
add_library(qwt STATIC ${QWT_SOURCES})
set_target_properties(qwt PROPERTIES AUTOMOC ON)
target_include_directories(qwt SYSTEM PUBLIC ${qwt_SOURCE_DIR}/src)
target_compile_definitions(qwt PUBLIC QWT_NO_SVG QWT_NO_OPENGL)
target_link_libraries(qwt PUBLIC Qt6::Widgets Qt6::PrintSupport Qt6::Concurrent)

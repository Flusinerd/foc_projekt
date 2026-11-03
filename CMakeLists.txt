cmake_minimum_required(VERSION 3.20)
project(foc_framework VERSION 0.1.0 LANGUAGES C CXX)

option(FOC_BUILD_GUI     "Build FOC Studio (Qt 6 Widgets, set CMAKE_PREFIX_PATH to the Qt kit)" ON)
option(FOC_BUILD_DEV_GUI "Build the ImGui developer tool focstudio-dev (needs OpenGL + GLFW)" ON)
option(FOC_BUILD_TESTS   "Build unit/integration tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(protocol)

# firmware binding (compiled on the host to keep it in sync; on target it is Custom Code)
add_library(focp_target STATIC firmware/focp_target.c)
target_include_directories(focp_target PUBLIC firmware)
target_link_libraries(focp_target PUBLIC focp)
set_target_properties(focp_target PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

# --- host core -------------------------------------------------------------------------
find_package(Threads REQUIRED)
add_library(fochost STATIC
    host/core/src/Device.cpp
    host/core/src/SimTransport.cpp
    host/core/src/VirtualDrive.cpp
    host/core/src/CanTransports.cpp
    host/core/src/GsUsbTransport.cpp
    host/core/src/GsUsbWinUsb.cpp
)
target_include_directories(fochost PUBLIC host/core/include)
target_link_libraries(fochost PUBLIC focp Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(fochost PUBLIC winusb setupapi)
endif()
if(MSVC)
    target_compile_options(fochost PRIVATE /W4)
else()
    target_compile_options(fochost PRIVATE -Wall -Wextra)
endif()

# --- GUI -------------------------------------------------------------------------------
if(FOC_BUILD_GUI)
    add_subdirectory(host/studio)
endif()
if(FOC_BUILD_DEV_GUI)
    add_subdirectory(host/app)
endif()

# --- tests -----------------------------------------------------------------------------
if(FOC_BUILD_TESTS)
    enable_testing()
    add_executable(test_protocol host/tests/test_protocol.cpp)
    target_link_libraries(test_protocol PRIVATE fochost focp_target)
    add_test(NAME protocol COMMAND test_protocol)

    add_executable(test_virtual_drive host/tests/test_virtual_drive.cpp)
    target_link_libraries(test_virtual_drive PRIVATE fochost)
    add_test(NAME virtual_drive COMMAND test_virtual_drive)

    add_executable(test_device host/tests/test_device.cpp)
    target_link_libraries(test_device PRIVATE fochost)
    add_test(NAME device_sim COMMAND test_device)

    add_executable(test_gsusb host/tests/test_gsusb.cpp)
    target_link_libraries(test_gsusb PRIVATE fochost)
    add_test(NAME gsusb COMMAND test_gsusb)
endif()

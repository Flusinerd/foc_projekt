# FOC Studio - end-user application (Qt 6 Widgets). Qt-free core: fochost.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
include(${PROJECT_SOURCE_DIR}/cmake/Qwt.cmake)

# Qt Core only parts, also used by the tests.
add_library(focstudio_core STATIC ParamFile.cpp ParamFile.hpp)
target_include_directories(focstudio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(focstudio_core PUBLIC fochost Qt6::Core)

qt_add_executable(focstudio WIN32
    main.cpp
    ParametersPage.cpp ParametersPage.hpp
    MainWindow.cpp MainWindow.hpp
    DriveController.cpp DriveController.hpp
    ConnectionDialog.cpp ConnectionDialog.hpp
    ControlPanel.cpp ControlPanel.hpp
    DashboardPage.cpp DashboardPage.hpp
    FaultText.cpp FaultText.hpp
)
set_target_properties(focstudio PROPERTIES AUTOMOC ON)
target_compile_definitions(focstudio PRIVATE
    FOC_STUDIO_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS)
target_link_libraries(focstudio PRIVATE focstudio_core fochost qwt Qt6::Widgets)

if(FOC_BUILD_TESTS)
    add_executable(test_paramfile ${PROJECT_SOURCE_DIR}/host/tests/test_paramfile.cpp)
    target_link_libraries(test_paramfile PRIVATE focstudio_core)
    add_test(NAME paramfile COMMAND test_paramfile)
    # Qt DLLs for ctest on Windows
    if(WIN32)
        set_tests_properties(paramfile PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()
if(MSVC)
    target_compile_options(focstudio PRIVATE /W4)
else()
    target_compile_options(focstudio PRIVATE -Wall -Wextra)
endif()

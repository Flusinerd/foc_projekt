# FOC Studio - end-user application (Qt 6 Widgets). Qt-free core: fochost.
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets LinguistTools)
include(${PROJECT_SOURCE_DIR}/cmake/Qwt.cmake)

# Qt Core only parts, also used by the tests.
add_library(focstudio_core STATIC ParamFile.cpp ParamFile.hpp)
target_include_directories(focstudio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(focstudio_core PUBLIC fochost Qt6::Core)

qt_add_executable(focstudio WIN32
    main.cpp
    ParametersPage.cpp ParametersPage.hpp
    ScopeWidget.cpp ScopeWidget.hpp
    ScopePage.cpp ScopePage.hpp
    CommissioningPage.cpp CommissioningPage.hpp
    EventLog.cpp EventLog.hpp
    EventsPage.cpp EventsPage.hpp
    MainWindow.cpp MainWindow.hpp
    DriveController.cpp DriveController.hpp
    ConnectionDialog.cpp ConnectionDialog.hpp
    ControlPanel.cpp ControlPanel.hpp
    DashboardPage.cpp DashboardPage.hpp
    FaultText.cpp FaultText.hpp
    AboutDialog.cpp AboutDialog.hpp
)

# Translations: English is the source language, German in focstudio_de.ts.
# `cmake --build build --target update_translations` refreshes the .ts files (lupdate).
# Qt's own strings (standard buttons, file dialog) are merged into the .qm files.
qt_add_translations(TARGETS focstudio
    SOURCE_TARGETS focstudio focstudio_core
    TS_FILES translations/focstudio_de.ts
    PLURALS_TS_FILE translations/focstudio_en.ts
    LUPDATE_OPTIONS -no-obsolete
    MERGE_QT_TRANSLATIONS
    QT_TRANSLATION_CATALOGS qtbase
    RESOURCE_PREFIX /i18n)

qt_add_resources(focstudio docs
    PREFIX /docs
    BASE ${PROJECT_SOURCE_DIR}/docs
    FILES ${PROJECT_SOURCE_DIR}/docs/LICENSES.md)
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

# FOC Studio - end-user application (Qt 6 Widgets). Qt-free core: fochost.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
include(${PROJECT_SOURCE_DIR}/cmake/Qwt.cmake)

qt_add_executable(focstudio WIN32
    main.cpp
    MainWindow.cpp MainWindow.hpp
    DriveController.cpp DriveController.hpp
    ConnectionDialog.cpp ConnectionDialog.hpp
    DashboardPage.cpp DashboardPage.hpp
    FaultText.cpp FaultText.hpp
)
set_target_properties(focstudio PROPERTIES AUTOMOC ON)
target_compile_definitions(focstudio PRIVATE
    FOC_STUDIO_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS)
target_link_libraries(focstudio PRIVATE fochost qwt Qt6::Widgets)
if(MSVC)
    target_compile_options(focstudio PRIVATE /W4)
else()
    target_compile_options(focstudio PRIVATE -Wall -Wextra)
endif()

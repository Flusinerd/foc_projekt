# Installer for FOC Studio (CPack). Windows: NSIS setup.exe, plus a ZIP for portable use.
#   cmake --build build --config Release
#   cd build && cpack -C Release            (or: cmake --build build --target package)
# Needs NSIS (makensis) in PATH or at its default location. See docs/Installation.md.
set(CPACK_PACKAGE_NAME "FOC Studio")
set(CPACK_PACKAGE_VENDOR "Hochschule Bochum")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FOC Studio - Bedien- und Inbetriebnahmewerkzeug fuer das FOC-Framework")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "FOC Studio")
set(CPACK_PACKAGE_FILE_NAME "FOCStudio-${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_EXECUTABLES "focstudio" "FOC Studio")
set(CPACK_STRIP_FILES OFF)

# CPack wants a plain text file for the license page.
configure_file(${PROJECT_SOURCE_DIR}/docs/LICENSES.md ${PROJECT_BINARY_DIR}/LICENSES.txt COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_BINARY_DIR}/LICENSES.txt)

if(WIN32)
    set(CPACK_GENERATOR NSIS ZIP)
    if(NOT CPACK_NSIS_EXECUTABLE)
        find_program(FOC_MAKENSIS makensis PATHS "$ENV{ProgramFiles\(x86\)}/NSIS" "$ENV{ProgramFiles}/NSIS")
        if(FOC_MAKENSIS)
            set(CPACK_NSIS_EXECUTABLE ${FOC_MAKENSIS})
        endif()
    endif()
    set(CPACK_NSIS_PACKAGE_NAME "FOC Studio ${PROJECT_VERSION}")
    set(CPACK_NSIS_DISPLAY_NAME "FOC Studio")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\focstudio.exe")
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/host/studio/icons/carpediem.ico" _foc_ico)
    string(REPLACE "\\" "\\\\" _foc_ico "${_foc_ico}")
    set(CPACK_NSIS_MUI_ICON "${_foc_ico}")
    set(CPACK_NSIS_MUI_UNIICON "${_foc_ico}")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin\\\\focstudio.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_URL_INFO_ABOUT "https://www.hs-bochum.de")
    # Fixed key: a newer version replaces an older one instead of installing side by side.
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "FOC Studio")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
else()
    set(CPACK_GENERATOR TGZ)
endif()

include(CPack)

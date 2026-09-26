cmake_minimum_required(VERSION 3.24)

project(konsole-ssh-manager VERSION 0.1.0 LANGUAGES CXX)

# Qt 5 pairs with KF5 (Debian 12), Qt 6 pairs with KF6 (Debian 13).
# When QT_MAJOR_VERSION is not given, prefer Qt 6 if Qt 6 and KF6 are both available.
if(NOT DEFINED QT_MAJOR_VERSION)
    find_package(Qt6 QUIET CONFIG COMPONENTS Core)
    if(Qt6_FOUND)
        find_package(KF6Parts QUIET CONFIG)
    endif()
    if(Qt6_FOUND AND KF6Parts_FOUND)
        set(QT_MAJOR_VERSION 6)
    else()
        set(QT_MAJOR_VERSION 5)
    endif()
endif()
if(NOT QT_MAJOR_VERSION MATCHES "^[56]$")
    message(FATAL_ERROR "QT_MAJOR_VERSION must be 5 or 6, got '${QT_MAJOR_VERSION}'")
endif()
set(KF_MAJOR_VERSION ${QT_MAJOR_VERSION})

if(QT_MAJOR_VERSION EQUAL 6)
    set(QT_MIN_VERSION "6.4.0")
    set(KF_MIN_VERSION "6.0.0")
else()
    set(QT_MIN_VERSION "5.15.2")
    set(KF_MIN_VERSION "5.103.0")
endif()

find_package(ECM 5.103.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMAddTests)
include(FeatureSummary)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Core Widgets Test)
find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS Parts CoreAddons I18n)

add_definitions(
    -DTRANSLATION_DOMAIN=\"konsole-ssh-manager\"
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050F00
    -DQT_NO_KEYWORDS
)

message(STATUS "Building against Qt ${QT_MAJOR_VERSION} / KF${KF_MAJOR_VERSION}")

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Desktop entry for the application menu. It points at the installed binary
# (CMAKE_INSTALL_PREFIX, ~/.local via `make install`), so the menu keeps working
# independently of the build directory. See "docs/00 specs/15-installation.md".
set(KSSHM_DESKTOP_ID "ro.binarylogic.konsole-ssh-manager")
set(KSSHM_APP_PATH "${KDE_INSTALL_FULL_BINDIR}/konsole-ssh-manager")
# Desktop Entry Exec quoting: backslash, double quote, backtick and dollar are
# escaped inside the quoted argument, and '%' is doubled.
string(REPLACE "\\" "\\\\\\\\" KSSHM_EXEC_ESCAPED "${KSSHM_APP_PATH}")
string(REPLACE "\"" "\\\\\"" KSSHM_EXEC_ESCAPED "${KSSHM_EXEC_ESCAPED}")
string(REPLACE "`" "\\\\`" KSSHM_EXEC_ESCAPED "${KSSHM_EXEC_ESCAPED}")
string(REPLACE "$" "\\\\$" KSSHM_EXEC_ESCAPED "${KSSHM_EXEC_ESCAPED}")
string(REPLACE "%" "%%" KSSHM_EXEC_ESCAPED "${KSSHM_EXEC_ESCAPED}")
set(KSSHM_DESKTOP_EXEC "\"${KSSHM_EXEC_ESCAPED}\"")
configure_file(data/${KSSHM_DESKTOP_ID}.desktop.in ${CMAKE_BINARY_DIR}/${KSSHM_DESKTOP_ID}.desktop @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/${KSSHM_DESKTOP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# App icon (menu entry and window). Named after the desktop file ID, as the icon theme spec expects.
install(FILES data/icons/${KSSHM_DESKTOP_ID}.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

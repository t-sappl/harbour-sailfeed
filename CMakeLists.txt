# SPDX-FileCopyrightText: 2026 Thomas Sappl
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

project(harbour-sailfeed
        VERSION 0.1.0
        DESCRIPTION "Lightweight Nextcloud News client for Sailfish OS"
        HOMEPAGE_URL "https://github.com/t-sappl/harbour-sailfeed"
        LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(FindPkgConfig)

find_package(Qt5 COMPONENTS Core Gui Qml Quick Network Sql LinguistTools REQUIRED)
pkg_search_module(SAILFISH sailfishapp REQUIRED IMPORTED_TARGET)
pkg_search_module(SECRETS sailfishsecrets REQUIRED IMPORTED_TARGET)

set(SAILFEED_APPNAME "harbour-sailfeed")
set(SAILFEED_DATADIR "${CMAKE_INSTALL_DATADIR}/${SAILFEED_APPNAME}")
set(SAILFEED_I18NDIR "${SAILFEED_DATADIR}/translations")

# --- libfuoten (LGPL-3.0-or-later), linked statically --------------------
# Local copy with a small patch (see 3rdparty/README.md). Static linking keeps
# the package to a single executable without a private library path.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libfuoten as static library" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(LIBFUOTEN_I18NDIR "${SAILFEED_I18NDIR}" CACHE PATH "Install directory for libfuoten translations" FORCE)
add_subdirectory(3rdparty/libfuoten)

# --- Application ---------------------------------------------------------
add_executable(${SAILFEED_APPNAME}
    src/main.cpp
    src/configuration.h
    src/configuration.cpp
    src/accountmanager.h
    src/accountmanager.cpp
    src/articlemodel.h
    src/articlemodel.cpp
    src/htmlutils.h
    src/htmlutils.cpp
    src/htmlhelper.h
    src/htmlhelper.cpp
    src/namfactory.h
    src/namfactory.cpp
    src/secretstore.h
    src/secretstore.cpp
)

target_compile_definitions(${SAILFEED_APPNAME} PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
    QT_NO_CAST_FROM_ASCII
    QT_USE_QSTRINGBUILDER
    SAILFEED_VERSION="${PROJECT_VERSION}"
    SAILFEED_I18NDIR="${CMAKE_INSTALL_PREFIX}/${SAILFEED_I18NDIR}"
)

target_link_libraries(${SAILFEED_APPNAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Qml
    Qt5::Quick
    Qt5::Network
    Qt5::Sql
    PkgConfig::SAILFISH
    PkgConfig::SECRETS
    FuotenQt5::Core
)

install(TARGETS ${SAILFEED_APPNAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY qml DESTINATION ${SAILFEED_DATADIR})
install(FILES ${SAILFEED_APPNAME}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

foreach(_size 86x86 108x108 128x128 172x172)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/icons/${_size}/${SAILFEED_APPNAME}.png)
        install(FILES icons/${_size}/${SAILFEED_APPNAME}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}/apps)
    endif()
endforeach()

# App translations (created in Etappe 2, picked up automatically once present)
file(GLOB SAILFEED_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/${SAILFEED_APPNAME}*.ts)
if(SAILFEED_TS_FILES)
    qt5_add_translation(SAILFEED_QM_FILES ${SAILFEED_TS_FILES})
    add_custom_target(sailfeed_translations ALL DEPENDS ${SAILFEED_QM_FILES})
    install(FILES ${SAILFEED_QM_FILES} DESTINATION ${SAILFEED_I18NDIR})
endif()

# Make QML and packaging files visible in Qt Creator
file(GLOB_RECURSE SAILFEED_QML_FILES qml/*.qml)
add_custom_target(distfiles SOURCES
    ${SAILFEED_QML_FILES}
    ${SAILFEED_APPNAME}.desktop
    rpm/${SAILFEED_APPNAME}.spec
)

file(WRITE "${CMAKE_BINARY_DIR}/QtCreatorDeployment.txt"
    "${CMAKE_INSTALL_PREFIX}\n${CMAKE_BINARY_DIR}/${SAILFEED_APPNAME}:bin\n")

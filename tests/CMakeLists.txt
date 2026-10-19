# SPDX-FileCopyrightText: 2026 Thomas Sappl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Desktop backend test (not part of the RPM build). Needs Qt 5 Core/Network/Sql.
# libfuoten is compiled directly from its sources here, so Qt LinguistTools
# are not needed. See README.md in this directory.
cmake_minimum_required(VERSION 3.16)
project(sailfeed-backendtest CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5 COMPONENTS Core Network Sql REQUIRED)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LF ${SRC}/3rdparty/libfuoten)
file(GLOB_RECURSE LF_SRC ${LF}/Fuoten/*.cpp ${LF}/Fuoten/*.h)
add_library(fuoten STATIC ${LF_SRC})
include(GenerateExportHeader)
generate_export_header(fuoten BASE_NAME fuoten)
target_include_directories(fuoten PUBLIC ${LF} ${LF}/Fuoten ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(fuoten PRIVATE QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII QT_USE_QSTRINGBUILDER
    FUOTEN_VERSION="0.8.2" QT_DISABLE_DEPRECATED_BEFORE=0x050500)
target_link_libraries(fuoten PUBLIC Qt5::Core Qt5::Network Qt5::Sql)

add_executable(backendtest backendtest.cpp
    ${SRC}/src/htmlutils.cpp ${SRC}/src/articlemodel.cpp ${SRC}/src/feedmodel.cpp ${SRC}/src/feedrowsmodel.cpp ${SRC}/src/feedrowsmodel.h ${SRC}/src/feedmanager.cpp ${SRC}/src/feedmanager.h ${SRC}/src/configuration.cpp ${SRC}/src/accountmanager.cpp ${SRC}/src/secretstore.cpp ${SRC}/src/prefetcher.cpp ${SRC}/src/prefetcher.h ${SRC}/src/geckocookiejar.cpp ${SRC}/src/geckocookiejar.h ${SRC}/src/nextcloudfiles.cpp ${SRC}/src/nextcloudfiles.h ${SRC}/src/settingsbackup.cpp ${SRC}/src/settingsbackup.h ${SRC}/src/cloudbackup.cpp ${SRC}/src/cloudbackup.h ${SRC}/src/feedfinder.cpp ${SRC}/src/feedfinder.h ${SRC}/src/articlecleaner.cpp ${SRC}/src/articlecleaner.h
    ${SRC}/src/articlemodel.h ${SRC}/src/feedmodel.h ${SRC}/src/configuration.h ${SRC}/src/accountmanager.h)
target_include_directories(backendtest PRIVATE ${SRC}/src)
target_compile_definitions(backendtest PRIVATE SAILFEED_NO_SECRETS QT_NO_CAST_FROM_ASCII QT_USE_QSTRINGBUILDER SAILFEED_VERSION="0.1.0")
target_compile_options(backendtest PRIVATE -Wall -Wextra)
target_link_libraries(backendtest PRIVATE fuoten)

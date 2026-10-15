# SPDX-FileCopyrightText: 2026 Thomas Sappl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# harbour-sailfeed – Nextcloud News client for Sailfish OS (personal use).
# Open this file in the Sailfish IDE (Qt Creator of the Sailfish SDK).

TARGET = harbour-sailfeed
VERSION = 1.1.0

CONFIG += sailfishapp c++14
QT += network sql qml quick

# App password storage (Sailfish Secrets).
# Do NOT add "CONFIG += link_pkgconfig" here: sailfishapp.prf adds it itself
# and only then evaluates PKGCONFIG. Adding it here loads link_pkgconfig too
# early, and sailfishapp's own library (-lsailfishapp) is never linked.
PKGCONFIG += sailfishsecrets

# libfuoten (local copy, compiled into the app)
include(3rdparty/libfuoten.pri)

DEFINES += \
    QT_NO_CAST_FROM_ASCII \
    QT_USE_QSTRINGBUILDER \
    SAILFEED_VERSION=\\\"$$VERSION\\\" \
    SAILFEED_I18NDIR=\\\"/usr/share/$${TARGET}/translations\\\"

SOURCES += \
    src/main.cpp \
    src/accountmanager.cpp \
    src/articlemodel.cpp \
    src/cloudbackup.cpp \
    src/configuration.cpp \
    src/feedmanager.cpp \
    src/feedfinder.cpp \
    src/feedmodel.cpp \
    src/feedrowsmodel.cpp \
    src/geckocookiejar.cpp \
    src/htmlhelper.cpp \
    src/htmlutils.cpp \
    src/namfactory.cpp \
    src/nextcloudfiles.cpp \
    src/prefetcher.cpp \
    src/secretstore.cpp \
    src/settingsbackup.cpp

HEADERS += \
    src/accountmanager.h \
    src/articlemodel.h \
    src/cloudbackup.h \
    src/configuration.h \
    src/feedmanager.h \
    src/feedfinder.h \
    src/feedmodel.h \
    src/feedrowsmodel.h \
    src/geckocookiejar.h \
    src/htmlhelper.h \
    src/htmlutils.h \
    src/namfactory.h \
    src/nextcloudfiles.h \
    src/prefetcher.h \
    src/secretstore.h \
    src/settingsbackup.h

# Mozilla Readability (Apache-2.0) for the reader mode
RESOURCES += harbour-sailfeed.qrc

DISTFILES += \
    qml/harbour-sailfeed.qml \
    qml/cover/CoverPage.qml \
    qml/pages/*.qml \
    qml/components/*.qml \
    rpm/harbour-sailfeed.spec \
    harbour-sailfeed.desktop \
    README.md TODO.md DESIGN.md TEST-PROTOCOL.md PRIVACY.md HANDOFF.md

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# App translations: lupdate creates/updates the .ts files during the build
CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/harbour-sailfeed-de.ts

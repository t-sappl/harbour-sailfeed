# SPDX-FileCopyrightText: 2026 Thomas Sappl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compiles the local copy of libfuoten (LGPL-3.0-or-later, see README.md in this
# directory) directly into the application. libfuoten's own libfuoten.pro is
# outdated (it lacks the generated fuoten_export.h), so it is not used.

LIBFUOTEN_DIR = $$PWD/libfuoten
LIBFUOTEN_VERSION = 0.8.2

INCLUDEPATH += $$LIBFUOTEN_DIR $$LIBFUOTEN_DIR/Fuoten $$PWD/libfuoten-qmake
DEPENDPATH  += $$LIBFUOTEN_DIR

QT += network sql

DEFINES += FUOTEN_VERSION=\\\"$$LIBFUOTEN_VERSION\\\"

HEADERS += $$files($$LIBFUOTEN_DIR/Fuoten/*.h, true) \
           $$PWD/libfuoten-qmake/fuoten_export.h
SOURCES += $$files($$LIBFUOTEN_DIR/Fuoten/*.cpp, true)

# --- libfuoten translations (id based) -> /usr/share/<app>/translations ----
LIBFUOTEN_TS = $$files($$LIBFUOTEN_DIR/translations/libfuoten*.ts)

isEmpty(QMAKE_LRELEASE) {
    QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    !exists($$QMAKE_LRELEASE): QMAKE_LRELEASE = lrelease
}

fuoten_lrelease.input = LIBFUOTEN_TS
fuoten_lrelease.output = $$OUT_PWD/fuoten-translations/${QMAKE_FILE_BASE}.qm
fuoten_lrelease.commands = $$QMAKE_LRELEASE -idbased -silent ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
fuoten_lrelease.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += fuoten_lrelease

fuoten_qm.path = /usr/share/$${TARGET}/translations
for(ts, LIBFUOTEN_TS) {
    fuoten_qm.files += $$OUT_PWD/fuoten-translations/$$basename(ts)
}
fuoten_qm.files = $$replace(fuoten_qm.files, \\.ts$, .qm)
fuoten_qm.CONFIG += no_check_exist
INSTALLS += fuoten_qm

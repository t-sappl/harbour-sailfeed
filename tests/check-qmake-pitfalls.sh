#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Catches two errors that only the qmake/SDK build shows, not the desktop
# CMake test build:
#  1. a source file named like one of libfuoten's (qmake builds all objects
#     into one folder, the two .o files overwrite each other)
#  2. a header that does not compile on its own in its moc file (qmake
#     compiles every moc_*.cpp separately; CMake's automoc bundles them)
# Usage: QTDIR=/path/to/qt5 tests/check-qmake-pitfalls.sh   (from the project root)
set -u
QTDIR=${QTDIR:-/home/claude/qt/inst}
TMP=$(mktemp -d)
status=0

clash=$(comm -12 <(ls src/*.cpp | xargs -n1 basename | sort) \
                 <(find 3rdparty/libfuoten -name '*.cpp' | xargs -n1 basename | sort))
if [ -n "$clash" ]; then
    echo "File names also used by libfuoten: $clash"
    status=1
fi

INC="-I$QTDIR/include -I$QTDIR/include/QtCore -I$QTDIR/include/QtNetwork -I$QTDIR/include/QtSql -I$QTDIR/include/QtQml -I3rdparty/libfuoten -I3rdparty/libfuoten/Fuoten -I3rdparty/libfuoten-qmake -Isrc"
for h in src/*.h; do
    b=$(basename "$h" .h)
    "$QTDIR/bin/moc" $INC "$h" -o "$TMP/moc_$b.cpp" 2>/dev/null
    [ -s "$TMP/moc_$b.cpp" ] || continue          # no QObject in it
    if ! g++ -std=c++14 -fPIC -fsyntax-only $INC "$TMP/moc_$b.cpp" 2>"$TMP/err"; then
        echo "moc_$b.cpp does not compile on its own:"
        grep -m3 error "$TMP/err"
        status=1
    fi
done
rm -rf "$TMP"
[ $status -eq 0 ] && echo "ok"
exit $status

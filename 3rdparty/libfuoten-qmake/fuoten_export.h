// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Replacement for the header that libfuoten's CMake build generates with
// generate_export_header(). In the qmake build libfuoten is compiled directly
// into the app, so no symbol export/visibility handling is needed.
#ifndef FUOTEN_EXPORT_H
#define FUOTEN_EXPORT_H

#define FUOTEN_EXPORT
#define FUOTEN_NO_EXPORT
#define FUOTEN_DEPRECATED __attribute__ ((__deprecated__))
#define FUOTEN_DEPRECATED_EXPORT FUOTEN_DEPRECATED
#define FUOTEN_DEPRECATED_NO_EXPORT FUOTEN_DEPRECATED

#endif // FUOTEN_EXPORT_H

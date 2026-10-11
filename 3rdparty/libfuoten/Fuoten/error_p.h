/*
 * SPDX-FileCopyrightText: (C) 2016-2022 Matthias Fehring <https://www.huessenbergnetz.de>
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * Modified 2026 by Thomas Sappl for harbour-sailfeed, see 3rdparty/README.md
 * of harbour-sailfeed and patches-libfuoten.diff.
 */

#ifndef FUOTENERROR_P_H
#define FUOTENERROR_P_H

#include "error.h"
#include <QMetaEnum>
#include <QMetaObject>

namespace Fuoten {

class ErrorPrivate
{
public:
    QString text;
    QString data;
    Error::Type type = Error::NoError;
    Error::Severity severity = Error::Nothing;
    int httpStatus = 0;     // sailfeed patch
    int networkError = 0;   // sailfeed patch: QNetworkReply::NetworkError

    void printOut()
    {
        switch(severity) {
        case Error::Warning:
            qWarning("%s: %s", Error::staticMetaObject.enumerator(Error::staticMetaObject.indexOfEnumerator("Type")).valueToKey(type), qUtf8Printable(text));
            break;
        case Error::Critical:
            qCritical("%s: %s", Error::staticMetaObject.enumerator(Error::staticMetaObject.indexOfEnumerator("Type")).valueToKey(type), qUtf8Printable(text));
            break;
        case Error::Fatal:
            qFatal("%s: %s", Error::staticMetaObject.enumerator(Error::staticMetaObject.indexOfEnumerator("Type")).valueToKey(type), qUtf8Printable(text));
        default:
            break;
        }

#ifndef QT_NO_DEBUG_OUTPUT
        if (!data.isEmpty()) {
            qDebug("Error data: %s", qUtf8Printable(data));
        }
#endif
    }

};

}

#endif // FUOTENERROR_P_H

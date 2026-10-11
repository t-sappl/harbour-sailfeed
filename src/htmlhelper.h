// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QVariantMap>

// QML singleton "Html" exposing the HtmlUtils functions and the reader mode.
class HtmlHelper : public QObject
{
    Q_OBJECT
public:
    explicit HtmlHelper(QObject *parent = nullptr);

    Q_INVOKABLE QString cleanBody(const QString &html) const;
    Q_INVOKABLE QString excerpt(const QString &html, int maxChars = 220) const;

    // Script for WebView.runJavaScript(): runs Mozilla Readability on the
    // loaded page and returns the article as JSON string, or "" if the page
    // has no readable article.
    Q_INVOKABLE QString readerExtractScript() const;

    // Builds the reader page from the JSON returned by readerExtractScript().
    // style keys (all optional): background, text, secondary, link (CSS
    // colors), fontPx (int). Returns "" on invalid input.
    Q_INVOKABLE QString readerHtml(const QString &articleJson, const QVariantMap &style) const;
    // "Always open in the original" sites (see HtmlUtils::siteKey/isListedSite)
    Q_INVOKABLE QString siteKey(const QString &url) const;
    Q_INVOKABLE bool isListedSite(const QString &url, const QStringList &sites) const;
};

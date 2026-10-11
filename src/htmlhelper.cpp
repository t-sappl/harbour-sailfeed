// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "htmlhelper.h"
#include "htmlutils.h"

#include <QFile>

HtmlHelper::HtmlHelper(QObject *parent) : QObject(parent) {}

QString HtmlHelper::cleanBody(const QString &html) const
{
    return HtmlUtils::cleanBody(html);
}

QString HtmlHelper::excerpt(const QString &html, int maxChars) const
{
    return HtmlUtils::plainExcerpt(html, maxChars);
}

QString HtmlHelper::siteKey(const QString &url) const
{
    return HtmlUtils::siteKey(url);
}

bool HtmlHelper::isListedSite(const QString &url, const QStringList &sites) const
{
    return HtmlUtils::isListedSite(url, sites);
}

QString HtmlHelper::readerExtractScript() const
{
    static QString script;
    if (script.isEmpty()) {
        QFile f(QStringLiteral(":/js/Readability.js"));
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning("Reader mode: Readability.js missing from resources");
            return QString();
        }
        // runJavaScript() runs the script as a function body and needs an
        // explicit return; only basic types can be returned, hence JSON.
        script = QString::fromUtf8(f.readAll()) + QStringLiteral(
            "\n;return (function () {\n"
            "  try {\n"
            // Pre-filter: drop fixed/sticky overlays (cookie and consent banners,
            // sticky ads) and elements whose class/id marks them as ads or consent.
            "    var drop = /^(ad|ads|adv|advert|advertisement|anzeige|anzeigen|werbung|sponsor|sponsored|promo|cookie|cookies|consent|gdpr|cmp|newsletter|outbrain|taboola)$/i;\n"
            "    var keep = { HTML: 1, BODY: 1, MAIN: 1, ARTICLE: 1 };\n"
            "    var all = document.body ? document.body.getElementsByTagName('*') : [];\n"
            "    for (var i = 0; i < all.length; i++) {\n"
            "      var el = all[i];\n"
            "      if (keep[el.tagName]) continue;\n"
            "      var pos = window.getComputedStyle(el).position;\n"
            "      var tokens = ((typeof el.className === 'string' ? el.className : '') + ' ' + (el.id || '')).split(/[\\s_-]+/);\n"
            "      var bad = pos === 'fixed' || pos === 'sticky';\n"
            "      for (var t = 0; !bad && t < tokens.length; t++) bad = drop.test(tokens[t]);\n"
            "      if (bad) el.setAttribute('data-sailfeed-drop', '1');\n"
            "    }\n"
            "    var doc = document.cloneNode(true);\n"
            "    var marked = document.querySelectorAll('[data-sailfeed-drop]');\n"
            "    for (var m = 0; m < marked.length; m++) marked[m].removeAttribute('data-sailfeed-drop');\n"
            "    var gone = doc.querySelectorAll('[data-sailfeed-drop]');\n"
            "    for (var g = 0; g < gone.length; g++) if (gone[g].parentNode) gone[g].parentNode.removeChild(gone[g]);\n"
            "    var a = new Readability(doc).parse();\n"
            // Consent wall ("accept ads and tracking or subscribe", e.g. golem.de):
            // a short text with several consent words. Its buttons get lost in the
            // reader page, so QML shows the original page instead.
            "    var text = a && a.textContent ? a.textContent : (document.body ? document.body.innerText || '' : '');\n"
            "    if (text.length < 2000) {\n"
            "      var words = ['zustimm', 'einwillig', 'cookie', 'consent', 'tracking', 'werbung', 'abonn', 'drittanbieter',\n"
            "                   'datenschutz', 'akzeptier', 'accept', 'subscri', 'third part', 'personali'];\n"
            "      var low = text.toLowerCase(), hits = 0;\n"
            "      for (var w = 0; w < words.length; w++) if (low.indexOf(words[w]) >= 0) hits++;\n"
            "      if (hits >= 4) return JSON.stringify({ consentWall: true });\n"
            "    }\n"
            "    if (!a || !a.content || (a.textContent || '').trim().length < 200) return '';\n"
            "    return JSON.stringify({ title: a.title || document.title || '', byline: a.byline || '',\n"
            "      siteName: a.siteName || location.hostname, content: a.content,\n"
            "      lang: a.lang || document.documentElement.lang || '', dir: a.dir || '' });\n"
            "  } catch (e) { return ''; }\n"
            "})();\n");
    }
    return script;
}

QString HtmlHelper::readerHtml(const QString &articleJson, const QVariantMap &style) const
{
    HtmlUtils::ReaderStyle s;
    auto str = [&style](const char *key, QString &target) {
        const QString v = style.value(QString::fromLatin1(key)).toString();
        // Only accept simple CSS color syntax (#hex, rgb()/rgba()).
        if (!v.isEmpty() && !v.contains(QLatin1Char(';')) && !v.contains(QLatin1Char('}'))
                && !v.contains(QLatin1Char('<')))
            target = v;
    };
    str("background", s.background);
    str("text", s.text);
    str("secondary", s.secondary);
    str("link", s.link);
    const int px = style.value(QStringLiteral("fontPx")).toInt();
    if (px >= 10 && px <= 40)
        s.fontPx = px;
    s.serif = style.value(QStringLiteral("serif")).toBool();
    if (style.contains(QStringLiteral("lineHeight")))
        s.lineHeight = style.value(QStringLiteral("lineHeight")).toDouble();
    if (style.contains(QStringLiteral("margin")))
        s.margin = style.value(QStringLiteral("margin")).toInt();
    s.justify = style.value(QStringLiteral("justify")).toBool();
    if (style.contains(QStringLiteral("images")))
        s.images = style.value(QStringLiteral("images")).toBool();
    return HtmlUtils::readerHtml(articleJson, s);
}

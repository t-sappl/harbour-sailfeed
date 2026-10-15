// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
#include "htmlutils.h"

#include <QHash>
#include <QJsonDocument>
#include <QStringList>
#include <QUrl>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

QUrl resolve(const QString &raw, const QUrl &base)
{
    const QString src = HtmlUtils::decodeEntities(raw.trimmed());
    if (src.isEmpty() || src.startsWith(QLatin1String("data:"), Qt::CaseInsensitive))
        return QUrl();

    QUrl url(src);
    if (!url.isValid())
        return QUrl();
    if (url.isRelative()) {
        if (!base.isValid())
            return QUrl();
        url = base.resolved(url);       // also handles "//host/path"
    }
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return QUrl();
    return url;
}

// Returns true for 1x1 tracking pixels and similar non-images.
bool isTrackingPixel(const QString &imgTag)
{
    static const QRegularExpression sizeRe(
        QStringLiteral("\\b(?:width|height)\\s*=\\s*[\"']?\\s*([0-9]+)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = sizeRe.globalMatch(imgTag);
    while (it.hasNext()) {
        if (it.next().captured(1).toInt() <= 2)
            return true;
    }
    static const QRegularExpression trackerRe(
        QStringLiteral("feedburner\\.com/~r/|feeds\\.feedburner|/pixel\\.|/tracking/|vg[0-9]+\\.met\\.vgwort\\.de|"
                       "doubleclick\\.net|/count\\.gif|/beacon"),
        QRegularExpression::CaseInsensitiveOption);
    return trackerRe.match(imgTag).hasMatch();
}

} // namespace

namespace HtmlUtils {

QUrl thumbnail(const QUrl &enclosureLink, const QString &enclosureMime,
               const QString &body, const QUrl &mediaThumbnail,
               const QUrl &articleUrl)
{
    // 1. Image enclosure
    if (!enclosureLink.isEmpty() && enclosureMime.startsWith(QLatin1String("image/"), Qt::CaseInsensitive)) {
        const QUrl u = resolve(enclosureLink.toString(), articleUrl);
        if (u.isValid())
            return u;
    }

    // 2. First usable <img src> in the body
    static const QRegularExpression imgRe(
        QStringLiteral("<img\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression srcRe(
        QStringLiteral("\\bsrc\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = imgRe.globalMatch(body);
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        if (isTrackingPixel(tag))
            continue;
        const QRegularExpressionMatch m = srcRe.match(tag);
        if (!m.hasMatch())
            continue;
        QString src = m.captured(1);
        if (src.isEmpty()) src = m.captured(2);
        if (src.isEmpty()) src = m.captured(3);
        const QUrl u = resolve(src, articleUrl);
        if (u.isValid())
            return u;
    }

    // 3. media:thumbnail from the feed
    if (!mediaThumbnail.isEmpty()) {
        const QUrl u = resolve(mediaThumbnail.toString(), articleUrl);
        if (u.isValid())
            return u;
    }

    return QUrl();
}

QString decodeEntities(const QString &text)
{
    if (!text.contains(QLatin1Char('&')))
        return text;

    static const QHash<QString, QChar> named = {
        { QStringLiteral("amp"),    QLatin1Char('&') },
        { QStringLiteral("lt"),     QLatin1Char('<') },
        { QStringLiteral("gt"),     QLatin1Char('>') },
        { QStringLiteral("quot"),   QLatin1Char('"') },
        { QStringLiteral("apos"),   QLatin1Char('\'') },
        { QStringLiteral("nbsp"),   QChar(0x00A0) },
        { QStringLiteral("shy"),    QChar(0x00AD) },
        { QStringLiteral("ndash"),  QChar(0x2013) },
        { QStringLiteral("mdash"),  QChar(0x2014) },
        { QStringLiteral("lsquo"),  QChar(0x2018) },
        { QStringLiteral("rsquo"),  QChar(0x2019) },
        { QStringLiteral("sbquo"),  QChar(0x201A) },
        { QStringLiteral("ldquo"),  QChar(0x201C) },
        { QStringLiteral("rdquo"),  QChar(0x201D) },
        { QStringLiteral("bdquo"),  QChar(0x201E) },
        { QStringLiteral("hellip"), QChar(0x2026) },
        { QStringLiteral("euro"),   QChar(0x20AC) },
        { QStringLiteral("copy"),   QChar(0x00A9) },
        { QStringLiteral("reg"),    QChar(0x00AE) },
        { QStringLiteral("trade"),  QChar(0x2122) },
        { QStringLiteral("auml"),   QChar(0x00E4) },
        { QStringLiteral("ouml"),   QChar(0x00F6) },
        { QStringLiteral("uuml"),   QChar(0x00FC) },
        { QStringLiteral("Auml"),   QChar(0x00C4) },
        { QStringLiteral("Ouml"),   QChar(0x00D6) },
        { QStringLiteral("Uuml"),   QChar(0x00DC) },
        { QStringLiteral("szlig"),  QChar(0x00DF) },
        { QStringLiteral("eacute"), QChar(0x00E9) },
        { QStringLiteral("egrave"), QChar(0x00E8) },
        { QStringLiteral("aacute"), QChar(0x00E1) },
        { QStringLiteral("agrave"), QChar(0x00E0) },
        { QStringLiteral("ccedil"), QChar(0x00E7) },
        { QStringLiteral("laquo"),  QChar(0x00AB) },
        { QStringLiteral("raquo"),  QChar(0x00BB) },
        { QStringLiteral("middot"), QChar(0x00B7) },
        { QStringLiteral("bull"),   QChar(0x2022) },
    };

    static const QRegularExpression entRe(QStringLiteral("&(#[0-9]{1,7}|#[xX][0-9a-fA-F]{1,6}|[a-zA-Z]{2,8});"));

    QString out;
    out.reserve(text.size());
    int last = 0;
    QRegularExpressionMatchIterator it = entRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += text.midRef(last, m.capturedStart() - last);
        const QString ent = m.captured(1);
        bool replaced = false;
        if (ent.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint code = (ent.size() > 1 && (ent.at(1) == QLatin1Char('x') || ent.at(1) == QLatin1Char('X')))
                    ? ent.mid(2).toUInt(&ok, 16) : ent.mid(1).toUInt(&ok, 10);
            if (ok && code > 0 && code <= 0x10FFFF) {
                if (QChar::requiresSurrogates(code)) {
                    out += QChar(QChar::highSurrogate(code));
                    out += QChar(QChar::lowSurrogate(code));
                } else {
                    out += QChar(static_cast<ushort>(code));
                }
                replaced = true;
            }
        } else {
            const auto n = named.constFind(ent);
            if (n != named.constEnd()) {
                out += n.value();
                replaced = true;
            }
        }
        if (!replaced)
            out += m.captured(0);
        last = m.capturedEnd();
    }
    out += text.midRef(last);
    return out;
}

QString plainExcerpt(const QString &html, int maxChars)
{
    static const QRegularExpression blockRe(
        QStringLiteral("<(script|style|figcaption|iframe|noscript)\\b[^>]*>.*?</\\1\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));

    QString text = html;
    text.remove(blockRe);
    text.replace(tagRe, QStringLiteral(" "));
    text = decodeEntities(text);
    text.replace(QChar(0x00A0), QLatin1Char(' '));
    text.remove(QChar(0x00AD));
    text.replace(wsRe, QStringLiteral(" "));
    text = text.trimmed();

    if (maxChars > 0 && text.size() > maxChars) {
        int cut = text.lastIndexOf(QLatin1Char(' '), maxChars);
        if (cut < maxChars * 2 / 3)
            cut = maxChars;
        text = text.left(cut).trimmed() + QChar(0x2026);
    }
    return text;
}

QString cleanBody(const QString &html)
{
    static const QRegularExpression blockRe(
        QStringLiteral("<(script|style|iframe|object|embed|form|noscript|svg|video|audio)\\b[^>]*>.*?</\\1\\s*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression singleRe(
        QStringLiteral("<(img|source|picture|/picture|input|button|/button|link|meta)\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    // Wrappers left empty once the images are gone (e.g. The Next Web:
    // <p><a href=…><img …></a></p>): RichText still draws their paragraph
    // margins, a large gap below the lead image's place (device test 15 Oct).
    // Removed from the inside out until nothing changes.
    static const QRegularExpression emptyWrapRe(
        QStringLiteral("<(p|div|figure|figcaption|span|a|center|strong|em|b|i)\\b[^>]*>"
                       "(?:\\s|&nbsp;|&#160;|<br\\s*/?>)*</\\1\\s*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression leadingBreakRe(
        QStringLiteral("^(?:\\s|&nbsp;|&#160;|<br\\s*/?>)+"),
        QRegularExpression::CaseInsensitiveOption);

    QString out = html;
    out.remove(blockRe);
    out.remove(singleRe);
    for (int i = 0; i < 8; ++i) {
        const int before = out.size();
        out.remove(emptyWrapRe);
        if (out.size() == before)
            break;
    }
    out.remove(leadingBreakRe);
    return out.trimmed();
}


QString escapeHtml(const QString &text)
{
    QString out;
    out.reserve(text.size() + 16);
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&': out += QLatin1String("&amp;"); break;
        case '<': out += QLatin1String("&lt;"); break;
        case '>': out += QLatin1String("&gt;"); break;
        case '"': out += QLatin1String("&quot;"); break;
        case '\'': out += QLatin1String("&#39;"); break;
        default: out += c;
        }
    }
    return out;
}

QString siteKey(const QString &url)
{
    const QUrl u(url.trimmed().contains(QLatin1String("://")) ? url.trimmed()
                                                               : QStringLiteral("https://") + url.trimmed());
    QString host = u.host().toLower();
    if (host.startsWith(QLatin1String("www.")))
        host = host.mid(4);
    return host;
}

bool isListedSite(const QString &url, const QStringList &sites)
{
    const QString host = siteKey(url);
    if (host.isEmpty())
        return false;
    for (const QString &s : sites) {
        const QString site = siteKey(s);
        if (!site.isEmpty() && (host == site || host.endsWith(QLatin1Char('.') + site)))
            return true;
    }
    return false;
}

// Links in the reader page: a tap or a long press is reported to QML
// (WebPage.qml) as "sailfeed:tap:<id>:<url>" or "sailfeed:long:<id>:<url>"
// (id = random document key + counter, so every message is unique). Two
// channels, QML drops duplicates by id:
//   - the document title (restored after 0.4 s): immediate
//   - the attribute data-sf-links of <html> (one message per line), which
//     WebPage polls and clears while the page is shown – the title alone was
//     not reliable (device test 13 Oct evening: links in Vermaden articles
//     did nothing, those of the Linux Mobile weekly updates worked).
// QML opens the link in the app or in the browser (setting; long press the
// other way round). The web view's own long-press menu is suppressed for
// links. Same-page anchors ("#…") keep their normal behaviour. Links with
// target="_blank" did nothing before (device test 13 Oct morning), they are
// handled the same way now.
static QString readerLinkScript()
{
    return QStringLiteral(
        "<script>(function(){"
        "var n=0,doc=Math.random().toString(36).slice(2,10)||'x',timer=null,sx=0,sy=0,longAt=0,"
        "title=document.title,restore=null;"
        "function send(kind,href){n++;var msg='sailfeed:'+kind+':'+doc+'-'+n+':'+href;"
        "var root=document.documentElement,q=root.getAttribute('data-sf-links');"
        "root.setAttribute('data-sf-links',q?q+'\\n'+msg:msg);"
        "document.title=msg;clearTimeout(restore);restore=setTimeout(function(){document.title=title;},400);}"
        "function linkOf(el){while(el&&!(el.tagName&&el.tagName.toUpperCase()==='A'))el=el.parentNode;"
        "if(!el||!el.href)return null;var h=el.getAttribute('href')||'';return h.charAt(0)==='#'?null:el;}"
        "function stop(){if(timer){clearTimeout(timer);timer=null;}}"
        "document.addEventListener('click',function(e){var a=linkOf(e.target);if(!a)return;"
        "e.preventDefault();e.stopPropagation();if(Date.now()-longAt<1000)return;send('tap',a.href);},true);"
        "document.addEventListener('touchstart',function(e){stop();var a=linkOf(e.target);"
        "if(!a||!e.touches.length)return;sx=e.touches[0].clientX;sy=e.touches[0].clientY;"
        "timer=setTimeout(function(){timer=null;longAt=Date.now();send('long',a.href);},600);},{passive:true});"
        "document.addEventListener('touchmove',function(e){if(!timer||!e.touches.length)return;"
        "if(Math.abs(e.touches[0].clientX-sx)>12||Math.abs(e.touches[0].clientY-sy)>12)stop();},{passive:true});"
        "document.addEventListener('touchend',stop,{passive:true});"
        "document.addEventListener('touchcancel',stop,{passive:true});"
        "document.addEventListener('contextmenu',function(e){var a=linkOf(e.target);if(!a)return;"
        "e.preventDefault();e.stopPropagation();stop();if(Date.now()-longAt>=1000){longAt=Date.now();send('long',a.href);}},true);"
        "})();</script>");
}

QString readerHtml(const QString &articleJson, const ReaderStyle &style)
{
    const QJsonDocument doc = QJsonDocument::fromJson(articleJson.toUtf8());
    if (!doc.isObject())
        return QString();
    const QJsonObject o = doc.object();
    const QString content = o.value(QStringLiteral("content")).toString();
    if (content.trimmed().isEmpty())
        return QString();

    const QString title = o.value(QStringLiteral("title")).toString();
    const QString byline = o.value(QStringLiteral("byline")).toString();
    const QString site = o.value(QStringLiteral("siteName")).toString();
    QString lang = o.value(QStringLiteral("lang")).toString();
    if (lang.isEmpty() || lang.contains(QLatin1Char('"')))
        lang = QStringLiteral("en");
    const QString dir = o.value(QStringLiteral("dir")).toString() == QLatin1String("rtl")
            ? QStringLiteral("rtl") : QStringLiteral("ltr");

    // Content comes from Readability, which removes scripts and event
    // handlers; it is inserted as HTML on purpose.
    const QString css = QStringLiteral(
        "html{background:%1;}"
        "body{margin:0;background:%1;color:%2;font-family:%6;"
        "font-size:%5px;line-height:%7;-moz-text-size-adjust:none;overflow-wrap:break-word;}"
        "main{padding:1.2em %8em 3em %8em;max-width:42em;margin:0 auto;}"
        "article p,article li{text-align:%9;-moz-hyphens:%10;hyphens:%10;}"
        ".site{font-size:.8em;color:%3;margin-bottom:.4em;}"
        "h1{font-size:1.45em;line-height:1.2;margin:0 0 .4em 0;color:%4;}"
        ".byline{font-size:.85em;color:%3;margin-bottom:1.2em;}"
        "a{color:%4;}"
        "img,video,svg{max-width:100%;height:auto;}"
        "figure{margin:1em 0;}figcaption,small{font-size:.8em;color:%3;}"
        "blockquote{margin:1em 0;padding-left:1em;border-left:3px solid %3;color:%3;}"
        "pre{overflow-x:auto;white-space:pre-wrap;}code{font-size:.9em;}"
        "table{display:block;overflow-x:auto;border-collapse:collapse;}td,th{padding:.2em .4em;}"
        "iframe,object,embed{display:none;}"
        "h2,h3,h4{line-height:1.25;}")
        .arg(style.background, style.text, style.secondary, style.link)
        .arg(style.fontPx)
        .arg(style.serif ? QStringLiteral("'Noto Serif','DejaVu Serif','Liberation Serif',Georgia,serif")
                         : QStringLiteral("'Sail Sans Pro','Source Sans Pro',sans-serif"))
        .arg(QString::number(qBound(1.2, style.lineHeight, 2.2), 'f', 2))
        .arg(style.margin <= 0 ? QStringLiteral("0.6") : (style.margin >= 2 ? QStringLiteral("1.6") : QStringLiteral("1")))
        .arg(style.justify ? QStringLiteral("justify") : QStringLiteral("start"))
        .arg(style.justify ? QStringLiteral("auto") : QStringLiteral("manual"))
        + (style.images ? QString()
                        : QStringLiteral("img,picture,video,svg,figure>img{display:none !important;}"));

    QString html;
    html.reserve(content.size() + 4096);
    html += QStringLiteral("<!DOCTYPE html><html lang=\"") + escapeHtml(lang)
         + QStringLiteral("\" dir=\"") + dir + QStringLiteral("\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<meta name=\"sailfeed-reader\" content=\"1\">"
           "<title>") + escapeHtml(title) + QStringLiteral("</title><style>") + css
         + QStringLiteral("</style></head><body><main>");
    if (!site.isEmpty())
        html += QStringLiteral("<div class=\"site\">") + escapeHtml(site) + QStringLiteral("</div>");
    if (!title.isEmpty())
        html += QStringLiteral("<h1>") + escapeHtml(title) + QStringLiteral("</h1>");
    if (!byline.isEmpty())
        html += QStringLiteral("<div class=\"byline\">") + escapeHtml(byline) + QStringLiteral("</div>");
    html += QStringLiteral("<article>") + content + QStringLiteral("</article></main>")
         + readerLinkScript() + QStringLiteral("</body></html>");
    return html;
}

} // namespace HtmlUtils

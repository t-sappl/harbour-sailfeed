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


// --- leanPage() helpers ---------------------------------------------------

struct Attr {
    QString name;       // lower case
    QString raw;        // as written, without the leading whitespace
    QString value;      // raw value (entities not decoded)
    bool hasValue = false;
};

bool isAsciiLetter(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
}

// Index after the end of the tag that starts at i ('<'), quote aware; -1 if
// the tag does not end.
int tagEnd(const QString &s, int i)
{
    QChar quote;
    for (int j = i + 1; j < s.size(); ++j) {
        const QChar c = s.at(j);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            // only a quote right after '=' (spaces allowed) starts a value
            int k = j - 1;
            while (k > i && s.at(k).isSpace())
                --k;
            if (s.at(k) == QLatin1Char('='))
                quote = c;
        } else if (c == QLatin1Char('>')) {
            return j + 1;
        }
    }
    return -1;
}

// Splits the inside of a start tag ("img src=… alt") into name and attributes.
void parseTag(const QString &tag, QString *name, QList<Attr> *attrs, bool *selfClosing)
{
    // tag: "<name ...>" complete
    int i = 1;
    const int end = tag.size() - 1;     // the '>'
    while (i < end && !tag.at(i).isSpace() && tag.at(i) != QLatin1Char('/') && tag.at(i) != QLatin1Char('>'))
        ++i;
    *name = tag.mid(1, i - 1).toLower();
    *selfClosing = end > 0 && tag.at(end - 1) == QLatin1Char('/');
    while (i < end) {
        while (i < end && (tag.at(i).isSpace() || tag.at(i) == QLatin1Char('/')))
            ++i;
        if (i >= end)
            break;
        const int start = i;
        while (i < end && !tag.at(i).isSpace() && tag.at(i) != QLatin1Char('=')
               && tag.at(i) != QLatin1Char('>') && !(tag.at(i) == QLatin1Char('/') && i + 1 == end))
            ++i;
        Attr a;
        a.name = tag.mid(start, i - start).toLower();
        int k = i;
        while (k < end && tag.at(k).isSpace())
            ++k;
        if (k < end && tag.at(k) == QLatin1Char('=')) {
            ++k;
            while (k < end && tag.at(k).isSpace())
                ++k;
            a.hasValue = true;
            if (k < end && (tag.at(k) == QLatin1Char('"') || tag.at(k) == QLatin1Char('\''))) {
                const QChar q = tag.at(k);
                const int close = tag.indexOf(q, k + 1);
                const int stop = close < 0 || close > end ? end : close;
                a.value = tag.mid(k + 1, stop - k - 1);
                i = stop < end ? stop + 1 : end;
            } else {
                const int vstart = k;
                while (k < end && !tag.at(k).isSpace() && !(tag.at(k) == QLatin1Char('/') && k + 1 == end))
                    ++k;
                a.value = tag.mid(vstart, k - vstart);
                i = k;
            }
        }
        a.raw = tag.mid(start, i - start);
        if (!a.name.isEmpty())
            attrs->append(a);
        if (i == start)     // no progress: skip one character
            ++i;
    }
}

// Absolute address for an attribute value (entities decoded first);
// absolute and data: values are returned unchanged.
QString absolute(const QString &value, const QUrl &base)
{
    const QString v = value.trimmed();
    if (v.isEmpty() || v.startsWith(QLatin1String("data:"), Qt::CaseInsensitive)
            || v.startsWith(QLatin1Char('#')))
        return value;
    static const QRegularExpression schemeRe(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    if (schemeRe.match(v).hasMatch() || !base.isValid())
        return value;
    const QUrl rel(HtmlUtils::decodeEntities(v));
    if (!rel.isValid())
        return value;
    return HtmlUtils::escapeHtml(base.resolved(rel).toString(QUrl::FullyEncoded));
}

// srcset: "a.jpg 1x, b.jpg 2x" – every address made absolute
QString absoluteSrcset(const QString &value, const QUrl &base)
{
    QString out;
    int i = 0;
    const int n = value.size();
    while (i < n) {
        const int start = i;
        while (i < n && (value.at(i).isSpace() || value.at(i) == QLatin1Char(',')))
            ++i;
        out += value.mid(start, i - start);
        if (i >= n)
            break;
        const int ustart = i;
        while (i < n && !value.at(i).isSpace())
            ++i;
        QString url = value.mid(ustart, i - ustart);
        QString trailing;
        while (url.endsWith(QLatin1Char(','))) {     // "a.jpg," without a descriptor
            url.chop(1);
            trailing += QLatin1Char(',');
        }
        out += absolute(url, base) + trailing;
        if (!trailing.isEmpty())
            continue;
        const int dstart = i;
        int depth = 0;
        while (i < n && (depth > 0 || value.at(i) != QLatin1Char(','))) {
            if (value.at(i) == QLatin1Char('('))
                ++depth;
            else if (value.at(i) == QLatin1Char(')') && depth > 0)
                --depth;
            ++i;
        }
        out += value.mid(dstart, i - dstart);
    }
    return out;
}

// Index of the closing tag "</name" (case insensitive) from i, -1 if none
int closingTag(const QString &s, const QString &name, int from)
{
    const QString needle = QStringLiteral("</") + name;
    int i = from;
    while ((i = s.indexOf(needle, i, Qt::CaseInsensitive)) >= 0) {
        const int after = i + needle.size();
        if (after >= s.size() || s.at(after).isSpace() || s.at(after) == QLatin1Char('>')
                || s.at(after) == QLatin1Char('/'))
            return i;
        i = after;
    }
    return -1;
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

namespace {
// Cut as plainExcerpt() does
QString cutText(QString text, int maxChars)
{
    if (maxChars > 0 && text.size() > maxChars) {
        int cut = text.lastIndexOf(QLatin1Char(' '), maxChars);
        if (cut < maxChars * 2 / 3)
            cut = maxChars;
        text = text.left(cut).trimmed() + QChar(0x2026);
    }
    return text;
}

QString attrValue(const QString &tag, const QString &name)
{
    const QRegularExpression re(QStringLiteral("\\b") + name
                                + QStringLiteral("\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(tag);
    if (!m.hasMatch())
        return QString();
    for (int i = 1; i <= 3; ++i)
        if (!m.captured(i).isEmpty())
            return m.captured(i);
    return QString();
}

// Smallest srcset candidate with a width descriptor >= minWidth, else the
// widest; "" without width descriptors
QString pickSrcset(const QString &srcset, int minWidth)
{
    QString best, widest;
    int bestW = 0, widestW = 0;
    // Candidates as the HTML spec splits them: the address runs up to the
    // next whitespace (it may contain commas: Cloudinary "w_400,c_fill",
    // WordPress "?fit=1024,683"), the descriptor up to the next comma
    const QString v = HtmlUtils::decodeEntities(srcset);
    const int n = v.size();
    int i = 0;
    while (i < n) {
        while (i < n && (v.at(i).isSpace() || v.at(i) == QLatin1Char(',')))
            ++i;
        const int ustart = i;
        while (i < n && !v.at(i).isSpace())
            ++i;
        QString url = v.mid(ustart, i - ustart);
        bool noDescriptor = false;
        while (url.endsWith(QLatin1Char(','))) {
            url.chop(1);
            noDescriptor = true;
        }
        QString descriptor;
        if (!noDescriptor) {
            const int dstart = i;
            while (i < n && v.at(i) != QLatin1Char(','))
                ++i;
            descriptor = v.mid(dstart, i - dstart).trimmed();
        }
        if (url.isEmpty() || !descriptor.endsWith(QLatin1Char('w')))
            continue;
        bool ok = false;
        const int w = descriptor.left(descriptor.size() - 1).trimmed().toInt(&ok);
        if (!ok || w <= 0)
            continue;
        if (w >= minWidth && (bestW == 0 || w < bestW)) {
            best = url;
            bestW = w;
        }
        if (w > widestW) {
            widest = url;
            widestW = w;
        }
    }
    return !best.isEmpty() ? best : widest;
}
} // namespace

QUrl listThumbnail(const QUrl &enclosureLink, const QString &enclosureMime,
                   const QString &body, const QUrl &mediaThumbnail,
                   const QUrl &articleUrl, int minWidth)
{
    if (!mediaThumbnail.isEmpty()) {
        const QUrl u = resolve(mediaThumbnail.toString(), articleUrl);
        if (u.isValid())
            return u;
    }
    if (!enclosureLink.isEmpty() && enclosureMime.startsWith(QLatin1String("image/"), Qt::CaseInsensitive)) {
        const QUrl u = resolve(enclosureLink.toString(), articleUrl);
        if (u.isValid())
            return u;
    }
    static const QRegularExpression imgRe(
        QStringLiteral("<img\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = imgRe.globalMatch(body);
    while (it.hasNext()) {
        const QString tag = it.next().captured(0);
        if (isTrackingPixel(tag))
            continue;
        const QString small = pickSrcset(attrValue(tag, QStringLiteral("srcset")), minWidth);
        if (!small.isEmpty()) {
            const QUrl u = resolve(small, articleUrl);
            if (u.isValid())
                return u;
        }
        const QUrl u = resolve(attrValue(tag, QStringLiteral("src")), articleUrl);
        if (u.isValid())
            return u;
    }
    return QUrl();
}

ArticleExtra articleExtra(const QString &title, const QString &body, const QUrl &enclosureLink,
                          const QString &enclosureMime, const QUrl &mediaThumbnail,
                          const QUrl &articleUrl)
{
    ArticleExtra e;
    const QString plain = plainExcerpt(body, 0);
    e.excerpt = cutText(plain, 220);
    e.thumb = listThumbnail(enclosureLink, enclosureMime, body, mediaThumbnail, articleUrl).toString();
    // as hasContent(), with the plain text made once
    e.hasContent = !enclosureLink.isEmpty() || plain.size() >= 25
            || !thumbnail(QUrl(), QString(), body, QUrl(), QUrl(QStringLiteral("https://localhost/"))).isEmpty();
    e.search = QString(decodeEntities(title) + QLatin1Char('\n') + plain).toCaseFolded();
    return e;
}

bool hasContent(const QString &body, const QUrl &enclosureLink, int minTextChars)
{
    if (!enclosureLink.isEmpty())
        return true;
    if (plainExcerpt(body, 0).size() >= minTextChars)
        return true;
    // Rule 2 of thumbnail() alone: a usable <img> in the body
    return !thumbnail(QUrl(), QString(), body, QUrl(), QUrl(QStringLiteral("https://localhost/"))).isEmpty();
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
// Swipe from right to left (1.2, author 17 Oct): "sailfeed:swipe:<id>:next"
// opens the next article. Detected here in the page instead of a QML
// MouseArea over the web view (that would take scrolling and links) and
// without an attached page in the page stack; the swipe to the right stays
// Silica's back navigation. Ignored when zoomed in, with more than one
// finger, in sideways scrollable parts (tables, code) and when mostly
// vertical, slow or short.
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
        // swipe to the next article
        "var sw=null;"
        "function sideScroll(el){for(;el&&el!==document.body&&el.nodeType===1;el=el.parentNode){"
        "if(el.scrollWidth>el.clientWidth+2){var o=getComputedStyle(el).overflowX;if(o==='auto'||o==='scroll')return true;}}return false;}"
        "function zoomed(){var v=window.visualViewport;return v&&v.scale>1.05;}"
        "document.addEventListener('touchstart',function(e){sw=null;if(e.touches.length!==1||zoomed()||sideScroll(e.target))return;"
        "sw={x:e.touches[0].clientX,y:e.touches[0].clientY,t:Date.now()};},{passive:true});"
        "document.addEventListener('touchmove',function(e){if(sw&&e.touches.length!==1)sw=null;},{passive:true});"
        "document.addEventListener('touchcancel',function(){sw=null;},{passive:true});"
        "document.addEventListener('touchend',function(e){if(!sw||!e.changedTouches.length)return;"
        "var dx=e.changedTouches[0].clientX-sw.x,dy=e.changedTouches[0].clientY-sw.y,dt=Date.now()-sw.t;sw=null;"
        "if(dx<=-Math.max(60,window.innerWidth*0.25)&&Math.abs(dy)<Math.abs(dx)*0.5&&dt<800)send('swipe','next');},{passive:true});"
        "})();</script>");
}

QString readerHtml(const QString &articleJson, const ReaderStyle &style)
{
    const QJsonDocument doc = QJsonDocument::fromJson(articleJson.toUtf8());
    if (!doc.isObject())
        return QString();
    const QJsonObject o = doc.object();
    // Article from a lean page (leanPage): addresses back to normal
    const QString content = restoreLeanResources(o.value(QStringLiteral("content")).toString(), style.images);
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

QString leanPage(const QString &html, const QUrl &pageUrl)
{
    QString out;
    out.reserve(html.size() / 2);
    QUrl base = pageUrl.adjusted(QUrl::RemoveFragment);
    // Marker, so WebPage.qml can tell the lean page from the one before or after it
    const QString leanMarker = QStringLiteral("<meta name=\"sailfeed-lean\" content=\"1\">");
    bool marked = false;
    const int n = html.size();
    int i = 0;
    while (i < n) {
        const int lt = html.indexOf(QLatin1Char('<'), i);
        if (lt < 0) {
            out += html.midRef(i);
            break;
        }
        out += html.midRef(i, lt - i);
        i = lt;
        // Comments (also conditional comments)
        if (html.midRef(i, 4) == QLatin1String("<!--")) {
            const int close = html.indexOf(QLatin1String("-->"), i + 4);
            i = close < 0 ? n : close + 3;
            continue;
        }
        const bool isStart = i + 1 < n && isAsciiLetter(html.at(i + 1));
        if (!isStart) {
            // End tags, <!DOCTYPE>, <?…>, a lone '<': unchanged
            const int end = (i + 1 < n && (html.at(i + 1) == QLatin1Char('/') || html.at(i + 1) == QLatin1Char('!')
                                           || html.at(i + 1) == QLatin1Char('?')))
                    ? html.indexOf(QLatin1Char('>'), i) : -1;
            if (end < 0) {
                out += html.at(i);
                ++i;
            } else {
                out += html.midRef(i, end + 1 - i);
                i = end + 1;
            }
            continue;
        }
        const int end = tagEnd(html, i);
        if (end < 0) {      // broken tail
            out += html.midRef(i);
            break;
        }
        const QString tag = html.mid(i, end - i);
        i = end;
        QString name;
        QList<Attr> attrs;
        bool selfClosing = false;
        parseTag(tag, &name, &attrs, &selfClosing);
        auto attrValue = [&attrs](const QString &n) {
            for (const Attr &a : attrs)
                if (a.name == n)
                    return a.value;
            return QString();
        };

        if (name == QLatin1String("script")) {
            const int close = closingTag(html, name, i);
            const int stop = close < 0 ? n : close;
            const int after = close < 0 ? n : qMax(stop, html.indexOf(QLatin1Char('>'), stop) + 1);
            if (attrValue(QStringLiteral("type")).contains(QLatin1String("ld+json"), Qt::CaseInsensitive))
            {
                out += tag;                                 // data only, does not run
                out += html.midRef(i, after - i);
            }
            i = after;
            continue;
        }
        if (name == QLatin1String("iframe") || name == QLatin1String("frame")) {
            const int close = closingTag(html, name, i);
            if (close >= 0) {
                const int gt = html.indexOf(QLatin1Char('>'), close);
                i = gt < 0 ? n : gt + 1;
            }
            continue;
        }
        if (name == QLatin1String("style") || name == QLatin1String("textarea") || name == QLatin1String("title")
                || name == QLatin1String("xmp")) {
            // Raw text: copied up to its end tag without looking inside
            const int close = closingTag(html, name, i);
            const int stop = close < 0 ? n : close;
            out += tag;
            out += html.midRef(i, stop - i);
            i = stop;
            continue;
        }
        if (name == QLatin1String("link") || name == QLatin1String("embed"))
            continue;
        if (name == QLatin1String("meta")
                && attrValue(QStringLiteral("http-equiv")).trimmed().compare(QLatin1String("refresh"), Qt::CaseInsensitive) == 0)
            continue;
        if (name == QLatin1String("head") && !marked) {
            out += tag;
            out += leanMarker;
            marked = true;
            continue;
        }
        if (name == QLatin1String("base")) {
            const QString href = attrValue(QStringLiteral("href")).trimmed();
            if (!href.isEmpty()) {
                const QUrl b(HtmlUtils::decodeEntities(href));
                if (b.isValid())
                    base = base.resolved(b);
            }
            out += tag;
            continue;
        }

        bool changed = false;
        bool hadImageAddress = false;
        QString rebuilt = QLatin1Char('<') + tag.mid(1, name.size());
        for (const Attr &a : attrs) {
            // Inline event handlers (onload, onerror …) would still run: the
            // placeholder images below fire onerror
            if (a.name.size() > 2 && a.name.startsWith(QLatin1String("on"))) {
                changed = true;
                continue;
            }
            // data: addresses load nothing (placeholders of lazy images): kept,
            // so Readability still sees the placeholder and takes data-src
            const bool resource = a.hasValue
                    && !a.value.trimmed().startsWith(QLatin1String("data:"), Qt::CaseInsensitive)
                    && (a.name == QLatin1String("src") || a.name == QLatin1String("srcset")
                        || a.name == QLatin1String("poster") || a.name == QLatin1String("background")
                        || (a.name == QLatin1String("data") && name == QLatin1String("object")));
            if (!resource) {
                rebuilt += QLatin1Char(' ') + a.raw;
                continue;
            }
            changed = true;
            if (name == QLatin1String("img") && (a.name == QLatin1String("src") || a.name == QLatin1String("srcset"))
                    && !a.value.trimmed().isEmpty())
                hadImageAddress = true;
            const QString v = a.name == QLatin1String("srcset") ? absoluteSrcset(a.value, base)
                                                                 : absolute(a.value, base);
            // Values are written in double quotes: a raw '"' (from a
            // single-quoted value) is escaped
            QString quoted = v;
            quoted.replace(QLatin1Char('"'), QLatin1String("&quot;"));
            rebuilt += QStringLiteral(" data-sf-") + a.name + QStringLiteral("=\"") + quoted + QLatin1Char('"');
        }
        if (!changed) {
            out += tag;
            continue;
        }
        if (hadImageAddress)
            rebuilt += QStringLiteral(" src=\"data:,\"");
        rebuilt += selfClosing ? QStringLiteral(" />") : QStringLiteral(">");
        out += rebuilt;
    }
    if (!marked)
        out.prepend(leanMarker);
    return out;
}

QString restoreLeanResources(const QString &content, bool images)
{
    QString out;
    if (images && content.contains(QLatin1String("data-sf-"))) {
        // Tag by tag: Readability may have set src itself (lazy images: it
        // copies data-src into src) – then that address wins and the renamed
        // one (often the page's placeholder) goes; otherwise the renamed one
        // comes back. The placeholder src="data:," always goes.
        out.reserve(content.size());
        int i = 0;
        const int n = content.size();
        while (i < n) {
            const int lt = content.indexOf(QLatin1Char('<'), i);
            if (lt < 0) {
                out += content.midRef(i);
                break;
            }
            out += content.midRef(i, lt - i);
            const int end = (lt + 1 < n && isAsciiLetter(content.at(lt + 1))) ? tagEnd(content, lt) : -1;
            if (end < 0) {
                out += content.at(lt);
                i = lt + 1;
                continue;
            }
            const QString tag = content.mid(lt, end - lt);
            i = end;
            if (!tag.contains(QLatin1String("data-sf-"))) {
                out += tag;
                continue;
            }
            QString name;
            QList<Attr> attrs;
            bool selfClosing = false;
            parseTag(tag, &name, &attrs, &selfClosing);
            auto real = [&attrs](const QString &n) {
                for (const Attr &a : attrs)
                    if (a.name == n && a.value.trimmed() != QLatin1String("data:,"))
                        return true;
                return false;
            };
            QString rebuilt = QLatin1Char('<') + tag.mid(1, name.size());
            for (const Attr &a : attrs) {
                if ((a.name == QLatin1String("src") || a.name == QLatin1String("srcset"))
                        && a.value.trimmed() == QLatin1String("data:,"))
                    continue;
                if (a.name.startsWith(QLatin1String("data-sf-"))) {
                    const QString orig = a.name.mid(8);
                    if (orig == QLatin1String("src") || orig == QLatin1String("srcset") || orig == QLatin1String("poster")) {
                        if (!real(orig))
                            rebuilt += QLatin1Char(' ') + orig + QStringLiteral("=\"") + a.value + QLatin1Char('"');
                        continue;
                    }
                }
                rebuilt += QLatin1Char(' ') + a.raw;
            }
            rebuilt += selfClosing ? QStringLiteral(" />") : QStringLiteral(">");
            out += rebuilt;
        }
    } else {
        out = content;
    }
    static const QRegularExpression imgRe(QStringLiteral("<img\\b(?![^>]*\\sloading=)"),
                                          QRegularExpression::CaseInsensitiveOption);
    out.replace(imgRe, QStringLiteral("<img loading=\"lazy\""));
    return out;
}

} // namespace HtmlUtils

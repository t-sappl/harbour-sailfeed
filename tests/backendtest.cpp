// SPDX-FileCopyrightText: 2026 Thomas Sappl
// SPDX-License-Identifier: GPL-3.0-or-later
// Backend test for harbour-sailfeed: HtmlUtils, ArticleModel, AccountManager,
// Synchronizer + queue against a mock News API server (mocknews.py).
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include <algorithm>
#include <cstdio>

#include <Fuoten/API/Component>
#include <Fuoten/Article>
#include <Fuoten/Error>
#include <Fuoten/Helpers/Synchronizer>
#include <Fuoten/Storage/SQLiteStorage>

#include "accountmanager.h"
#include "articlemodel.h"
#include "articleindex.h"
#include "feedmanager.h"
#include "feedmodel.h"
#include "feedrowsmodel.h"
#include <Fuoten/Feed>
#include <Fuoten/Folder>
#include "prefetcher.h"
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "configuration.h"
#include "htmlutils.h"
#include "nextcloudfiles.h"
#include "settingsbackup.h"
#include "cloudbackup.h"
#include "feedfinder.h"
#include "articlecleaner.h"
#include "opmlfile.h"
#include "feedtransfer.h"
#include <QTemporaryDir>
#include <QFileInfo>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } else { std::printf("ok   %s\n", #cond); } } while (0)
#define CHECK_EQ(a, b) do { const auto _a = (a); const auto _b = (b); if (!(_a == _b)) { ++failures; std::printf("FAIL %s:%d: %s == %s\n     got: %s\n     exp: %s\n", __FILE__, __LINE__, #a, #b, qUtf8Printable(QVariant(_a).toString()), qUtf8Printable(QVariant(_b).toString())); } else { std::printf("ok   %s == %s\n", #a, #b); } } while (0)

static bool waitFor(const std::function<bool()> &cond, int ms = 10000)
{
    QElapsedTimer t; t.start();
    while (!cond() && t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return cond();
}

static void testHtml()
{
    using namespace HtmlUtils;
    const QUrl base(QStringLiteral("https://www.golem.de/news/x.html"));
    CHECK_EQ(thumbnail(QUrl(QStringLiteral("https://e/x.jpg")), QStringLiteral("image/jpeg"), QStringLiteral("<img src=\"https://b/y.jpg\">"), QUrl(), base).toString(),
             QStringLiteral("https://e/x.jpg"));
    CHECK_EQ(thumbnail(QUrl(QStringLiteral("https://e/x.mp3")), QStringLiteral("audio/mpeg"), QStringLiteral("<p><IMG class=a SRC='//cdn.x/y.jpg?a=1&amp;b=2'></p>"), QUrl(), base).toString(),
             QStringLiteral("https://cdn.x/y.jpg?a=1&b=2"));
    CHECK_EQ(thumbnail(QUrl(), QString(), QStringLiteral("<img src=\"https://t/p.gif\" width=\"1\" height=\"1\"><img src=\"/img/a.png\">"), QUrl(), base).toString(),
             QStringLiteral("https://www.golem.de/img/a.png"));
    CHECK_EQ(thumbnail(QUrl(), QString(), QStringLiteral("<img src=data:image/png;base64,AAA>"), QUrl(QStringLiteral("https://m/t.jpg")), base).toString(),
             QStringLiteral("https://m/t.jpg"));
    CHECK(thumbnail(QUrl(), QString(), QStringLiteral("<p>none</p>"), QUrl(), base).isEmpty());

    // 1.2: articles without text open the full article at once
    CHECK(!HtmlUtils::hasContent(QString(), QUrl()));                                           // news.orf.at
    CHECK(!HtmlUtils::hasContent(QStringLiteral("  <p>&nbsp;</p> "), QUrl()));
    CHECK(!HtmlUtils::hasContent(QStringLiteral("<a href=\"https://x/a\">Weiterlesen &hellip;</a>"), QUrl()));
    CHECK(!HtmlUtils::hasContent(QStringLiteral("<img src=\"https://t/p.gif\" width=\"1\" height=\"1\">"), QUrl()));   // tracking pixel only
    CHECK(HtmlUtils::hasContent(QStringLiteral("<p>Der Ministerrat hat heute das Budget beschlossen.</p>"), QUrl()));
    CHECK(HtmlUtils::hasContent(QStringLiteral("<img src=\"https://comic/today.png\">"), QUrl()));                      // comic
    CHECK(HtmlUtils::hasContent(QString(), QUrl(QStringLiteral("https://pod/ep1.mp3"))));                               // podcast
    CHECK_EQ(decodeEntities(QStringLiteral("F&uuml;nf &amp; &#8222;x&#x201C; &hellip; &bogus;")),
             QString::fromUtf8("Fünf & „x“ … &bogus;"));
    CHECK_EQ(plainExcerpt(QStringLiteral("<p>Hallo&nbsp;<b>Welt</b></p>\n<script>evil()</script><p>Zwei</p>"), 220),
             QStringLiteral("Hallo Welt Zwei"));
    const QString longText = QStringLiteral("wort ").repeated(100);
    const QString ex = plainExcerpt(longText, 50);
    CHECK(ex.size() <= 51 && ex.endsWith(QChar(0x2026)));
    CHECK(readerHtml(QStringLiteral("not json"), ReaderStyle()).isEmpty());
    // 1.2: swipe to the left in the reader page reports "next"
    CHECK(readerHtml(QStringLiteral("{\"content\":\"<p>x</p>\"}"), ReaderStyle()).contains(QStringLiteral("send('swipe','next')")));
    CHECK(readerHtml(QStringLiteral("{\"title\":\"x\",\"content\":\"  \"}"), ReaderStyle()).isEmpty());
    {
        const QString r = readerHtml(QStringLiteral("{\"title\":\"A <b>&</b>\",\"siteName\":\"Golem\",\"content\":\"<p>Text</p>\",\"lang\":\"de\"}"), ReaderStyle());
        CHECK(r.contains(QStringLiteral("<h1>A &lt;b&gt;&amp;&lt;/b&gt;</h1>")));
        CHECK(r.contains(QStringLiteral("<article><p>Text</p></article>")));
        CHECK(r.contains(QStringLiteral("lang=\"de\"")));
        CHECK(r.contains(QStringLiteral("font-size:20px")));
        // Marker (WebPage uncovers only its own reader page) and the link script
        CHECK(r.contains(QStringLiteral("<meta name=\"sailfeed-reader\" content=\"1\">")));
        CHECK(r.contains(QStringLiteral("'sailfeed:'+kind")));
        CHECK(r.contains(QStringLiteral("data-sf-links")));                 // second channel (polled)
        CHECK(r.indexOf(QStringLiteral("</article>")) < r.indexOf(QStringLiteral("<script>")));
        CHECK(r.endsWith(QStringLiteral("</script></body></html>")));
        ReaderStyle big; big.fontPx = 26;
        CHECK(readerHtml(QStringLiteral("{\"content\":\"<p>x</p>\"}"), big).contains(QStringLiteral("font-size:26px")));
    }
    {
        // Reader mode settings
        const QString json = QStringLiteral("{\"content\":\"<p>x</p>\"}");
        const QString def = readerHtml(json, ReaderStyle());
        CHECK(def.contains(QStringLiteral("sans-serif")));
        CHECK(def.contains(QStringLiteral("line-height:1.55")));
        CHECK(def.contains(QStringLiteral("padding:1.2em 1em 3em 1em")));
        CHECK(def.contains(QStringLiteral("text-align:start")));
        CHECK(def.contains(QStringLiteral("max-width:42em")));        // readable width in landscape / on tablets
        CHECK(!def.contains(QStringLiteral("display:none !important")));
        ReaderStyle st; st.serif = true; st.lineHeight = 1.9; st.margin = 2; st.justify = true; st.images = false;
        const QString r = readerHtml(json, st);
        CHECK(r.contains(QStringLiteral("Georgia,serif")));
        CHECK(r.contains(QStringLiteral("line-height:1.90")));
        CHECK(r.contains(QStringLiteral("padding:1.2em 1.6em 3em 1.6em")));
        CHECK(r.contains(QStringLiteral("text-align:justify")) && r.contains(QStringLiteral("hyphens:auto")));
        CHECK(r.contains(QStringLiteral("img,picture,video")));
        st.lineHeight = 9; st.margin = -3;
        const QString clamped = readerHtml(json, st);
        CHECK(clamped.contains(QStringLiteral("line-height:2.20")));
        CHECK(clamped.contains(QStringLiteral("padding:1.2em 0.6em 3em 0.6em")));
    }
    {
        // Lean page for the extraction (1.4): nothing that loads or runs
        const QUrl pageUrl(QStringLiteral("https://news.example/2026/a/artikel.html#top"));
        const QString page = QStringLiteral(
            "<!DOCTYPE html><html><HEAD><title>T <img src=x></title>"
            "<meta http-equiv=\"Refresh\" content=\"0; url=/nojs\"><meta charset=\"utf-8\">"
            "<link rel=\"stylesheet\" href=\"/s.css\"><link rel=preload href=/f.woff2 as=font>"
            "<script src=\"/app.js\"></script><script>var s = '</div><img src=a>';</script >"
            "<SCRIPT type=\"application/ld+json\">{\"author\":\"Eva\"}</SCRIPT>"
            "<style>.x{position:fixed}</style></HEAD>"
            "<body background=\"bg.png\"><!-- <script>evil()</script> -->"
            "<p class=a>Text &amp; mehr <b>fett</b> a < b</p>"
            "<img src=\"bild.jpg\" alt=\"Bild\" width=600>"
            "<img data-src=\"lazy.jpg\" class=\"lazy\">"
            "<IMG SRC='//cdn.example/x?w=1&amp;h=2' srcset=\"k.jpg 1x, /g.jpg 2x\">"
            "<picture><source srcset=\"/a.webp\" type=\"image/webp\"><img src=\"https://cdn.example/abs.png\"/></picture>"
            "<video poster=\"p.jpg\" src=\"v.mp4\" controls></video>"
            "<iframe src=\"https://www.youtube.com/embed/x\"><p>inner</p></iframe><embed src=\"x.swf\">"
            "<object data=\"o.svg\"></object>"
            "<textarea><img src=\"keep.png\"></textarea>"
            "<a href=\"rel.html\">Link</a></body></html>");
        const QString lean = leanPage(page, pageUrl);
        CHECK(lean.startsWith(QStringLiteral("<!DOCTYPE html><html><HEAD><meta name=\"sailfeed-lean\" content=\"1\"><title>T <img src=x></title>")));
        CHECK(!lean.contains(QStringLiteral("app.js")) && !lean.contains(QStringLiteral("var s")));
        CHECK(lean.contains(QStringLiteral("<SCRIPT type=\"application/ld+json\">{\"author\":\"Eva\"}</SCRIPT>")));
        CHECK(!lean.contains(QStringLiteral("<link")) && !lean.contains(QStringLiteral("Refresh")));
        CHECK(lean.contains(QStringLiteral("<meta charset=\"utf-8\">")));
        CHECK(lean.contains(QStringLiteral("<style>.x{position:fixed}</style>")));
        CHECK(!lean.contains(QStringLiteral("evil")));
        CHECK(lean.contains(QStringLiteral("<body data-sf-background=\"https://news.example/2026/a/bg.png\">")));
        CHECK(lean.contains(QStringLiteral("<p class=a>Text &amp; mehr <b>fett</b> a < b</p>")));
        CHECK(lean.contains(QStringLiteral("<img data-sf-src=\"https://news.example/2026/a/bild.jpg\" alt=\"Bild\" width=600 src=\"data:,\">")));
        CHECK(lean.contains(QStringLiteral("<img data-src=\"lazy.jpg\" class=\"lazy\">")));      // Readability fixes lazy images itself
        // A placeholder (data:) stays, the image does not get a second one
        CHECK_EQ(leanPage(QStringLiteral("<img class=lazyload src=\"data:image/gif;base64,R0l=\" data-src=\"/d.jpg\">"), pageUrl),
                 QStringLiteral("<meta name=\"sailfeed-lean\" content=\"1\"><img class=lazyload src=\"data:image/gif;base64,R0l=\" data-src=\"/d.jpg\">"));
        CHECK(lean.contains(QStringLiteral("<IMG data-sf-src=\"https://cdn.example/x?w=1&amp;h=2\" "
                                           "data-sf-srcset=\"https://news.example/2026/a/k.jpg 1x, https://news.example/g.jpg 2x\" src=\"data:,\">")));
        CHECK(lean.contains(QStringLiteral("<source data-sf-srcset=\"https://news.example/a.webp\" type=\"image/webp\">")));
        CHECK(lean.contains(QStringLiteral("<img data-sf-src=\"https://cdn.example/abs.png\" src=\"data:,\" />")));
        CHECK(lean.contains(QStringLiteral("<video data-sf-poster=\"https://news.example/2026/a/p.jpg\" data-sf-src=\"https://news.example/2026/a/v.mp4\" controls>")));
        CHECK(!lean.contains(QStringLiteral("youtube")) && !lean.contains(QStringLiteral("inner")) && !lean.contains(QStringLiteral("x.swf")));
        CHECK(lean.contains(QStringLiteral("<object data-sf-data=")));
        CHECK(lean.contains(QStringLiteral("<textarea><img src=\"keep.png\"></textarea>")));
        CHECK(lean.contains(QStringLiteral("<a href=\"rel.html\">Link</a></body></html>")));
        // Nothing in the lean page loads: no src/srcset/poster attribute but the placeholder
        {
            static const QRegularExpression loads(QStringLiteral("\\s(src|srcset|poster)=\"(?!data:,)"),
                                                  QRegularExpression::CaseInsensitiveOption);
            const QString body = lean.mid(lean.indexOf(QStringLiteral("<body")));
            const QString outsideTextarea = body.left(body.indexOf(QStringLiteral("<textarea")));
            CHECK(!loads.match(outsideTextarea).hasMatch());
        }
        // <base href> changes how addresses resolve; no <head>: marker in front
        const QString based = leanPage(QStringLiteral("<base href=\"https://cdn.example/m/\"><img src=\"i.png\">"), pageUrl);
        CHECK(based.startsWith(QStringLiteral("<meta name=\"sailfeed-lean\" content=\"1\"><base href=")));
        CHECK(based.contains(QStringLiteral("data-sf-src=\"https://cdn.example/m/i.png\"")));
        // Broken markup does not hang or lose the rest
        CHECK_EQ(leanPage(QStringLiteral("<p>a</p><img src=\"x"), pageUrl), QStringLiteral("<meta name=\"sailfeed-lean\" content=\"1\"><p>a</p><img src=\"x"));
        CHECK(leanPage(QStringLiteral("<script>never closed"), pageUrl).endsWith(QStringLiteral("content=\"1\">")));
        CHECK(leanPage(QStringLiteral("x <!-- open"), pageUrl).endsWith(QStringLiteral("x ")));
        // Back to normal in Readability's output
        const QString art = QStringLiteral("<p><img data-sf-src=\"https://e/a.jpg\" alt=\"\" src=\"data:,\"></p>"
                                           "<img data-sf-srcset=\"https://e/b.jpg 2x\" src=\"data:,\" loading=\"eager\">"
                                           "<video data-sf-poster=\"https://e/p.jpg\"></video>");
        CHECK_EQ(restoreLeanResources(art, true),
                 QStringLiteral("<p><img loading=\"lazy\" src=\"https://e/a.jpg\" alt=\"\"></p>"
                                "<img srcset=\"https://e/b.jpg 2x\" loading=\"eager\">"
                                "<video poster=\"https://e/p.jpg\"></video>"));
        CHECK(!restoreLeanResources(art, false).contains(QStringLiteral(" src=\"https")));          // hidden images stay unloaded
        // Readability set src itself (lazy image: data-src copied into src):
        // that one wins, the renamed placeholder goes
        CHECK_EQ(restoreLeanResources(QStringLiteral("<img class=\"lazyload\" data-sf-src=\"https://e/blank.gif\" data-src=\"/real.jpg\" src=\"https://e/real.jpg\">"), true),
                 QStringLiteral("<img loading=\"lazy\" class=\"lazyload\" data-src=\"/real.jpg\" src=\"https://e/real.jpg\">"));
        CHECK_EQ(restoreLeanResources(QStringLiteral("<p>a &lt; b</p><img data-sf-src=\"https://e/x.jpg\" src=\"data:,\"/>"), true),
                 QStringLiteral("<p>a &lt; b</p><img loading=\"lazy\" src=\"https://e/x.jpg\" />"));
        // Inline event handlers are gone (the placeholders would fire onerror)
        CHECK_EQ(leanPage(QStringLiteral("<img src=\"a.png\" onerror=\"go()\" ONLOAD='x()'><body onload=\"y()\" class=c>"), pageUrl),
                 QStringLiteral("<meta name=\"sailfeed-lean\" content=\"1\"><img data-sf-src=\"https://news.example/2026/a/a.png\" src=\"data:,\"><body class=c>"));
        CHECK(readerHtml(QStringLiteral("{\"content\":\"<img data-sf-src=\\\"https://e/a.jpg\\\" src=\\\"data:,\\\">\"}"), ReaderStyle())
              .contains(QStringLiteral("<img loading=\"lazy\" src=\"https://e/a.jpg\">")));
        // Feed text (showFeedText) has normal addresses: kept, made lazy
        CHECK(readerHtml(QStringLiteral("{\"content\":\"<img src=\\\"https://e/f.jpg\\\">\"}"), ReaderStyle())
              .contains(QStringLiteral("<img loading=\"lazy\" src=\"https://e/f.jpg\">")));
    }
    {
        // List thumbnails (1.4): small images first
        const QUrl art(QStringLiteral("https://news.example/a/1.html"));
        CHECK_EQ(listThumbnail(QUrl(QStringLiteral("https://e/big.jpg")), QStringLiteral("image/jpeg"), QString(),
                               QUrl(QStringLiteral("https://e/small.jpg")), art).toString(), QStringLiteral("https://e/small.jpg"));
        CHECK_EQ(listThumbnail(QUrl(QStringLiteral("https://e/big.jpg")), QStringLiteral("image/jpeg"), QString(), QUrl(), art).toString(),
                 QStringLiteral("https://e/big.jpg"));
        const QString body = QStringLiteral("<img src=\"/t.gif\" width=\"1\" height=\"1\">"
                                            "<img src=\"/full.jpg\" srcset=\"/w200.jpg 200w, /w400.jpg 400w, /w1600.jpg 1600w\">");
        CHECK_EQ(listThumbnail(QUrl(), QString(), body, QUrl(), art).toString(), QStringLiteral("https://news.example/w400.jpg"));
        CHECK_EQ(listThumbnail(QUrl(), QString(), body, QUrl(), art, 2000).toString(), QStringLiteral("https://news.example/w1600.jpg"));
        CHECK_EQ(listThumbnail(QUrl(), QString(), QStringLiteral("<img src='/only.png' srcset='/x.png 2x'>"), QUrl(), art).toString(),
                 QStringLiteral("https://news.example/only.png"));                     // no widths: src
        CHECK_EQ(thumbnail(QUrl(), QString(), body, QUrl(), art).toString(), QStringLiteral("https://news.example/full.jpg"));   // preview: big
        CHECK(listThumbnail(QUrl(), QString(), QStringLiteral("<p>x</p>"), QUrl(), art).isEmpty());
        // Commas inside the addresses (Cloudinary, WordPress)
        CHECK_EQ(listThumbnail(QUrl(), QString(), QStringLiteral("<img src=\"/f.jpg\" srcset=\"https://res.cloudinary.com/d/upload/w_200,c_fill/a.jpg 200w, "
                                                                 "https://res.cloudinary.com/d/upload/w_400,c_fill/a.jpg 400w\">"), QUrl(), art).toString(),
                 QStringLiteral("https://res.cloudinary.com/d/upload/w_400,c_fill/a.jpg"));
        CHECK_EQ(listThumbnail(QUrl(), QString(), QStringLiteral("<img src=\"/f.jpg\" srcset=\"https://wp.example/i.jpg?fit=1024%2C683&amp;w=1024 1024w,https://wp.example/i.jpg?fit=300,200 300w\">"),
                               QUrl(), art).toString(), QStringLiteral("https://wp.example/i.jpg?fit=1024%2C683&w=1024"));
        // Everything for the list and the search, the plain text made once
        const ArticleExtra e = articleExtra(QStringLiteral("Fünf &amp; Kameras"), QStringLiteral("<p>Ring erweitert <b>das</b> Sortiment.</p>"),
                                            QUrl(), QString(), QUrl(), art);
        CHECK_EQ(e.excerpt, QStringLiteral("Ring erweitert das Sortiment."));
        CHECK_EQ(e.excerpt, plainExcerpt(QStringLiteral("<p>Ring erweitert <b>das</b> Sortiment.</p>")));
        CHECK(e.hasContent);
        CHECK(e.thumb.isEmpty());
        CHECK_EQ(e.search, QString::fromUtf8("fünf & kameras\nring erweitert das sortiment."));
        const QString longText = QStringLiteral("<p>") + QStringLiteral("wort ").repeated(100) + QStringLiteral("</p>");
        CHECK_EQ(articleExtra(QString(), longText, QUrl(), QString(), QUrl(), art).excerpt, plainExcerpt(longText, 220));
        CHECK(!articleExtra(QString(), QStringLiteral("<a href=x>Mehr</a>"), QUrl(), QString(), QUrl(), art).hasContent);
        CHECK(articleExtra(QString(), QString(), QUrl(QStringLiteral("https://p/e.mp3")), QStringLiteral("audio/mpeg"), QUrl(), art).hasContent);
        CHECK_EQ(articleExtra(QString(), QStringLiteral("<a href=x>Mehr</a>"), QUrl(), QString(), QUrl(), art).hasContent,
                 hasContent(QStringLiteral("<a href=x>Mehr</a>"), QUrl()));
    }
    // "Always open in the original" sites
    CHECK_EQ(siteKey(QStringLiteral("https://www.Golem.de/news/x.html")), QStringLiteral("golem.de"));
    CHECK_EQ(siteKey(QStringLiteral("derstandard.at")), QStringLiteral("derstandard.at"));
    CHECK(siteKey(QStringLiteral("")).isEmpty());
    {
        const QStringList sites { QStringLiteral("golem.de"), QStringLiteral("www.heise.de") };
        CHECK(isListedSite(QStringLiteral("https://www.golem.de/a"), sites));
        CHECK(isListedSite(QStringLiteral("https://video.golem.de/a"), sites));
        CHECK(isListedSite(QStringLiteral("https://heise.de/x"), sites));
        CHECK(!isListedSite(QStringLiteral("https://notgolem.de/a"), sites));
        CHECK(!isListedSite(QStringLiteral("https://example.org"), sites));
        CHECK(!isListedSite(QStringLiteral("https://golem.de"), QStringList()));
    }
    CHECK_EQ(cleanBody(QStringLiteral("<p>a<img src=x></p><iframe src=y></iframe><script>1</script><p> </p><a href=\"u\">l</a>")),
             QStringLiteral("<p>a</p><a href=\"u\">l</a>"));
    // Wrappers left empty by the removed images (The Next Web), nested, with
    // breaks; a link with text and a paragraph with content stay
    CHECK_EQ(cleanBody(QStringLiteral("<p><a href=\"https://t/x.avif\"><img src=\"https://t/x.avif\"/></a></p>\n"
                                      "<figure class=\"f\"><img src=x><figcaption> </figcaption></figure><br/>"
                                      "<p>Text <a href=\"u\">link</a></p><div><span>&nbsp;</span></div>")),
             QStringLiteral("<p>Text <a href=\"u\">link</a></p>"));
    CHECK_EQ(cleanBody(QStringLiteral("<br><br />Start<p><b>bold</b></p>")),
             QStringLiteral("Start<p><b>bold</b></p>"));
}

// "Subscribe to website": feed parsing, discovery helpers and the search
// against the mock's test sites
static QVariantList runFinder(FeedFinder &f, const QString &address)
{
    bool done = false;
    auto c = QObject::connect(&f, &FeedFinder::finished, [&] { done = true; });
    f.find(address);
    CHECK(waitFor([&] { return done; }, 30000));
    QObject::disconnect(c);
    return f.feeds();
}

static QStringList titlesOf(const QVariantList &feeds)
{
    QStringList t;
    for (const QVariant &v : feeds)
        t << v.toMap().value(QStringLiteral("title")).toString();
    return t;
}

static void testFeedFinder(const QString &port)
{
    // parseFeed
    {
        const QVariantMap r = FeedFinder::parseFeed(QByteArrayLiteral(
            "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>T &amp; Co</title><description>&lt;p&gt;Desc&lt;/p&gt;</description>"
            "<image><title>Logo</title></image>"
            "<item><title>A</title><pubDate>Mon, 12 Oct 2026 10:00:00 GMT</pubDate></item>"
            "<item><title>B</title><pubDate>Tue, 13 Oct 2026 10:00:00 +0200</pubDate></item>"
            "<item><title>C</title></item><item><title>D</title></item></channel></rss>"));
        CHECK(r.value(QStringLiteral("ok")).toBool());
        CHECK_EQ(r.value(QStringLiteral("format")).toString(), QStringLiteral("RSS"));
        CHECK_EQ(r.value(QStringLiteral("title")).toString(), QStringLiteral("T & Co"));
        CHECK_EQ(r.value(QStringLiteral("description")).toString(), QStringLiteral("Desc"));
        CHECK_EQ(r.value(QStringLiteral("itemCount")).toInt(), 4);
        CHECK_EQ(r.value(QStringLiteral("recent")).toStringList(), (QStringList{ QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C") }));
        CHECK_EQ(r.value(QStringLiteral("latest")).toDateTime().toUTC(), QDateTime(QDate(2026, 10, 13), QTime(8, 0), Qt::UTC));
        CHECK(!r.value(QStringLiteral("podcast")).toBool());
    }
    {
        const QVariantMap r = FeedFinder::parseFeed(QByteArrayLiteral(
            "<feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Atom</title><subtitle>S</subtitle>"
            "<entry><title type=\"html\">E1</title><published>2026-10-01T10:00:00Z</published>"
            "<link rel=\"enclosure\" type=\"audio/mpeg\" href=\"x.mp3\"/></entry></feed>"));
        CHECK(r.value(QStringLiteral("ok")).toBool());
        CHECK_EQ(r.value(QStringLiteral("format")).toString(), QStringLiteral("Atom"));
        CHECK_EQ(r.value(QStringLiteral("description")).toString(), QStringLiteral("S"));
        CHECK(r.value(QStringLiteral("podcast")).toBool());
    }
    CHECK_EQ(FeedFinder::parseFeed(QByteArrayLiteral("<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><channel><title>R</title></channel><item><title>x</title></item></rdf:RDF>")).value(QStringLiteral("format")).toString(), QStringLiteral("RDF"));
    CHECK(!FeedFinder::parseFeed(QByteArrayLiteral("<html><head></head><body>x</body></html>")).value(QStringLiteral("ok")).toBool());
    CHECK(!FeedFinder::parseFeed(QByteArrayLiteral("not xml at all")).value(QStringLiteral("ok")).toBool());

    // discoverLinks
    {
        const QList<QUrl> l = FeedFinder::discoverLinks(QByteArrayLiteral(
            "<head><base href=\"https://ex.org/blog/\"><link rel='alternate' type='application/atom+xml' href='atom.xml'>"
            "<link rel=\"alternate\" hreflang=\"de\" href=\"/de/\"><link rel=\"stylesheet\" href=\"s.css\"></head>"
            "<body><a href=\"/feedback\">Feedback</a><a href=\"/rss?cat=1&amp;x=2\"><img alt=\"RSS\" src=\"i.png\"></a>"
            "<a href=\"/about\">About</a><a href=\"feed://ex.org/feed/\">Subscribe</a><a href=\"/a\">RSS</a><a href=\"/atom.xml\">x</a></body>"),
            QUrl(QStringLiteral("https://ex.org/blog/post")));
        QStringList s;
        for (const QUrl &u : l) s << u.toString();
        CHECK_EQ(s, (QStringList{ QStringLiteral("https://ex.org/blog/atom.xml"), QStringLiteral("https://ex.org/rss?cat=1&x=2"),
                                  QStringLiteral("https://ex.org/feed/"), QStringLiteral("https://ex.org/a"),
                                  QStringLiteral("https://ex.org/atom.xml") }));
    }
    {
        QStringList s;
        for (const QUrl &u : FeedFinder::parentPages(QUrl(QStringLiteral("https://ex.org/a/b/c.html?x=1")))) s << u.toString();
        CHECK_EQ(s, (QStringList{ QStringLiteral("https://ex.org/a/b/"), QStringLiteral("https://ex.org/a/"), QStringLiteral("https://ex.org/") }));
        CHECK(FeedFinder::parentPages(QUrl(QStringLiteral("https://ex.org/"))).isEmpty());
    }
    {
        const QList<QUrl> w = FeedFinder::wellKnownCandidates(QUrl(QStringLiteral("https://ex.org/blog/2026/x")), { QStringLiteral("index.xml"), QStringLiteral("news.rss") });
        QStringList s;
        for (const QUrl &u : w) s << u.toString();
        CHECK(s.contains(QStringLiteral("https://ex.org/feed")));
        CHECK(s.contains(QStringLiteral("https://ex.org/?feed=rss2")));
        CHECK(s.contains(QStringLiteral("https://ex.org/feeds/posts/default")));
        CHECK(s.contains(QStringLiteral("https://ex.org/news.rss")));
        CHECK(s.contains(QStringLiteral("https://ex.org/blog/feed/")));
        CHECK_EQ(s.count(QStringLiteral("https://ex.org/index.xml")), 1);
    }
    CHECK_EQ(FeedFinder::fileNamesOf({ QStringLiteral("https://a.org/index.xml"), QStringLiteral("https://b.org/index.xml"),
                                       QStringLiteral("https://c.org/feed/"), QStringLiteral("https://d.org/news") }),
             (QStringList{ QStringLiteral("index.xml"), QStringLiteral("feed/") }));
    CHECK_EQ(FeedFinder::compareKey(QStringLiteral("https://www.Ex.org/feed/")), FeedFinder::compareKey(QStringLiteral("http://ex.org/feed")));
    // tracking parameters and fragments do not count, other parameters do
    CHECK_EQ(FeedFinder::compareKey(QStringLiteral("https://www.heise.de/rss/heise.rdf?wt_mc=rss.red.ho.ho.rdf.beitrag.beitrag")),
             FeedFinder::compareKey(QStringLiteral("https://heise.de/rss/heise.rdf")));
    CHECK_EQ(FeedFinder::compareKey(QStringLiteral("https://ex.org/feed/?utm_source=x&utm_medium=y#top")), QStringLiteral("ex.org/feed"));
    CHECK_EQ(FeedFinder::compareKey(QStringLiteral("https://ex.org/?feed=rss2&utm_source=x")), QStringLiteral("ex.org?feed=rss2"));
    CHECK(FeedFinder::compareKey(QStringLiteral("https://ex.org/?feed=rss2")) != FeedFinder::compareKey(QStringLiteral("https://ex.org/?feed=atom")));
    CHECK_EQ(FeedFinder::titleSiteKey(QStringLiteral(" The  Next Web "), QStringLiteral("https://www.thenextweb.com/news")),
             QStringLiteral("the next web|thenextweb.com"));
    CHECK(FeedFinder::titleSiteKey(QString(), QStringLiteral("https://ex.org")).isEmpty());
    {
        const QVariantMap r = FeedFinder::parseFeed(QByteArrayLiteral(
            "<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\"><channel><title>T</title><link>https://site.org/</link>"
            "<atom:link rel=\"self\" href=\"https://site.org/feed/\"/><image><link>https://img.org/</link></image>"
            "<item><title>A</title><link>https://site.org/a</link></item></channel></rss>"));
        CHECK_EQ(r.value(QStringLiteral("selfUrl")).toString(), QStringLiteral("https://site.org/feed/"));
        CHECK_EQ(r.value(QStringLiteral("siteLink")).toString(), QStringLiteral("https://site.org/"));
    }
    CHECK(FeedFinder::looksLikeComments(QStringLiteral("https://ex.org/comments/feed/"), QString()));
    CHECK(FeedFinder::looksLikeComments(QString(), QStringLiteral("Kommentare zu X")));

    // Search against the mock
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(port);
    FeedFinder f;
    f.setKnownUrls({ base + QStringLiteral("/site/comments/feed/") });
    // a page announcing feeds: news, comments (already subscribed), podcast (linked)
    QVariantList r = runFinder(f, base + QStringLiteral("/site/"));
    CHECK_EQ(titlesOf(r), (QStringList{ QStringLiteral("Site News"), QStringLiteral("Comments for Site"), QStringLiteral("Site Podcast") }));
    if (r.size() == 3) {
        const QVariantMap news = r.at(0).toMap(), com = r.at(1).toMap(), pod = r.at(2).toMap();
        CHECK_EQ(news.value(QStringLiteral("itemCount")).toInt(), 4);
        CHECK_EQ(news.value(QStringLiteral("recent")).toStringList().value(0), QStringLiteral("First & best"));
        CHECK(!news.value(QStringLiteral("subscribed")).toBool());
        CHECK(com.value(QStringLiteral("comments")).toBool());
        CHECK(com.value(QStringLiteral("subscribed")).toBool());
        CHECK(pod.value(QStringLiteral("podcast")).toBool());
    }
    CHECK(f.error().isEmpty());
    // an article page without feed links: found via its parent pages
    r = runFinder(f, base + QStringLiteral("/site/2026/10/post.html"));
    CHECK_EQ(r.size(), 3);
    // the address is a feed itself (and a redirect to one)
    r = runFinder(f, base + QStringLiteral("/direct.atom"));
    CHECK_EQ(titlesOf(r), QStringList{ QStringLiteral("Direct Atom") });
    r = runFinder(f, base + QStringLiteral("/redir"));
    CHECK_EQ(titlesOf(r), QStringList{ QStringLiteral("Site News") });
    // nothing announced or above: well-known paths of the sub-folder blog
    r = runFinder(f, base + QStringLiteral("/blog/2026/hello"));
    CHECK_EQ(titlesOf(r), QStringList{ QStringLiteral("Sub-folder blog") });
    // errors
    r = runFinder(f, QStringLiteral("not a url with spaces"));
    CHECK(r.isEmpty() && !f.error().isEmpty());
    r = runFinder(f, base + QStringLiteral("/site/missing"));
    CHECK(r.isEmpty() && f.error().contains(QStringLiteral("404")));

    // Already subscribed (device test 15 Oct): by an address on the way
    // (the subscribed address redirects to the feed found) …
    f.setKnownUrls({ base + QStringLiteral("/redir") });
    r = runFinder(f, base + QStringLiteral("/redir"));
    CHECK(r.size() == 1 && r.at(0).toMap().value(QStringLiteral("subscribed")).toBool());
    // … by the feed's self link, with tracking parameters in the announced address …
    f.setKnownUrls({ base + QStringLiteral("/canonical/feed") });
    r = runFinder(f, base + QStringLiteral("/self/"));
    CHECK_EQ(titlesOf(r), QStringList{ QStringLiteral("Self News") });
    CHECK(r.size() == 1 && r.at(0).toMap().value(QStringLiteral("subscribed")).toBool());
    // … and without it only the tracking-free address
    f.setKnownUrls({ base + QStringLiteral("/self/feed.rss") });
    r = runFinder(f, base + QStringLiteral("/self/"));
    CHECK(r.size() == 1 && r.at(0).toMap().value(QStringLiteral("subscribed")).toBool());
    // same title and website, other address: "likely", the button stays
    f.setKnownUrls({});
    QVariantMap known;
    known.insert(QStringLiteral("url"), QStringLiteral("https://feeds.example.net/likely"));
    known.insert(QStringLiteral("link"), QStringLiteral("https://www.likely.example.org/home"));
    known.insert(QStringLiteral("title"), QStringLiteral("Likely News"));
    f.setKnownFeeds({ known });
    r = runFinder(f, base + QStringLiteral("/likely.xml"));
    CHECK(r.size() == 1);
    if (r.size() == 1) {
        CHECK(!r.at(0).toMap().value(QStringLiteral("subscribed")).toBool());
        CHECK_EQ(r.at(0).toMap().value(QStringLiteral("likelySubscribedAs")).toString(), QStringLiteral("Likely News"));
    }
    // its self link is the subscribed address: subscribed, no "likely"
    known.insert(QStringLiteral("url"), QStringLiteral("https://likely.example.org/atom"));
    f.setKnownFeeds({ known });
    CHECK(f.feeds().size() == 1 && f.feeds().at(0).toMap().value(QStringLiteral("subscribed")).toBool());
    CHECK(f.feeds().size() == 1 && f.feeds().at(0).toMap().value(QStringLiteral("likelySubscribedAs")).toString().isEmpty());
    // the same articles as a subscribed feed: "likely" (other address and title)
    QVariantMap same;
    same.insert(QStringLiteral("url"), QStringLiteral("https://gadget.example.com/rss.xml"));
    same.insert(QStringLiteral("link"), QStringLiteral("https://gadget.example.com/"));
    same.insert(QStringLiteral("title"), QStringLiteral("Gadgets"));
    same.insert(QStringLiteral("articleTitles"), QVariantList{ QStringLiteral("The first gadget of the week"), QStringLiteral("Another & better gadget") });
    f.setKnownFeeds({ same });
    r = runFinder(f, base + QStringLiteral("/same-items.xml"));
    CHECK(r.size() == 1 && r.at(0).toMap().value(QStringLiteral("likelySubscribedAs")).toString() == QStringLiteral("Gadgets"));
    same.insert(QStringLiteral("articleTitles"), QVariantList{ QStringLiteral("The first gadget of the week") });     // one hit is not enough
    f.setKnownFeeds({ same });
    CHECK(f.feeds().size() == 1 && f.feeds().at(0).toMap().value(QStringLiteral("likelySubscribedAs")).toString().isEmpty());
    f.setKnownFeeds({});
    CHECK(!f.busy());
}

static void testAges()
{
    const QDateTime now = QDateTime::currentDateTime();
    CHECK_EQ(ArticleModel::ageText(now.addSecs(-20), now), QStringLiteral("now"));
    CHECK_EQ(ArticleModel::ageText(now.addSecs(-6 * 60), now), QStringLiteral("6 min"));
    CHECK_EQ(ArticleModel::ageText(now.addSecs(-3 * 3600), now), QStringLiteral("3 h"));
    CHECK_EQ(ArticleModel::ageText(now.addDays(-2), now), QStringLiteral("2 d"));
    CHECK_EQ(ArticleModel::dayText(now, now), QStringLiteral("Today"));
    CHECK_EQ(ArticleModel::dayText(now.addDays(-1), now), QStringLiteral("Yesterday"));
}

// WebDAV access to the user's Nextcloud files (settings backup, package 5)
static void testCloud(Configuration *config, const QString &port)
{
    // Multistatus as Nextcloud sends it: the folder itself, files, a
    // subfolder; missing properties come back empty in a 404 propstat.
    const QByteArray xml =
        "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\">"
        "<d:response><d:href>/remote.php/dav/files/t%20s/Sailfeed/</d:href><d:propstat><d:prop>"
        "<d:getlastmodified>Sat, 10 Oct 2026 10:00:00 GMT</d:getlastmodified><d:resourcetype><d:collection/></d:resourcetype>"
        "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>"
        "<d:propstat><d:prop><d:getcontentlength/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response>"
        "<d:response><d:href>/remote.php/dav/files/t%20s/Sailfeed/old%20f%C3%BCr.json</d:href><d:propstat><d:prop>"
        "<d:getlastmodified>Fri, 9 Oct 2026 08:30:00 GMT</d:getlastmodified><d:getcontentlength>12</d:getcontentlength>"
        "<d:getetag>&quot;abc&quot;</d:getetag><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "<d:response><d:href>/remote.php/dav/files/t%20s/Sailfeed/new.json</d:href><d:propstat><d:prop>"
        "<d:getlastmodified>Sat, 10 Oct 2026 09:15:00 GMT</d:getlastmodified><d:getcontentlength>34</d:getcontentlength>"
        "<d:getetag>W/&quot;def&quot;</d:getetag><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "<d:response><d:href>/remote.php/dav/files/t%20s/Sailfeed/sub/</d:href><d:propstat><d:prop>"
        "<d:resourcetype><d:collection/></d:resourcetype></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "</d:multistatus>";
    const QList<NextcloudFiles::Entry> parsed = NextcloudFiles::parseMultistatus(xml);
    CHECK_EQ(parsed.size(), 2);
    if (parsed.size() == 2) {
        CHECK_EQ(parsed.at(0).name, QStringLiteral("new.json"));             // newest first
        CHECK_EQ(parsed.at(0).size, 34);
        CHECK_EQ(parsed.at(0).etag, QStringLiteral("def"));
        CHECK_EQ(parsed.at(0).modified, QDateTime(QDate(2026, 10, 10), QTime(9, 15), Qt::UTC));
        CHECK_EQ(parsed.at(1).name, QStringLiteral("old für.json"));
        CHECK_EQ(parsed.at(1).modified, QDateTime(QDate(2026, 10, 9), QTime(8, 30), Qt::UTC));
    }
    CHECK(NextcloudFiles::parseMultistatus("no xml").isEmpty());
    CHECK(NextcloudFiles::describe(0, 507, QString()).contains(QStringLiteral("storage space")));
    CHECK(NextcloudFiles::describe(0, 401, QString()).startsWith(QStringLiteral("Sign-in failed")));
    CHECK(NextcloudFiles::describe(QNetworkReply::OperationCanceledError, 0, QString()).startsWith(QStringLiteral("Server not reachable")));

    NextcloudFiles files(config);
    files.setTimeout(5000);
    auto run = [&](const std::function<void(const NextcloudFiles::Callback &)> &op) {
        NextcloudFiles::Result res;
        bool done = false;
        op([&](const NextcloudFiles::Result &r) { res = r; done = true; });
        CHECK(waitFor([&] { return done; }));
        return res;
    };

    // The user ID comes from the server and differs from the login name
    files.clearUserId();
    CHECK(files.userId().isEmpty());
    CHECK(files.folderUrl().isEmpty());
    NextcloudFiles::Result r = run([&](const NextcloudFiles::Callback &cb) { files.resolveUserId(cb); });
    CHECK(r.ok);
    CHECK_EQ(files.userId(), QStringLiteral("tsappl"));
    CHECK_EQ(files.folderUrl().toString(),
             QStringLiteral("http://127.0.0.1:%1/remote.php/dav/files/tsappl/Sailfeed/").arg(port));
    {
        NextcloudFiles again(config);                                    // cached in the settings
        CHECK_EQ(again.userId(), QStringLiteral("tsappl"));
    }
    config->setUsername(QStringLiteral("someone-else"));                 // other account: cache invalid
    CHECK(files.userId().isEmpty());
    config->setUsername(QStringLiteral("thomas"));
    CHECK_EQ(files.userId(), QStringLiteral("tsappl"));

    // No folder yet: an empty list, not an error
    r = run([&](const NextcloudFiles::Callback &cb) { files.list(cb); });
    CHECK(r.ok);
    CHECK(r.entries.isEmpty());

    // Uploading creates the folder; content round trip with umlauts
    const QByteArray json = QStringLiteral("{\"place\":\"Kärnten\"}").toUtf8();
    r = run([&](const NextcloudFiles::Callback &cb) { files.upload(QStringLiteral("sailfeed-settings.json"), json, cb); });
    CHECK(r.ok);
    CHECK(!r.etag.isEmpty());
    const QString etag = r.etag;
    r = run([&](const NextcloudFiles::Callback &cb) { files.download(QStringLiteral("sailfeed-settings.json"), cb); });
    CHECK(r.ok);
    CHECK_EQ(r.data, json);
    CHECK_EQ(r.etag, etag);
    r = run([&](const NextcloudFiles::Callback &cb) { files.ensureFolder(cb); });
    CHECK(r.ok);                                                         // exists already

    r = run([&](const NextcloudFiles::Callback &cb) { files.upload(QStringLiteral("sailfeed-settings-2026-10-09.json"), "{}", cb); });
    CHECK(r.ok);
    r = run([&](const NextcloudFiles::Callback &cb) { files.list(cb); });
    CHECK(r.ok);
    CHECK_EQ(r.entries.size(), 2);
    QStringList names;
    for (const auto &e : r.entries)
        names << e.name;
    names.sort();
    CHECK_EQ(names.join(QLatin1Char(',')), QStringLiteral("sailfeed-settings-2026-10-09.json,sailfeed-settings.json"));
    for (const auto &e : r.entries) {
        CHECK(e.modified.isValid());
        if (e.name == QStringLiteral("sailfeed-settings.json"))
            CHECK_EQ(e.size, json.size());
    }

    // Delete; deleting again is fine; a missing file is "not found"
    r = run([&](const NextcloudFiles::Callback &cb) { files.remove(QStringLiteral("sailfeed-settings-2026-10-09.json"), cb); });
    CHECK(r.ok);
    r = run([&](const NextcloudFiles::Callback &cb) { files.remove(QStringLiteral("sailfeed-settings-2026-10-09.json"), cb); });
    CHECK(r.ok);
    r = run([&](const NextcloudFiles::Callback &cb) { files.download(QStringLiteral("sailfeed-settings-2026-10-09.json"), cb); });
    CHECK(!r.ok);
    CHECK(r.notFound());
    CHECK(!r.errorString.isEmpty());
    r = run([&](const NextcloudFiles::Callback &cb) { files.list(cb); });
    CHECK_EQ(r.entries.size(), 1);

    // Folder and file names are encoded
    files.setFolderName(QStringLiteral("Sailfeed Test ä"));
    r = run([&](const NextcloudFiles::Callback &cb) { files.upload(QStringLiteral("a b.json"), "1", cb); });
    CHECK(r.ok);
    r = run([&](const NextcloudFiles::Callback &cb) { files.list(cb); });
    CHECK(r.ok && r.entries.size() == 1 && r.entries.at(0).name == QStringLiteral("a b.json"));
    files.setFolderName(QStringLiteral("Sailfeed"));

    // Wrong app password: understandable error
    config->setPassword(QStringLiteral("wrong"));
    r = run([&](const NextcloudFiles::Callback &cb) { files.download(QStringLiteral("sailfeed-settings.json"), cb); });
    CHECK(!r.ok);
    CHECK_EQ(r.httpStatus, 401);
    CHECK(r.errorString.startsWith(QStringLiteral("Sign-in failed")));
    config->setPassword(QStringLiteral("secret"));
}

// Settings backup file: format, validation, restore modes (package 5b)
static void testSettingsBackup()
{
    SettingsBackup b;
    CHECK(b.generalKeys().contains(QStringLiteral("readerColors")));
    CHECK(b.generalKeys().contains(QStringLiteral("linksInApp")));
    CHECK(b.generalKeys().contains(QStringLiteral("listView")));            // 1.2: compact list
    CHECK(b.generalKeys().contains(QStringLiteral("deleteReadAfter")));     // deleting old read articles
    CHECK(b.generalKeys().contains(QStringLiteral("imageCacheMB")));
    CHECK(b.generalKeys().contains(QStringLiteral("oldestFirst")));
    CHECK(b.textSizeKeys().contains(QStringLiteral("readerFontPerFeed")));
    CHECK(!b.generalKeys().contains(QStringLiteral("readerFontPx")));      // a text size
    CHECK(!b.deviceName().isEmpty());

    // Feed settings (1.2): per feed ID, validated, merged by "add missing"
    {
        QVariantMap in, st0;
        in.insert(QStringLiteral("5"), 1.0);
        in.insert(QStringLiteral("6"), 7.0);                    // out of range: dropped
        in.insert(QStringLiteral("x"), 1.0);                    // no feed ID: dropped
        st0.insert(QStringLiteral("feedOpenMode"), in);
        const QVariantMap back = b.parse(b.serialize(st0, false)).value(QStringLiteral("state")).toMap();
        CHECK_EQ(back.value(QStringLiteral("feedOpenMode")).toMap().size(), 1);
        CHECK_EQ(back.value(QStringLiteral("feedOpenMode")).toMap().value(QStringLiteral("5")).toInt(), 1);
        CHECK(b.generalKeys().contains(QStringLiteral("feedSkipPreview")));
        QVariantMap cur, bak, curMap, bakMap;
        curMap.insert(QStringLiteral("5"), 1);
        bakMap.insert(QStringLiteral("5"), 0);
        bakMap.insert(QStringLiteral("9"), 1);
        cur.insert(QStringLiteral("feedListView"), curMap);
        bak.insert(QStringLiteral("feedListView"), bakMap);
        QVariantMap pl = b.plan(cur, bak, SettingsBackup::AddMissing, false);
        const QVariantMap merged = pl.value(QStringLiteral("changes")).toMap().value(QStringLiteral("feedListView")).toMap();
        CHECK_EQ(merged.value(QStringLiteral("5")).toInt(), 1);   // own value kept
        CHECK_EQ(merged.value(QStringLiteral("9")).toInt(), 1);   // missing one added
        pl = b.plan(cur, bak, SettingsBackup::Replace, false);
        CHECK_EQ(pl.value(QStringLiteral("changes")).toMap().value(QStringLiteral("feedListView")).toMap(), bakMap);
        pl = b.plan(cur, cur, SettingsBackup::AddMissing, false);
        CHECK(!pl.value(QStringLiteral("changes")).toMap().contains(QStringLiteral("feedListView")));
    }

    auto fav = [](qint64 id, const QString &title) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("title"), title);
        return QVariant(m);
    };
    QVariantMap perFeedPreview;
    perFeedPreview.insert(QStringLiteral("10"), 2.0);          // numbers arrive as double from QML
    QVariantMap phone;
    phone.insert(QStringLiteral("autoReadDelay"), 4.0);
    phone.insert(QStringLiteral("autoReadMode"), 1);
    phone.insert(QStringLiteral("readerColors"), 3);
    phone.insert(QStringLiteral("readerJustify"), true);
    phone.insert(QStringLiteral("shareBrowserCookies"), false);
    phone.insert(QStringLiteral("readerFontPx"), 22);
    phone.insert(QStringLiteral("previewFontStep"), 1);
    phone.insert(QStringLiteral("textSizePerFeed"), true);
    phone.insert(QStringLiteral("previewFontPerFeed"), perFeedPreview);
    phone.insert(QStringLiteral("readerFontPerFeed"), QVariantMap());
    phone.insert(QStringLiteral("favoriteFeeds"), QVariantList{ fav(10, QStringLiteral("Golem.de")), fav(11, QStringLiteral("Android Police")) });
    phone.insert(QStringLiteral("originalSites"), QStringList{ QStringLiteral("Golem.de"), QStringLiteral("heise.de") });
    phone.insert(QStringLiteral("somethingElse"), 5);                     // not part of the backup

    const QDateTime when(QDate(2026, 10, 10), QTime(11, 0), Qt::UTC);
    const QString withSizes = b.serialize(phone, true, when);
    const QString without = b.serialize(phone, false, when);
    CHECK(withSizes.contains(QStringLiteral("\"format\": \"sailfeed-settings\"")));
    CHECK(withSizes.contains(QStringLiteral("2026-10-10T11:00:00Z")));
    CHECK(withSizes.contains(QStringLiteral("textSizes")));
    CHECK(!without.contains(QStringLiteral("textSizes")));
    CHECK(!without.contains(QStringLiteral("somethingElse")));

    QVariantMap parsed = b.parse(withSizes);
    CHECK(parsed.value(QStringLiteral("ok")).toBool());
    CHECK(parsed.value(QStringLiteral("hasTextSizes")).toBool());
    CHECK_EQ(parsed.value(QStringLiteral("savedAt")).toDateTime(), when);
    QVariantMap st = parsed.value(QStringLiteral("state")).toMap();
    CHECK_EQ(st.value(QStringLiteral("autoReadDelay")).toInt(), 4);
    CHECK_EQ(st.value(QStringLiteral("readerColors")).toInt(), 3);
    CHECK_EQ(st.value(QStringLiteral("readerFontPx")).toInt(), 22);
    CHECK_EQ(st.value(QStringLiteral("previewFontPerFeed")).toMap().value(QStringLiteral("10")).toInt(), 2);
    CHECK_EQ(st.value(QStringLiteral("originalSites")).toStringList().join(QLatin1Char(',')), QStringLiteral("golem.de,heise.de"));
    CHECK_EQ(st.value(QStringLiteral("favoriteFeeds")).toList().size(), 2);
    QVariantMap parsedNoSizes = b.parse(without);
    CHECK(!parsedNoSizes.value(QStringLiteral("hasTextSizes")).toBool());
    CHECK(!parsedNoSizes.value(QStringLiteral("state")).toMap().contains(QStringLiteral("readerFontPx")));

    // Invalid files and values
    CHECK(!b.parse(QStringLiteral("{}")).value(QStringLiteral("ok")).toBool());
    CHECK(!b.parse(QStringLiteral("not json")).value(QStringLiteral("error")).toString().isEmpty());
    CHECK(b.parse(QStringLiteral("{\"format\":\"sailfeed-settings\",\"version\":2}")).value(QStringLiteral("error"))
          .toString().contains(QStringLiteral("newer")));
    const QVariantMap bad = b.parse(QStringLiteral(
        "{\"format\":\"sailfeed-settings\",\"version\":1,\"settings\":{\"readerColors\":9,\"autoReadDelay\":2.5,"
        "\"readerSerif\":\"yes\",\"openMode\":2},\"textSizes\":{\"readerFontPx\":99,\"readerFontPerFeed\":{\"x\":20,\"12\":40,\"13\":18}},"
        "\"favoriteFeeds\":[{\"id\":-1},{\"id\":12},12,\"13\"],\"originalSites\":[\"\",\"A.org\",\"a.org\"]}"))
        .value(QStringLiteral("state")).toMap();
    CHECK(!bad.contains(QStringLiteral("readerColors")));        // out of range
    CHECK(!bad.contains(QStringLiteral("autoReadDelay")));       // not a whole number
    CHECK(!bad.contains(QStringLiteral("readerSerif")));         // wrong type
    CHECK_EQ(bad.value(QStringLiteral("openMode")).toInt(), 2);
    CHECK(!bad.contains(QStringLiteral("readerFontPx")));
    CHECK_EQ(bad.value(QStringLiteral("readerFontPerFeed")).toMap().size(), 1);   // only "13": 18
    CHECK_EQ(bad.value(QStringLiteral("favoriteFeeds")).toList().size(), 2);      // 12 and 13
    CHECK_EQ(bad.value(QStringLiteral("originalSites")).toStringList().size(), 1);

    // A tablet with other settings restores the phone's backup
    QVariantMap tablet;
    tablet.insert(QStringLiteral("autoReadDelay"), 4);
    tablet.insert(QStringLiteral("autoReadMode"), 0);
    tablet.insert(QStringLiteral("readerColors"), 0);
    tablet.insert(QStringLiteral("readerJustify"), true);
    tablet.insert(QStringLiteral("shareBrowserCookies"), true);
    tablet.insert(QStringLiteral("readerFontPx"), 26);
    tablet.insert(QStringLiteral("previewFontStep"), 3);
    tablet.insert(QStringLiteral("textSizePerFeed"), true);
    QVariantMap tabletPerFeed;
    tabletPerFeed.insert(QStringLiteral("10"), 4);
    tabletPerFeed.insert(QStringLiteral("12"), 5);
    tablet.insert(QStringLiteral("previewFontPerFeed"), tabletPerFeed);
    tablet.insert(QStringLiteral("readerFontPerFeed"), QVariantMap());
    tablet.insert(QStringLiteral("favoriteFeeds"), QVariantList{ fav(11, QStringLiteral("Android Police")), fav(12, QStringLiteral("Barihunks")) });
    tablet.insert(QStringLiteral("originalSites"), QStringList{ QStringLiteral("example.org") });

    // Replace, without text sizes
    QVariantMap p = b.plan(tablet, st, SettingsBackup::Replace, false);
    QVariantMap ch = p.value(QStringLiteral("changes")).toMap();
    CHECK(p.value(QStringLiteral("hasChanges")).toBool());
    CHECK_EQ(p.value(QStringLiteral("settingsChanged")).toInt(), 3);     // autoReadMode, readerColors, shareBrowserCookies
    CHECK_EQ(ch.value(QStringLiteral("readerColors")).toInt(), 3);
    CHECK_EQ(ch.value(QStringLiteral("shareBrowserCookies")).toBool(), false);
    CHECK(!ch.contains(QStringLiteral("autoReadDelay")));                 // same value
    CHECK(!ch.contains(QStringLiteral("readerFontPx")));                  // text sizes not chosen
    CHECK(!p.value(QStringLiteral("textSizesChanged")).toBool());
    CHECK_EQ(p.value(QStringLiteral("favoriteFeeds")).toList().size(), 2);
    CHECK_EQ(p.value(QStringLiteral("favoritesAdded")).toStringList(), QStringList{ QStringLiteral("Golem.de") });
    CHECK_EQ(p.value(QStringLiteral("favoritesRemoved")).toStringList(), QStringList{ QStringLiteral("Barihunks") });
    CHECK_EQ(p.value(QStringLiteral("originalSites")).toStringList().join(QLatin1Char(',')), QStringLiteral("golem.de,heise.de"));
    CHECK_EQ(p.value(QStringLiteral("sitesRemoved")).toStringList(), QStringList{ QStringLiteral("example.org") });

    // Replace, with text sizes: sizes and per-feed maps from the backup
    p = b.plan(tablet, st, SettingsBackup::Replace, true);
    ch = p.value(QStringLiteral("changes")).toMap();
    CHECK(p.value(QStringLiteral("textSizesChanged")).toBool());
    CHECK_EQ(ch.value(QStringLiteral("readerFontPx")).toInt(), 22);
    CHECK_EQ(ch.value(QStringLiteral("previewFontPerFeed")).toMap().size(), 1);
    CHECK(!ch.contains(QStringLiteral("readerFontPerFeed")));            // both empty

    // Add missing only: nothing replaced, favorites/sites/per-feed sizes merged
    p = b.plan(tablet, st, SettingsBackup::AddMissing, true);
    ch = p.value(QStringLiteral("changes")).toMap();
    CHECK_EQ(p.value(QStringLiteral("settingsChanged")).toInt(), 0);
    CHECK(!ch.contains(QStringLiteral("readerColors")));
    CHECK(!ch.contains(QStringLiteral("readerFontPx")));
    CHECK(!ch.contains(QStringLiteral("previewFontPerFeed")));           // feed 10 has its own size already
    CHECK_EQ(p.value(QStringLiteral("favoritesAdded")).toStringList(), QStringList{ QStringLiteral("Golem.de") });
    CHECK(p.value(QStringLiteral("favoritesRemoved")).toStringList().isEmpty());
    CHECK_EQ(p.value(QStringLiteral("favoriteFeeds")).toList().size(), 3);
    CHECK_EQ(p.value(QStringLiteral("originalSites")).toStringList().join(QLatin1Char(',')),
             QStringLiteral("example.org,golem.de,heise.de"));
    CHECK(p.value(QStringLiteral("sitesRemoved")).toStringList().isEmpty());
    QVariantMap phonePerFeed;
    phonePerFeed.insert(QStringLiteral("11"), 6);
    st.insert(QStringLiteral("previewFontPerFeed"), phonePerFeed);
    p = b.plan(tablet, st, SettingsBackup::AddMissing, true);
    const QVariantMap merged = p.value(QStringLiteral("changes")).toMap().value(QStringLiteral("previewFontPerFeed")).toMap();
    CHECK_EQ(merged.size(), 3);                                           // 10 and 12 kept, 11 added
    CHECK_EQ(merged.value(QStringLiteral("10")).toInt(), 4);
    CHECK_EQ(merged.value(QStringLiteral("11")).toInt(), 6);

    // Fingerprint: what a backup contains, without titles, order or date
    const QString fp = b.fingerprint(phone, true);
    CHECK_EQ(fp.size(), 16);
    QVariantMap phone2 = phone;
    phone2.insert(QStringLiteral("favoriteFeeds"), QVariantList{ fav(11, QStringLiteral("renamed")), fav(10, QString()) });
    phone2.insert(QStringLiteral("originalSites"), QStringList{ QStringLiteral("heise.de"), QStringLiteral("golem.de") });
    phone2.insert(QStringLiteral("somethingElse"), 6);
    CHECK_EQ(b.fingerprint(phone2, true), fp);
    phone2.insert(QStringLiteral("readerColors"), 2);
    CHECK(b.fingerprint(phone2, true) != fp);
    CHECK(b.fingerprint(phone, false) != fp);
    QVariantMap phone3 = phone;
    phone3.insert(QStringLiteral("readerFontPx"), 30);                   // a text size
    CHECK(b.fingerprint(phone3, true) != fp);
    CHECK_EQ(b.fingerprint(phone3, false), b.fingerprint(phone, false));

    // Restoring the own state changes nothing
    p = b.plan(phone, b.parse(withSizes).value(QStringLiteral("state")).toMap(), SettingsBackup::Replace, true);
    CHECK(!p.value(QStringLiteral("hasChanges")).toBool());
}

// Backup and restore files on the Nextcloud (package 5c)
static void testCloudBackup(Configuration *config)
{
    CHECK(CloudBackup::isBackupFile(QStringLiteral("sailfeed-settings.json")));
    CHECK(CloudBackup::isBackupFile(QStringLiteral("sailfeed-settings-2026-10-10.json")));
    CHECK(!CloudBackup::isBackupFile(QStringLiteral("sailfeed-settings-x.json")));
    CHECK(!CloudBackup::isBackupFile(QStringLiteral("notes.txt")));

    CloudBackup cb(config);
    cb.files()->setTimeout(5000);
    bool done = false, okResult = false;
    QString message;
    QObject::connect(&cb, &CloudBackup::backupFinished, [&](bool ok, const QString &m) { done = true; okResult = ok; message = m; });
    QObject::connect(&cb, &CloudBackup::refreshed, [&](bool ok, const QString &m) { done = true; okResult = ok; message = m; });
    QString fetchedName, fetchedContent;
    QObject::connect(&cb, &CloudBackup::fetched, [&](const QString &n, bool ok, const QString &c, const QString &m) {
        done = true; okResult = ok; fetchedName = n; fetchedContent = c; message = m; });

    // Earlier states are kept as copies (device test 11 Oct: one copy per
    // day always equalled the latest backup, the previous settings were gone)
    CHECK_EQ(CloudBackup::copyName(QDateTime(QDate(2026, 10, 11), QTime(15, 30, 12), Qt::LocalTime)),
             QStringLiteral("sailfeed-settings-2026-10-11_153012.json"));
    CHECK(CloudBackup::isBackupFile(QStringLiteral("sailfeed-settings-2026-10-11_153012.json")));
    auto pause = [](int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20); };
    auto names = [&]() {
        QStringList out;
        for (const QVariant &v : cb.backups())
            out << v.toMap().value(QStringLiteral("name")).toString();
        return out;
    };
    auto contentOf = [&](const QString &name) {
        done = false;
        fetchedContent.clear();
        CHECK(cb.fetch(name));
        CHECK(waitFor([&] { return done; }));
        return fetchedContent;
    };
    auto backUp = [&](const QString &json) {
        done = false;
        CHECK(cb.backup(json));
        CHECK(cb.busy());
        CHECK(!cb.backup(QStringLiteral("{}")));                            // one operation at a time
        CHECK(waitFor([&] { return done; }));
        CHECK(okResult);
    };
    {
        bool removed = false;                                               // start without backups
        cb.files()->remove(SettingsBackup::currentFileName(), [&](const NextcloudFiles::Result &) { removed = true; });
        CHECK(waitFor([&] { return removed; }));
    }
    cb.setMaxCopies(3);
    cb.setCopyInterval(0);                                                  // a copy before every backup
    backUp(QStringLiteral("{\"s\":1}"));
    CHECK_EQ(names(), QStringList{ SettingsBackup::currentFileName() });    // nothing to keep yet
    for (int i = 2; i <= 5; ++i) {
        pause(1100);                                                        // copies are named by the second
        backUp(QStringLiteral("{\"s\":%1}").arg(i));
    }
    CHECK(!cb.busy());
    CHECK(cb.lastBackup().isValid());
    CHECK(cb.listed());
    QStringList n = names();
    CHECK_EQ(n.size(), 4);                                                  // latest + 3 copies (s1 pruned)
    CHECK(cb.backups().value(0).toMap().value(QStringLiteral("current")).toBool());
    CHECK(cb.backups().value(1).toMap().value(QStringLiteral("stateTime")).toDateTime().isValid());
    if (n.size() == 4) {
        CHECK_EQ(contentOf(n.at(0)), QStringLiteral("{\"s\":5}"));
        CHECK_EQ(contentOf(n.at(1)), QStringLiteral("{\"s\":4}"));        // the state before the last change
        CHECK_EQ(contentOf(n.at(2)), QStringLiteral("{\"s\":3}"));
        CHECK_EQ(contentOf(n.at(3)), QStringLiteral("{\"s\":2}"));
        CHECK(n.at(1) > n.at(2) && n.at(2) > n.at(3));                       // newest first
    }
    // At most one copy per interval: right after a copy, the next backup makes none
    cb.setCopyInterval(3600);
    backUp(QStringLiteral("{\"s\":6}"));
    CHECK_EQ(names().size(), 4);
    CHECK_EQ(contentOf(names().at(1)), QStringLiteral("{\"s\":4}"));
    CHECK_EQ(contentOf(SettingsBackup::currentFileName()), QStringLiteral("{\"s\":6}"));

    // List again (other files in the folder are ignored) and fetch
    {
        bool up = false;
        cb.files()->upload(QStringLiteral("notes.txt"), "x", [&](const NextcloudFiles::Result &) { up = true; });
        CHECK(waitFor([&] { return up; }));
    }
    done = false;
    CHECK(cb.refresh());
    CHECK(waitFor([&] { return done; }));
    CHECK(okResult);
    CHECK_EQ(names().size(), 4);
    CHECK(!cb.fetch(QStringLiteral("notes.txt")));                           // not a backup
    done = false;
    CHECK(cb.fetch(QStringLiteral("sailfeed-settings-2026-10-01_000000.json")));   // not there
    CHECK(waitFor([&] { return done; }));
    CHECK(!okResult);
    CHECK(!message.isEmpty());

    // Local copy before restoring
    CHECK(!cb.localCopyTime().isValid());
    cb.saveLocalCopy(QStringLiteral("{\"before\":true}"));
    CHECK(cb.localCopyTime().isValid());
    CHECK_EQ(cb.localCopy(), QStringLiteral("{\"before\":true}"));

    // Wrong app password: the backup fails with a hint
    config->setPassword(QStringLiteral("wrong"));
    done = false;
    CHECK(cb.backup(QStringLiteral("{}")));
    CHECK(waitFor([&] { return done; }));
    CHECK(!okResult);
    CHECK(message.startsWith(QStringLiteral("Sign-in failed")));
    CHECK(!cb.busy());
    config->setPassword(QStringLiteral("secret"));
}

// Automatic backup and linking a device to the account (packages 5d, 5e)
static void testAutoBackup(Configuration *config)
{
    CloudBackup cb(config);
    cb.files()->setTimeout(5000);
    cb.setAutoDelay(200);
    cb.setRetryDelay(600);
    int requests = 0, offers = 0;
    bool done = false, okResult = false;
    QObject::connect(&cb, &CloudBackup::autoBackupRequested, [&] { ++requests; });
    QObject::connect(&cb, &CloudBackup::restoreOffer, [&] { ++offers; });
    QObject::connect(&cb, &CloudBackup::backupFinished, [&](bool ok, const QString &) { done = true; okResult = ok; });
    auto backUp = [&](const QString &fp) {
        done = false;
        CHECK(cb.backup(QStringLiteral("{\"fp\":\"%1\"}").arg(fp), fp));
        CHECK(waitFor([&] { return done; }));
        return okResult;
    };
    auto pause = [](int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20); };

    CHECK(cb.linked());                                     // backed up by hand before (testCloudBackup)
    CHECK(!cb.pending());                                   // nothing noted yet
    cb.noteSettings(QStringLiteral("fp1"));
    CHECK(cb.pending());
    CHECK(waitFor([&] { return requests == 1; }));
    CHECK(backUp(QStringLiteral("fp1")));
    CHECK(!cb.pending());
    CHECK(cb.lastError().isEmpty());
    cb.noteSettings(QStringLiteral("fp1"));                 // no change: nothing to do
    pause(400);
    CHECK_EQ(requests, 1);

    // Several changes in a row: one backup after the last
    cb.noteSettings(QStringLiteral("fp2"));
    pause(100);
    cb.noteSettings(QStringLiteral("fp3"));
    pause(100);
    CHECK_EQ(requests, 1);
    CHECK(waitFor([&] { return requests == 2; }));
    pause(300);
    CHECK_EQ(requests, 2);
    CHECK(backUp(QStringLiteral("fp3")));

    // App goes to the background: at once
    cb.noteSettings(QStringLiteral("fp4"));
    cb.flush();
    CHECK_EQ(requests, 3);
    CHECK(backUp(QStringLiteral("fp4")));
    cb.flush();                                             // nothing pending
    CHECK_EQ(requests, 3);

    // Back to the backed-up settings before the delay: nothing to do
    cb.noteSettings(QStringLiteral("fp5"));
    cb.noteSettings(QStringLiteral("fp4"));
    pause(400);
    CHECK_EQ(requests, 3);

    // Automatic backup off
    cb.setAutoEnabled(false);
    cb.noteSettings(QStringLiteral("fp6"));
    cb.flush();
    pause(400);
    CHECK_EQ(requests, 3);
    CHECK(cb.pending());
    cb.setAutoEnabled(true);
    CHECK(waitFor([&] { return requests == 4; }));

    // Failed backup: error kept, tried again later
    config->setPassword(QStringLiteral("wrong"));
    CHECK(!backUp(QStringLiteral("fp6")));
    CHECK(!cb.lastError().isEmpty());
    CHECK(cb.pending());
    config->setPassword(QStringLiteral("secret"));
    CHECK(waitFor([&] { return requests == 5; }));          // retry delay
    CHECK(backUp(QStringLiteral("fp6")));
    CHECK(cb.lastError().isEmpty());

    // Another account is not linked
    config->setUsername(QStringLiteral("someone-else"));
    CHECK(!cb.linked());
    config->setUsername(QStringLiteral("thomas"));
    CHECK(cb.linked());

    // A new device (not linked) with a backup on the server: offer, no automatic backup
    QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).remove(QStringLiteral("backup/linkedAccount"));
    CHECK(!cb.linked());
    cb.noteSettings(QStringLiteral("fp7"));
    cb.flush();
    pause(400);
    CHECK_EQ(requests, 5);
    cb.checkLink();
    CHECK(waitFor([&] { return offers == 1; }));
    CHECK(!cb.linked());
    CHECK(cb.listed());
    cb.setLinked();                                         // answered (restored or declined)
    CHECK(cb.linked());
    CHECK(waitFor([&] { return requests == 6; }));          // now its settings are backed up
    CHECK(backUp(QStringLiteral("fp7")));
    cb.checkLink();                                         // linked: nothing to do
    pause(300);
    CHECK_EQ(offers, 1);

    // A new device and no backup on the server yet: linked at once
    {
        bool removed = false;
        cb.files()->remove(SettingsBackup::currentFileName(), [&](const NextcloudFiles::Result &) { removed = true; });
        CHECK(waitFor([&] { return removed; }));
    }
    QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).remove(QStringLiteral("backup/linkedAccount"));
    cb.checkLink();
    CHECK(waitFor([&] { return cb.linked(); }));
    CHECK_EQ(offers, 1);
}


// Deleting old read articles (ArticleCleaner) on a database with libfuoten's schema
static void testCleaner()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString db = dir.path() + QStringLiteral("/database.sqlite");
    const QString ini = dir.path() + QStringLiteral("/settings.ini");

    // Defaults: new installation 3 months, update "never" plus the hint
    ArticleCleaner::initDefault(ini, false);
    CHECK_EQ(QSettings(ini, QSettings::IniFormat).value(QStringLiteral("storage/deleteReadAfterMonths")).toInt(), 3);
    ArticleCleaner::initDefault(ini, true);     // already decided: unchanged
    CHECK_EQ(QSettings(ini, QSettings::IniFormat).value(QStringLiteral("storage/deleteReadAfterMonths")).toInt(), 3);
    const QString ini2 = dir.path() + QStringLiteral("/update.ini");
    ArticleCleaner::initDefault(ini2, true);
    {
        ArticleCleaner c;
        c.setPaths(db, ini2);
        CHECK_EQ(c.deleteAfterMonths(), 0);
        CHECK(c.hintPending());
        CHECK(!ArticleCleaner::isDue(ini2, QDate::currentDate()));
        c.dismissHint();
        CHECK(!c.hintPending());
    }

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 nowSecs = now.toMSecsSinceEpoch() / 1000;
    const qint64 old = nowSecs - 200LL * 24 * 3600;     // > 6 months
    const qint64 recent = nowSecs - 10LL * 24 * 3600;
    {
        QSqlDatabase d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("cleanertest"));
        d.setDatabaseName(db);
        CHECK(d.open());
        QSqlQuery q(d);
        // libfuoten's tables (SQLiteStorage itself keeps one global
        // connection "fuotendb" with its first path, so it cannot be used
        // for a second database here)
        CHECK(q.exec(QStringLiteral("CREATE TABLE feeds (id INTEGER PRIMARY KEY NOT NULL, folderId INTEGER NOT NULL, title TEXT NOT NULL, url TEXT NOT NULL, "
                                    "link TEXT NOT NULL, added INTEGER NOT NULL, unreadCount INTEGER DEFAULT 0, ordering INTEGER NOT NULL, pinned INTEGER NOT NULL, "
                                    "updateErrorCount INTEGER NOT NULL, lastUpdateError TEXT, faviconLink TEXT)")));
        CHECK(q.exec(QStringLiteral("CREATE TABLE items (id INTEGER PRIMARY KEY NOT NULL, feedId INTEGER NOT NULL, guid TEXT NOT NULL, guidHash TEXT NOT NULL, "
                                    "url TEXT NOT NULL, title TEXT NOT NULL, author TEXT NOT NULL, pubDate INTEGER NOT NULL, body TEXT NOT NULL, enclosureMime TEXT, "
                                    "enclosureLink TEXT, unread INTEGER NOT NULL, starred INTEGER NOT NULL, lastModified INTEGER NOT NULL, fingerprint TEXT NOT NULL, "
                                    "queue INTEGER DEFAULT 0, rtl INTEGER NOT NULL DEFAULT 0, mediaThumbnail TEXT, mediaDescription TEXT, "
                                    "FOREIGN KEY(feedId) REFERENCES feeds(id) ON DELETE CASCADE)")));
        CHECK(q.exec(QStringLiteral("INSERT INTO feeds (id, folderId, title, url, link, added, ordering, pinned, updateErrorCount) VALUES (1, 0, 'F', 'u', 'l', 0, 0, 0, 0)")));
        auto add = [&](int id, qint64 pub, int unread, int starred, int queue) {
            QSqlQuery i(d);
            i.prepare(QStringLiteral("INSERT INTO items (id, feedId, guid, guidHash, url, title, author, pubDate, body, unread, starred, lastModified, fingerprint, queue) "
                                     "VALUES (?, 1, 'g', ?, 'u', 't', 'a', ?, 'b', ?, ?, 0, 'f', ?)"));
            i.addBindValue(id); i.addBindValue(QString::number(id)); i.addBindValue(pub);
            i.addBindValue(unread); i.addBindValue(starred); i.addBindValue(queue);
            return i.exec();
        };
        CHECK(add(1, old, 0, 0, 0));        // read, old -> deleted
        CHECK(add(2, old, 0, 1, 0));        // bookmark -> stays
        CHECK(add(3, old, 1, 0, 0));        // unread -> stays
        CHECK(add(4, old, 0, 0, 1));        // change not sent yet -> stays
        CHECK(add(5, recent, 0, 0, 0));     // read, recent -> stays
        d.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("cleanertest"));
    auto ids = [&]() {
        QStringList l;
        QSqlDatabase d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("cleanerids"));
        d.setDatabaseName(db);
        d.open();
        {
            QSqlQuery q(d);
            q.exec(QStringLiteral("SELECT id FROM items ORDER BY id"));
            while (q.next())
                l << q.value(0).toString();
        }
        d.close();
        d = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("cleanerids"));
        return l.join(QLatin1Char(','));
    };

    // Due at once (never ran), a cutoff of 3 months
    CHECK(ArticleCleaner::isDue(ini, now.date()));
    CHECK_EQ(ArticleCleaner::runIfDue(db, ini, now), 1);
    CHECK_EQ(ids(), QStringLiteral("2,3,4,5"));
    // At most once a day
    CHECK(!ArticleCleaner::isDue(ini, now.date()));
    CHECK_EQ(ArticleCleaner::runIfDue(db, ini, now), -1);
    CHECK(ArticleCleaner::isDue(ini, now.date().addDays(1)));

    ArticleCleaner c;
    c.setPaths(db, ini);
    const QString info = c.storageInfo();
    std::printf("     storageInfo: %s\n", qUtf8Printable(info));
    CHECK(info.contains(QStringLiteral("4 articles stored")));
    CHECK(info.contains(QStringLiteral("1 article deleted")));
    // A shorter setting cleans up at the next start, even on the same day
    c.setDeleteAfterMonths(12);
    CHECK_EQ(c.deleteAfterMonths(), 12);
    CHECK(ArticleCleaner::isDue(ini, now.date()));
    CHECK_EQ(ArticleCleaner::runIfDue(db, ini, now), 0);
    CHECK(!ArticleCleaner::isDue(ini, now.date()));
    CHECK(c.storageInfo().contains(QStringLiteral("nothing to delete")));
    c.setDeleteAfterMonths(0);
    CHECK(!ArticleCleaner::isDue(ini, now.date().addDays(5)));
    c.setDeleteAfterMonths(1);

    // Many deletions: VACUUM gives the space back
    {
        QSqlDatabase d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("cleanerbulk"));
        d.setDatabaseName(db);
        d.open();
        d.transaction();
        const QString body(4000, QLatin1Char('x'));
        for (int id = 100; id < 100 + ArticleCleaner::vacuumThreshold + 50; ++id) {
            QSqlQuery i(d);
            i.prepare(QStringLiteral("INSERT INTO items (id, feedId, guid, guidHash, url, title, author, pubDate, body, unread, starred, lastModified, fingerprint) "
                                     "VALUES (?, 1, 'g', ?, 'u', 't', 'a', ?, ?, 0, 0, 0, 'f')"));
            i.addBindValue(id); i.addBindValue(QString::number(id)); i.addBindValue(recent - 30LL * 24 * 3600); i.addBindValue(body);
            i.exec();
        }
        d.commit();
        d.close();
        d = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("cleanerbulk"));
    }
    const qint64 before = QFileInfo(db).size();
    bool vacuumed = false;
    CHECK_EQ(ArticleCleaner::deleteOld(db, recent - 24 * 3600, &vacuumed), ArticleCleaner::vacuumThreshold + 50);
    CHECK(vacuumed);
    CHECK(QFileInfo(db).size() < before / 2);
    CHECK_EQ(ids(), QStringLiteral("2,3,4,5"));
    // Small cleanups do not compact
    CHECK_EQ(ArticleCleaner::deleteOld(db, nowSecs, &vacuumed), 1);
    CHECK(!vacuumed);
    CHECK_EQ(ids(), QStringLiteral("2,3,4"));
    // Image cache (1.2): size, limit, clearing without "prepared"
    {
        const QString cacheDir = dir.path() + QStringLiteral("/images");
        QDir().mkpath(cacheDir + QStringLiteral("/data8/a"));
        QDir().mkpath(cacheDir + QStringLiteral("/prepared"));
        auto write = [](const QString &path, int bytes) { QFile f(path); f.open(QIODevice::WriteOnly); f.write(QByteArray(bytes, 'x')); };
        write(cacheDir + QStringLiteral("/data8/a/one.d"), 3000);
        write(cacheDir + QStringLiteral("/data8/a/two.d"), 2000);
        write(cacheDir + QStringLiteral("/prepared/busy.tmp"), 500);
        ArticleCleaner ic;
        ic.setPaths(db, ini, cacheDir);
        CHECK_EQ(ic.imageCacheMB(), ArticleCleaner::defaultImageCacheMB);
        CHECK_EQ(ArticleCleaner::imageCacheLimit(ini), qint64(100) * 1024 * 1024);
        ic.setImageCacheMB(250);
        CHECK_EQ(ArticleCleaner::imageCacheLimit(ini), qint64(250) * 1024 * 1024);
        CHECK_EQ(ic.imageCacheBytes(), 5000.0);
        CHECK_EQ(ic.clearImageCache(), 5000.0);
        CHECK_EQ(ic.imageCacheBytes(), 0.0);
        CHECK(QFile::exists(cacheDir + QStringLiteral("/prepared/busy.tmp")));
    }
    // Missing database: nothing happens
    CHECK_EQ(ArticleCleaner::runIfDue(dir.path() + QStringLiteral("/none.sqlite"), ini, now.addDays(1)), -1);
}


// OPML file format and feed: links (1.4), without the server
static void testOpml()
{
    // feed: scheme
    CHECK_EQ(FeedFinder::fromFeedScheme(QStringLiteral("feed://example.org/rss")), QStringLiteral("https://example.org/rss"));
    CHECK_EQ(FeedFinder::fromFeedScheme(QStringLiteral("feed:https://example.org/rss")), QStringLiteral("https://example.org/rss"));
    CHECK_EQ(FeedFinder::fromFeedScheme(QStringLiteral("feed://http://example.org/rss")), QStringLiteral("http://example.org/rss"));
    CHECK_EQ(FeedFinder::fromFeedScheme(QStringLiteral(" FEED://Example.org/a?b=1 ")), QStringLiteral("https://Example.org/a?b=1"));
    CHECK_EQ(FeedFinder::fromFeedScheme(QStringLiteral("https://example.org/rss")), QStringLiteral("https://example.org/rss"));
    CHECK_EQ(FeedManager::linkToFeedUrl(QStringLiteral("feed://example.org/rss")), QStringLiteral("https://example.org/rss"));
    CHECK_EQ(FeedManager::linkToFeedUrl(QStringLiteral("feed:example.org/feed/")), QStringLiteral("https://example.org/feed/"));
    CHECK(FeedManager::linkToFeedUrl(QStringLiteral("feed://")).isEmpty());
    CHECK(FeedManager::linkToFeedUrl(QStringLiteral("feed:ftp://example.org/rss")).isEmpty());

    // Writing and reading back: folders, an empty folder, a feed without folder,
    // characters that need escaping
    QList<OpmlFile::Feed> feeds;
    feeds << OpmlFile::Feed{ QString::fromUtf8("Golem & Co <Tech>"), QStringLiteral("https://www.golem.de/rss"), QStringLiteral("https://www.golem.de"), QStringLiteral("high-tech") }
          << OpmlFile::Feed{ QStringLiteral("Android Police"), QStringLiteral("https://www.androidpolice.com/feed"), QString(), QStringLiteral("Android") }
          << OpmlFile::Feed{ QString::fromUtf8("Österreich"), QStringLiteral("https://rss.orf.at/news.xml?x=1&y=2"), QStringLiteral("https://orf.at"), QString() };
    const QByteArray written = OpmlFile::write(feeds, QStringList() << QStringLiteral("Leer") << QStringLiteral("Android"),
                                               QDateTime(QDate(2026, 10, 27), QTime(15, 0), Qt::UTC));
    CHECK(written.startsWith("<?xml version=\"1.0\""));
    CHECK(written.contains("<opml version=\"2.0\">"));
    CHECK(written.contains("<dateCreated>Tue, 27 Oct 2026 15:00:00 GMT</dateCreated>"));
    CHECK(written.contains("Golem &amp; Co &lt;Tech"));
    CHECK(written.contains("xmlUrl=\"https://rss.orf.at/news.xml?x=1&amp;y=2\""));
    CHECK(written.indexOf("high-tech") < written.indexOf("Leer"));       // folders, then empty ones, then the rest
    CHECK(written.indexOf("Leer") < written.indexOf(QString::fromUtf8("Österreich").toUtf8()));
    CHECK(OpmlFile::looksLikeOpml(written.left(200)));
    const OpmlFile::Parsed back = OpmlFile::parse(written);
    CHECK(back.ok);
    CHECK_EQ(back.title, QStringLiteral("Sailfeed"));
    CHECK_EQ(back.feeds.size(), 3);
    if (back.feeds.size() == 3) {
        CHECK_EQ(back.feeds.at(0).title, QString::fromUtf8("Golem & Co <Tech>"));
        CHECK_EQ(back.feeds.at(0).folder, QStringLiteral("high-tech"));
        CHECK_EQ(back.feeds.at(0).link, QStringLiteral("https://www.golem.de"));
        CHECK_EQ(back.feeds.at(1).folder, QStringLiteral("Android"));
        CHECK(back.feeds.at(1).link.isEmpty());
        CHECK_EQ(back.feeds.at(2).url, QStringLiteral("https://rss.orf.at/news.xml?x=1&y=2"));
        CHECK(back.feeds.at(2).folder.isEmpty());
    }
    CHECK_EQ(back.folders.join(QLatin1Char(',')), QStringLiteral("high-tech,Android,Leer"));

    // A file of another reader: nested folders (the innermost counts),
    // attribute case, feed:// addresses, duplicates, unusable addresses, no
    // title, an empty folder, a wrapper with only sub-folders
    const QByteArray other =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<OPML version=\"1.0\"><head><title> Feedly  export </title></head><body>\n"
        "<outline text=\"Tech\">\n"
        "  <outline text=\"Linux\">\n"
        "    <outline type=\"rss\" text=\"LWN\" xmlurl=\"https://lwn.net/headlines/rss\" htmlurl=\"https://lwn.net\"/>\n"
        "    <outline type=\"rss\" title=\"Phoronix\" text=\"x\" xmlUrl=\"feed://www.phoronix.com/rss.php\"/>\n"
        "  </outline>\n"
        "  <outline type=\"rss\" text=\"Golem again\" xmlUrl=\"http://golem.de/rss/\"/>\n"
        "</outline>\n"
        "<outline text=\"Wrapper\"><outline text=\"Empty\"/></outline>\n"
        "<outline type=\"rss\" xmlUrl=\"https://blog.example.org/feed\"/>\n"
        "<outline type=\"rss\" text=\"Broken\" xmlUrl=\"javascript:alert(1)\"/>\n"
        "<outline type=\"rss\" text=\"LWN twice\" xmlUrl=\"https://lwn.net/headlines/rss#x\"/>\n"
        "</body></OPML>\n";
    const OpmlFile::Parsed o = OpmlFile::parse(other);
    CHECK(o.ok);
    CHECK_EQ(o.title, QStringLiteral("Feedly export"));
    CHECK_EQ(o.feeds.size(), 4);
    if (o.feeds.size() == 4) {
        CHECK_EQ(o.feeds.at(0).title, QStringLiteral("LWN"));
        CHECK_EQ(o.feeds.at(0).folder, QStringLiteral("Linux"));
        CHECK_EQ(o.feeds.at(0).link, QStringLiteral("https://lwn.net"));
        CHECK_EQ(o.feeds.at(1).title, QStringLiteral("Phoronix"));           // title before text
        CHECK_EQ(o.feeds.at(1).url, QStringLiteral("https://www.phoronix.com/rss.php"));
        CHECK_EQ(o.feeds.at(2).folder, QStringLiteral("Tech"));
        CHECK_EQ(o.feeds.at(3).title, QStringLiteral("blog.example.org"));   // no title: the host
        CHECK(o.feeds.at(3).folder.isEmpty());
    }
    CHECK(o.folders.contains(QStringLiteral("Empty")));
    CHECK(!o.folders.contains(QStringLiteral("Wrapper")));

    // Errors
    CHECK(!OpmlFile::parse("<rss version=\"2.0\"><channel/></rss>").ok);
    CHECK_EQ(OpmlFile::parse("<rss version=\"2.0\"><channel/></rss>").error, QStringLiteral("This is not an OPML file."));
    CHECK(OpmlFile::parse("<opml><body><outline text=\"a\"").error.startsWith(QStringLiteral("The file is damaged")));
    CHECK_EQ(OpmlFile::parse("<opml><body><outline text=\"Folder\"/></body></opml>").error, QStringLiteral("There are no feeds in this file."));
    CHECK(!OpmlFile::parse(QByteArray()).ok);
    CHECK(!OpmlFile::looksLikeOpml("<rss><channel>"));
    CHECK(!OpmlFile::looksLikeOpml("{\"format\": \"sailfeed-settings\"}"));
    CHECK_EQ(OpmlFile::exportFileName(QDate(2026, 10, 27)), QStringLiteral("sailfeed-feeds-2026-10-27.opml"));
    CHECK_EQ(OpmlFile::exportFileName(QDate(2026, 10, 27), 2), QStringLiteral("sailfeed-feeds-2026-10-27-2.opml"));
    CHECK(FeedTransfer::isOpmlName(QStringLiteral("Feeds.OPML")));
    CHECK(FeedTransfer::isOpmlName(QStringLiteral("subscriptions.xml")));
    CHECK(!FeedTransfer::isOpmlName(QStringLiteral("sailfeed-settings.json")));
}

// Import and export of the feed list against the mock server (1.4):
// Downloads folder, WebDAV, import with folders, skipped, failed, cancelled
static void testFeedTransfer(Configuration *config, Fuoten::AbstractStorage *storage)
{
    QTemporaryDir downloads;
    FeedTransfer t(config);
    t.files()->setTimeout(5000);
    t.setDownloadsPath(downloads.path());

    auto feedCount = [&]() {
        Fuoten::QueryArgs args;
        const QList<Fuoten::Feed *> list = storage->getFeeds(args);
        const int n = list.size();
        qDeleteAll(list);
        return n;
    };
    auto feedByUrl = [&](const QString &url, qint64 *folderId) {
        Fuoten::QueryArgs args;
        const QList<Fuoten::Feed *> list = storage->getFeeds(args);
        bool found = false;
        for (Fuoten::Feed *f : list) {
            if (f->url().toString() == url) {
                found = true;
                if (folderId) *folderId = f->folderId();
            }
        }
        qDeleteAll(list);
        return found;
    };
    auto folderId = [&](const QString &name) {
        const QList<Fuoten::Folder *> list = storage->getFolders();
        qint64 id = -1;
        for (Fuoten::Folder *f : list)
            if (f->name() == name) id = f->id();
        qDeleteAll(list);
        return id;
    };

    const int before = feedCount();
    CHECK(before >= 2);
    CHECK_EQ(t.summary().value(QStringLiteral("feeds")).toInt(), before);

    // Export: the text holds every feed of the device
    const OpmlFile::Parsed mine = OpmlFile::parse(t.opmlText().toUtf8());
    CHECK(mine.ok);
    CHECK_EQ(mine.feeds.size(), before);
    bool golemInFolder = false;
    for (const OpmlFile::Feed &f : mine.feeds)
        golemInFolder = golemInFolder || (f.url == QStringLiteral("https://www.golem.de/rss") && f.folder == QStringLiteral("high-tech"));
    CHECK(golemInFolder);

    // Downloads: a counter when the name exists; listing only OPML
    const QVariantMap e1 = t.exportToDownloads();
    const QVariantMap e2 = t.exportToDownloads();
    CHECK(e1.value(QStringLiteral("ok")).toBool() && e2.value(QStringLiteral("ok")).toBool());
    CHECK_EQ(e1.value(QStringLiteral("name")).toString(), OpmlFile::exportFileName(QDate::currentDate()));
    CHECK_EQ(e2.value(QStringLiteral("name")).toString(), OpmlFile::exportFileName(QDate::currentDate(), 2));
    {
        QFile x(downloads.path() + QStringLiteral("/podcast.xml"));
        x.open(QIODevice::WriteOnly); x.write("<rss version=\"2.0\"><channel/></rss>"); x.close();
        QFile y(downloads.path() + QStringLiteral("/subscriptions.xml"));
        y.open(QIODevice::WriteOnly); y.write("<?xml version=\"1.0\"?>\n<opml version=\"1.0\"><body><outline xmlUrl=\"https://a.example/rss\"/></body></opml>"); y.close();
    }
    QStringList listed;
    for (const QVariant &v : t.downloadFiles())
        listed << v.toMap().value(QStringLiteral("name")).toString();
    CHECK_EQ(listed.size(), 3);
    CHECK(listed.contains(QStringLiteral("subscriptions.xml")));
    CHECK(!listed.contains(QStringLiteral("podcast.xml")));
    const QVariantMap own = t.loadFile(downloads.path() + QLatin1Char('/') + e1.value(QStringLiteral("name")).toString());
    CHECK(own.value(QStringLiteral("ok")).toBool());
    CHECK_EQ(own.value(QStringLiteral("newCount")).toInt(), 0);               // all subscribed
    CHECK_EQ(own.value(QStringLiteral("subscribed")).toInt(), before);
    CHECK(!t.startImport(true));                                              // nothing to do
    QTemporaryDir elsewhere;
    QFile::copy(downloads.path() + QStringLiteral("/subscriptions.xml"), elsewhere.path() + QStringLiteral("/s.xml"));
    CHECK(!t.loadFile(elsewhere.path() + QStringLiteral("/s.xml")).value(QStringLiteral("ok")).toBool());

    // Nextcloud: export, list (backups and other files left out)
    bool done = false, okResult = false;
    QString message;
    QObject::connect(&t, &FeedTransfer::exported, [&](bool ok, const QString &, const QString &m) { done = true; okResult = ok; message = m; });
    QObject::connect(&t, &FeedTransfer::nextcloudListed, [&](bool ok, const QString &m) { done = true; okResult = ok; message = m; });
    QVariantMap loadedPlan;
    QObject::connect(&t, &FeedTransfer::loaded, [&](const QString &, bool ok, const QString &m, const QVariantMap &p) { done = true; okResult = ok; message = m; loadedPlan = p; });
    CHECK(t.exportToNextcloud());
    CHECK(t.busy());
    CHECK(!t.exportToNextcloud());
    CHECK(waitFor([&] { return done; }));
    CHECK(okResult);
    // A file of another reader in the same folder
    const QByteArray other =
        "<?xml version=\"1.0\"?>\n<opml version=\"2.0\"><head><title>Other reader</title></head><body>\n"
        "<outline text=\"android\"><outline text=\"New in Android\" xmlUrl=\"https://new.example/android.xml\"/></outline>\n"
        "<outline text=\"Podcasts\"><outline text=\"New podcast\" xmlUrl=\"https://new.example/podcast.xml\"/>\n"
        "  <outline text=\"Golem\" xmlUrl=\"http://golem.de/rss\"/></outline>\n"
        "<outline text=\"Leer\"/>\n"
        "<outline text=\"Broken\" xmlUrl=\"https://bad.example/rss\"/>\n"
        "<outline text=\"Taken meanwhile\" xmlUrl=\"https://meanwhile.example/rss\"/>\n"
        "</body></opml>\n";
    done = false;
    t.files()->upload(QStringLiteral("other.opml"), other, [&](const NextcloudFiles::Result &r) { done = true; okResult = r.ok; });
    CHECK(waitFor([&] { return done; }));
    done = false;
    CHECK(t.listNextcloud());
    CHECK(waitFor([&] { return done; }));
    CHECK(okResult);
    QStringList ncNames;
    for (const QVariant &v : t.nextcloudFiles())
        ncNames << v.toMap().value(QStringLiteral("name")).toString();
    CHECK(ncNames.contains(OpmlFile::nextcloudFileName()));
    CHECK(ncNames.contains(QStringLiteral("other.opml")));
    CHECK(!ncNames.contains(QStringLiteral("sailfeed-settings.json")));

    // Plan of the other file: Golem subscribed (other address form), 4 new,
    // folder "android" exists (case), "Podcasts" and the empty "Leer" are new
    done = false;
    CHECK(t.loadNextcloud(QStringLiteral("other.opml")));
    CHECK(waitFor([&] { return done; }));
    CHECK(okResult);
    CHECK_EQ(loadedPlan.value(QStringLiteral("title")).toString(), QStringLiteral("Other reader"));
    CHECK_EQ(loadedPlan.value(QStringLiteral("total")).toInt(), 5);
    CHECK_EQ(loadedPlan.value(QStringLiteral("subscribed")).toInt(), 1);
    CHECK_EQ(loadedPlan.value(QStringLiteral("newCount")).toInt(), 4);
    CHECK_EQ(loadedPlan.value(QStringLiteral("newFolders")).toStringList().join(QLatin1Char(',')), QStringLiteral("Podcasts,Leer"));
    done = false;
    CHECK(!t.loadNextcloud(QStringLiteral("missing.opml")) || waitFor([&] { return done; }));
    CHECK(!okResult);
    CHECK(!message.isEmpty());
    done = false;
    CHECK(t.loadNextcloud(QStringLiteral("other.opml")));          // the plan again
    CHECK(waitFor([&] { return done; }));

    // One of the new feeds is subscribed elsewhere meanwhile: the server says 409
    {
        FeedManager fm;
        bool added = false;
        QObject::connect(&fm, &FeedManager::succeeded, [&](const QString &) { added = true; });
        CHECK(fm.addFeed(QStringLiteral("https://meanwhile.example/rss"), 0));
        CHECK(waitFor([&] { return added; }));
    }
    const int beforeImport = feedCount();
    int fAdded = -1, fSkipped = -1, fFailed = -1;
    bool fCancelled = true;
    QObject::connect(&t, &FeedTransfer::importFinished, [&](int a, int s, int f, bool c) { fAdded = a; fSkipped = s; fFailed = f; fCancelled = c; });
    CHECK(t.startImport(true));
    CHECK(t.importing());
    CHECK(!t.startImport(true));
    CHECK_EQ(t.importTotal(), 4);
    CHECK(waitFor([&] { return fAdded >= 0; }, 20000));
    CHECK(!t.importing());
    CHECK_EQ(fAdded, 2);
    CHECK_EQ(fSkipped, 1);
    CHECK_EQ(fFailed, 1);
    CHECK(!fCancelled);
    CHECK_EQ(t.importDone(), 4);
    const QVariantList failedList = t.lastImport().value(QStringLiteral("failed")).toList();
    CHECK_EQ(failedList.size(), 1);
    if (!failedList.isEmpty()) {
        CHECK_EQ(failedList.first().toMap().value(QStringLiteral("url")).toString(), QStringLiteral("https://bad.example/rss"));
        CHECK(!failedList.first().toMap().value(QStringLiteral("reason")).toString().isEmpty());
    }
    CHECK(waitFor([&] { return feedCount() == beforeImport + 2; }));
    qint64 fid = -2;
    CHECK(feedByUrl(QStringLiteral("https://new.example/android.xml"), &fid));
    CHECK_EQ(fid, folderId(QStringLiteral("Android")));              // existing folder, other case
    CHECK(waitFor([&] { return folderId(QStringLiteral("Podcasts")) > 0; }));
    CHECK(feedByUrl(QStringLiteral("https://new.example/podcast.xml"), &fid));
    CHECK_EQ(fid, folderId(QStringLiteral("Podcasts")));
    CHECK(waitFor([&] { return folderId(QStringLiteral("Leer")) > 0; }));   // empty folder created
    CHECK(!t.startImport(true));                                     // a new import needs a new plan

    // Without folders: no folder created, the feed goes to "Other feeds"
    const QByteArray noFolders =
        "<opml version=\"2.0\"><body><outline text=\"Kept out\"><outline text=\"Flat\" xmlUrl=\"https://flat.example/rss\"/></outline>"
        "<outline text=\"Empty too\"/></body></opml>";
    QVariantMap p = t.plan(noFolders);
    CHECK_EQ(p.value(QStringLiteral("newCount")).toInt(), 1);
    fAdded = -1;
    CHECK(t.startImport(false));
    CHECK(waitFor([&] { return fAdded >= 0; }, 20000));
    CHECK_EQ(fAdded, 1);
    CHECK(waitFor([&] { return feedByUrl(QStringLiteral("https://flat.example/rss"), &fid); }));
    CHECK_EQ(fid, 0);
    CHECK_EQ(folderId(QStringLiteral("Kept out")), -1);
    CHECK_EQ(folderId(QStringLiteral("Empty too")), -1);

    // Stopping: the feed being subscribed is finished, the rest is left
    const QByteArray three =
        "<opml version=\"2.0\"><body><outline text=\"S1\" xmlUrl=\"https://slow.example/1\"/>"
        "<outline text=\"S2\" xmlUrl=\"https://slow.example/2\"/><outline text=\"S3\" xmlUrl=\"https://slow.example/3\"/></body></opml>";
    p = t.plan(three);
    CHECK_EQ(p.value(QStringLiteral("newCount")).toInt(), 3);
    fAdded = -1;
    CHECK(t.startImport(true));
    CHECK(waitFor([&] { return t.importCurrent() == QStringLiteral("S1"); }));   // S1 on its way
    t.cancelImport();
    CHECK(waitFor([&] { return fAdded >= 0; }, 20000));
    CHECK(fCancelled);
    CHECK_EQ(fAdded, 1);
    CHECK(t.lastImport().value(QStringLiteral("cancelled")).toBool());

    // A damaged file gives a reason, no plan
    p = t.plan("<opml><body>");
    CHECK(!p.value(QStringLiteral("ok")).toBool());
    CHECK(!p.value(QStringLiteral("error")).toString().isEmpty());
    CHECK(!t.startImport(true));
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("org.sappl"));
    app.setApplicationName(QStringLiteral("harbour-sailfeed"));
    qRegisterMetaType<Fuoten::IdList>("Fuoten::IdList");
    qRegisterMetaType<Fuoten::ArticleList>("Fuoten::ArticleList");
    const QString port = QString::fromLocal8Bit(qgetenv("MOCK_PORT"));

    testHtml();
    testAges();
    testSettingsBackup();
    testCleaner();
    testOpml();
    testFeedFinder(port);

    // Legacy 0.1.0 settings with a base64 password must be migrated
    {
        QSettings legacy(Configuration::settingsFilePath(), QSettings::IniFormat);
        legacy.setValue(QStringLiteral("account/password"), QByteArray("old").toBase64());
    }

    // --- backend wiring as in main.cpp ---
    auto *config = new Configuration(&app);
    CHECK_EQ(config->getPassword(), QStringLiteral("old"));
    CHECK(!QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).contains(QStringLiteral("account/password")));
    std::printf("settings: %s\n", qUtf8Printable(Configuration::settingsFilePath()));
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    auto *storage = new Fuoten::SQLiteStorage(dataDir + QStringLiteral("/database.sqlite"), &app);
    storage->setConfiguration(config);
    storage->init();
    CHECK(waitFor([&] { return storage->ready(); }));
    Fuoten::Component::setDefaultConfiguration(config);
    Fuoten::Component::setDefaultStorage(storage);
    Fuoten::Component::setDefaultNam(new QNetworkAccessManager(&app));
    auto *sync = new Fuoten::Synchronizer(&app);
    sync->setConfiguration(config);
    sync->setStorage(storage);
    AccountManager account(config, storage, sync);

    // Server address parsing
    CHECK(config->setServerAddress(QStringLiteral("example.your-storageshare.de")));
    CHECK_EQ(config->serverUrl(), QStringLiteral("https://example.your-storageshare.de"));
    CHECK(config->setServerAddress(QStringLiteral("http://127.0.0.1:%1/").arg(port)));
    CHECK_EQ(config->getServerPort(), port.toInt());
    CHECK(!config->getUseSSL());

    // Test 2: wrong app password
    bool failed = false, ok = false;
    QObject::connect(&account, &AccountManager::loginFailed, [&](const QString &e) { failed = true; std::printf("     loginFailed: %s\n", qUtf8Printable(e)); });
    QObject::connect(&account, &AccountManager::loginSucceeded, [&] { ok = true; });
    account.login(QStringLiteral("http://127.0.0.1:%1").arg(port), QStringLiteral("thomas"), QStringLiteral("wrong"));
    CHECK(waitFor([&] { return failed || ok; }));
    CHECK(failed && !ok);
    CHECK(!config->isAccountValid());
    CHECK(!account.errorString().isEmpty());

    // Model loaded before the first sync must pick up the synced articles
    ArticleModel early;
    early.setStorage(storage);
    early.load();
    CHECK(waitFor([&] { return early.loaded() && !early.inOperation(); }));
    CHECK_EQ(early.count(), 0);

    // Test 1: correct login -> first sync
    failed = ok = false;
    bool synced = false;
    QObject::connect(sync, &Fuoten::Synchronizer::succeeded, [&] { synced = true; });
    QObject::connect(sync, &Fuoten::Synchronizer::failed, [&](Fuoten::Error *e) { std::printf("     sync failed: %s\n", e ? qUtf8Printable(e->text()) : "?"); synced = true; });
    account.login(QStringLiteral("http://127.0.0.1:%1").arg(port), QStringLiteral("thomas"), QStringLiteral(" secret "));
    CHECK(waitFor([&] { return failed || ok; }));
    CHECK(ok);
    CHECK(config->isAccountValid());
    CHECK_EQ(account.serverVersion(), QStringLiteral("25.3.4"));
    CHECK_EQ(config->getPassword(), QStringLiteral("secret"));
    CHECK(!QSettings(Configuration::settingsFilePath(), QSettings::IniFormat).contains(QStringLiteral("account/password")));
    CHECK(waitFor([&] { return synced && !sync->inOperation(); }, 20000));

    testCloud(config, port);
    testCloudBackup(config);
    testAutoBackup(config);
    CHECK(config->getLastSync().isValid());
    CHECK_EQ(storage->totalUnread(), 4);
    CHECK(waitFor([&] { return early.count() == 4; }));
    CHECK_EQ(early.count(), 4);
    CHECK_EQ(early.data(early.index(0, 0), ArticleModel::ItemIdRole).toInt(), 101);

    // Article model: all articles
    ArticleModel model;
    model.setStorage(storage);
    model.load();
    CHECK(waitFor([&] { return model.loaded() && !model.inOperation(); }));
    CHECK_EQ(model.count(), 4);
    auto role = [&](int row, int r) { return model.data(model.index(row, 0), r); };
    CHECK_EQ(role(0, ArticleModel::ItemIdRole).toInt(), 101);                   // newest first
    CHECK_EQ(role(0, ArticleModel::TitleRole).toString(), QString::fromUtf8("Porsches E-Rückzug"));
    CHECK_EQ(role(0, ArticleModel::FeedTitleRole).toString(), QStringLiteral("Golem.de"));
    CHECK_EQ(role(0, ArticleModel::AgeRole).toString(), QStringLiteral("now"));
    CHECK_EQ(role(0, ArticleModel::DayRole).toString(), QStringLiteral("Today"));
    // 1.4: excerpt and thumbnail come from the ArticleIndex (worker); the
    // list has no bodies
    {
        auto *idx = ArticleIndex::forStorage(qobject_cast<Fuoten::SQLiteStorage *>(storage));
        CHECK(idx != nullptr);
        CHECK(idx == ArticleIndex::forStorage(qobject_cast<Fuoten::SQLiteStorage *>(storage)));   // one per storage
        role(0, ArticleModel::ExcerptRole);            // asks for the rows around
        CHECK(waitFor([&] { return !idx->busy() && !role(0, ArticleModel::ExcerptRole).toString().isEmpty()
                                   && !role(3, ArticleModel::ThumbRole).toString().isEmpty(); }));
        auto *a0 = model.data(model.index(0, 0), ArticleModel::ArticleRole).value<Fuoten::Article *>();
        CHECK(a0 && a0->body().isEmpty());                  // not in memory
    }
    CHECK_EQ(role(0, ArticleModel::ThumbRole).toString(), QStringLiteral("https://example.com/thumb/porsche.jpg"));
    CHECK_EQ(role(0, ArticleModel::ExcerptRole).toString(), QStringLiteral("Porsche will wieder mehr Geld & so."));
    CHECK_EQ(role(1, ArticleModel::ThumbRole).toString(), QStringLiteral("https://cdn.example.com/ring.jpg"));
    CHECK_EQ(role(2, ArticleModel::ItemIdRole).toInt(), 104);
    CHECK(role(2, ArticleModel::ThumbRole).toString().isEmpty());
    // 1.2: oldest first, and back
    {
        QStringList newestFirst;
        for (int r = 0; r < model.count(); ++r)
            newestFirst << role(r, ArticleModel::ItemIdRole).toString();
        CHECK(!model.oldestFirst());
        model.setOldestFirst(true);
        CHECK(model.oldestFirst());
        QStringList oldest;
        for (int r = 0; r < model.count(); ++r)
            oldest << role(r, ArticleModel::ItemIdRole).toString();
        QStringList reversed = newestFirst;
        std::reverse(reversed.begin(), reversed.end());
        CHECK_EQ(oldest.join(QLatin1Char(',')), reversed.join(QLatin1Char(',')));
        CHECK(role(0, ArticleModel::PubDateRole).toDateTime() <= role(model.count() - 1, ArticleModel::PubDateRole).toDateTime());
        model.setOldestFirst(false);
        CHECK_EQ(role(0, ArticleModel::ItemIdRole).toInt(), 101);
    }
    CHECK_EQ(role(3, ArticleModel::ThumbRole).toString(), QStringLiteral("https://cdn.example.com/pixel.jpg"));
    CHECK_EQ(role(3, ArticleModel::DayRole).toString(), QStringLiteral("Yesterday"));
    CHECK(model.roleNames().value(ArticleModel::ThumbRole) == "thumb");
    const QVariantMap snap = model.get(0);
    CHECK_EQ(snap.value(QStringLiteral("feedId")).toInt(), 10);          // per-feed text size
    CHECK_EQ(snap.value(QStringLiteral("body")).toString(), QStringLiteral("<p> Porsche will wieder mehr Geld &amp; so.</p>"));   // from the database
    CHECK_EQ(snap.value(QStringLiteral("thumb")).toString(), QStringLiteral("https://example.com/thumb/porsche.jpg"));
    {
        // info(): the same without the body (1.4)
        const QVariantMap inf = model.info(0);
        CHECK(!inf.contains(QStringLiteral("body")));
        CHECK_EQ(inf.value(QStringLiteral("itemId")).toInt(), 101);
        CHECK_EQ(inf.value(QStringLiteral("url")).toString(), snap.value(QStringLiteral("url")).toString());
        CHECK(inf.value(QStringLiteral("hasContent")).toBool());
        CHECK(model.info(99).isEmpty());
    }

    // dataChanged must carry our roles (empty role list = all roles)
    bool sawAllRoles = false;
    QObject::connect(&model, &QAbstractItemModel::dataChanged, [&](const QModelIndex &, const QModelIndex &, const QVector<int> &roles) {
        if (roles.isEmpty()) sawAllRoles = true;
    });

    // Marking read must not re-sort/reset the list (the ListView would jump to the top)
    int layoutChanges = 0, resets = 0;
    QObject::connect(&model, &QAbstractItemModel::layoutChanged, [&] { ++layoutChanges; });
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { ++resets; });

    // Mark read (queued), unreadOnly keeps the row
    model.setUnreadOnly(true);
    CHECK_EQ(model.count(), 4);
    model.markRead(0);
    CHECK(waitFor([&] { return !role(0, ArticleModel::UnreadRole).toBool(); }));
    CHECK(sawAllRoles);
    CHECK_EQ(model.count(), 4);                 // stays visible (greyed)
    CHECK_EQ(layoutChanges, 0);
    CHECK_EQ(resets, 0);
    CHECK_EQ(storage->totalUnread(), 3);
    model.markRowsRead(QVariantList{1, 2});
    CHECK(waitFor([&] { return storage->totalUnread() == 1; }));
    CHECK_EQ(model.count(), 4);

    model.refreshAges();
    QCoreApplication::processEvents();
    CHECK_EQ(layoutChanges, 0);
    CHECK_EQ(resets, 0);

    // Star via id (queued)
    model.setStarredById(103, true);
    CHECK(waitFor([&] { return role(model.rowOfId(103), ArticleModel::StarredRole).toBool(); }));

    // Reload drops the read ones in unread-only mode
    model.reload();
    CHECK(waitFor([&] { return model.loaded() && !model.inOperation() && model.count() == 1; }));
    CHECK_EQ(model.count(), 1);
    model.setUnreadOnly(false);
    CHECK_EQ(model.count(), 4);

    // Feed list: grouped by folder, unread counts
    FeedModel feeds;
    feeds.setStorage(storage);
    feeds.load();
    CHECK(waitFor([&] { return feeds.loaded() && feeds.count() == 2; }));
    CHECK_EQ(feeds.count(), 2);
    auto frole = [&](int row, int r) { return feeds.data(feeds.index(row, 0), r); };
    CHECK_EQ(frole(0, FeedModel::FolderNameRole).toString(), QStringLiteral("Android"));   // sorted by folder
    CHECK_EQ(frole(0, FeedModel::TitleRole).toString(), QStringLiteral("Android Police"));
    CHECK_EQ(frole(1, FeedModel::FolderNameRole).toString(), QStringLiteral("high-tech"));
    CHECK_EQ(frole(0, FeedModel::FeedIdRole).toInt(), 11);
    CHECK_EQ(feeds.folderIdForName(QStringLiteral("Android")), 1);
    {   // Feeds for a new folder (1.4): by folder name, then title
        const QVariantList byFolder = feeds.feedsByFolder();
        CHECK_EQ(byFolder.size(), 2);
        CHECK_EQ(byFolder.value(0).toMap().value(QStringLiteral("folderName")).toString(), QStringLiteral("Android"));
        CHECK_EQ(byFolder.value(0).toMap().value(QStringLiteral("id")).toInt(), 11);
        CHECK_EQ(byFolder.value(1).toMap().value(QStringLiteral("folderName")).toString(), QStringLiteral("high-tech"));
        CHECK(byFolder.value(1).toMap().value(QStringLiteral("folderId")).toInt() > 0);
    }
    CHECK(feeds.roleNames().value(FeedModel::UnreadCountRole) == "unreadCount");

    // Feed selection
    model.setSelection(-1, 11);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2; }));
    CHECK_EQ(model.count(), 2);
    CHECK_EQ(role(0, ArticleModel::FeedTitleRole).toString(), QStringLiteral("Android Police"));
    model.setSelection(-1, 10);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2 && role(0, ArticleModel::FeedTitleRole).toString() == QStringLiteral("Golem.de"); }));

    // Folder selection
    model.setSelection(1, -1);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2; }));
    CHECK_EQ(model.count(), 2);
    CHECK_EQ(role(0, ArticleModel::FeedTitleRole).toString(), QStringLiteral("Android Police"));

    // Mark folder read (queued)
    const int unreadBefore = frole(0, FeedModel::UnreadCountRole).toInt();
    CHECK(waitFor([&] { return model.unreadCount() == unreadBefore; }));     // listed unread articles
    model.markAllRead();
    CHECK(waitFor([&] { return storage->totalUnread() == 0; }));
    CHECK(unreadBefore > 0);
    CHECK(waitFor([&] { return frole(0, FeedModel::UnreadCountRole).toInt() == 0; }));
    CHECK_EQ(feeds.folderUnread(QStringLiteral("Android")), 0);
    CHECK(!role(0, ArticleModel::UnreadRole).toBool() && !role(1, ArticleModel::UnreadRole).toBool());
    CHECK(waitFor([&] { return model.unreadCount() == 0; }));               // mark-all button greyed out

    // Feed list sorting: folders/feeds with unread articles first
    feeds.resort();
    CHECK_EQ(frole(0, FeedModel::FolderNameRole).toString(), QStringLiteral("Android"));   // all read: alphabetical
    model.setSelection(-1, -1);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 4; }));
    model.setUnreadById(101, true);                                                       // Golem.de (high-tech) unread again
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 1; }));
    CHECK(waitFor([&] { return model.unreadCount() == 1; }));               // active again
    CHECK_EQ(frole(0, FeedModel::FolderNameRole).toString(), QStringLiteral("Android"));   // not re-sorted by itself
    feeds.resort();
    CHECK_EQ(frole(0, FeedModel::FolderNameRole).toString(), QStringLiteral("high-tech"));
    CHECK_EQ(frole(0, FeedModel::TitleRole).toString(), QStringLiteral("Golem.de"));
    // 1.4 (758): muted feed – out of "All articles", not counted, own list intact
    {
        model.setUnreadById(101, true);
        CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 1; }));
        const int gRow = model.rowOfId(101);
        CHECK(gRow >= 0);
        const int feedId = model.info(gRow).value(QStringLiteral("feedId")).toInt();
        const int before = model.count();
        int inFeed = 0;
        for (int r = 0; r < model.count(); ++r)
            if (model.info(r).value(QStringLiteral("feedId")).toInt() == feedId)
                ++inFeed;
        CHECK(inFeed > 0);
        feeds.setMutedFeeds(QVariantList{ feedId });
        model.setMutedFeeds(QVariantList{ feedId });
        CHECK(feeds.isMuted(feedId));
        CHECK_EQ(feeds.folderUnread(QStringLiteral("high-tech")), 0);
        CHECK_EQ(feeds.mutedUnread(), 1);
        CHECK_EQ(model.count(), before - inFeed);
        CHECK(waitFor([&] { return model.unreadCount() == 0; }));
        model.setSelection(-1, feedId);                       // the feed itself shows them
        CHECK(waitFor([&] { return !model.inOperation() && model.count() >= inFeed; }));
        model.setSelection(-1, -1);
        CHECK(waitFor([&] { return !model.inOperation() && model.count() == before - inFeed; }));
        feeds.setMutedFeeds(QVariantList());
        model.setMutedFeeds(QVariantList());
        CHECK_EQ(model.count(), before);
        CHECK_EQ(feeds.mutedUnread(), 0);
        CHECK_EQ(feeds.folderUnread(QStringLiteral("high-tech")), 1);
    }
    model.setUnreadById(101, false);                                                      // back to all read
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 0; }));

    // Search (1.1): every word, title and text, case-insensitive, read articles included
    CHECK_EQ(ArticleModel::searchWords(QStringLiteral("  Foo   BAR ")), (QStringList{ QStringLiteral("foo"), QStringLiteral("bar") }));
    CHECK(ArticleModel::matchesSearch({ QStringLiteral("a"), QStringLiteral("c") }, QStringLiteral("abc")));
    CHECK(!ArticleModel::matchesSearch({ QStringLiteral("x") }, QStringLiteral("abc")));
    // 1.4: the text is searched in the index's worker; titles at once
    auto searchCount = [&](const QString &text, int expected) {
        model.setSearchText(text);
        return waitFor([&] { return model.count() == expected; }, 3000) && model.count() == expected;
    };
    CHECK(searchCount(QStringLiteral("ring"), 1));
    CHECK(searchCount(QStringLiteral("sortiment"), 1));             // only in the text
    model.setSearchFullText(false);
    CHECK_EQ(model.count(), 0);                                      // title only: at once
    model.setSearchFullText(true);
    CHECK(waitFor([&] { return model.count() == 1; }));
    model.setSearchText(QStringLiteral("ring"));
    CHECK_EQ(model.count(), 1);                                      // title match at once, before the worker answers
    CHECK(searchCount(QStringLiteral("android BETA"), 1));
    CHECK(searchCount(QString::fromUtf8("FÜNF kameras"), 1));
    CHECK(searchCount(QStringLiteral("android golem"), 0));         // all words must match
    {
        // the last answer counts, not a slower earlier one
        model.setSearchText(QStringLiteral("ring"));
        model.setSearchText(QStringLiteral("android golem"));
        QElapsedTimer t; t.start();
        while (t.elapsed() < 300) QCoreApplication::processEvents();
        CHECK_EQ(model.count(), 0);
    }
    CHECK(searchCount(QString(), 4));
    model.setSearchOnly(true);                                        // search page: nothing without words
    CHECK_EQ(model.count(), 0);
    CHECK(searchCount(QStringLiteral("pixel"), 1));
    model.setSearchText(QString());
    model.setSearchOnly(false);
    CHECK_EQ(model.count(), 4);
    CHECK_EQ(feeds.feedsMatching(QStringLiteral("GOLEM")).size(), 1);
    CHECK_EQ(feeds.feedsMatching(QStringLiteral("android police")).value(0).toMap().value(QStringLiteral("id")).toInt(), 11);
    CHECK(feeds.feedsMatching(QStringLiteral("  ")).isEmpty());

    // Feed list context menu: mark feed / folder / all read without touching the selection
    model.setUnreadById(101, true);
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 1; }));
    CHECK(feeds.markFeedRead(10));
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 0; }));
    CHECK(waitFor([&] { return !role(model.rowOfId(101), ArticleModel::UnreadRole).toBool(); }));
    model.setUnreadById(101, true);
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 1; }));
    CHECK(feeds.markFolderRead(feeds.folderIdForName(QStringLiteral("high-tech"))));
    CHECK(waitFor([&] { return feeds.folderUnread(QStringLiteral("high-tech")) == 0; }));
    model.setUnreadById(101, true);
    CHECK(waitFor([&] { return storage->totalUnread() == 1; }));
    CHECK(feeds.markAllRead());
    CHECK(waitFor([&] { return storage->totalUnread() == 0; }));
    CHECK(!feeds.markFeedRead(-1));
    CHECK_EQ(model.count(), 4);

    // Article context menu / "mark when scrolling on": by id
    model.setUnreadById(101, true);
    CHECK(waitFor([&] { return role(model.rowOfId(101), ArticleModel::UnreadRole).toBool(); }));
    CHECK_EQ(model.idAt(model.rowOfId(101)), 101.0);
    CHECK_EQ(model.idAt(99), -1.0);
    model.markIdsRead(QVariantList { 101.0, 999999.0 });
    CHECK(waitFor([&] { return storage->totalUnread() == 0; }));
    CHECK(!role(model.rowOfId(101), ArticleModel::UnreadRole).toBool());

    // Starred view: only starred articles, read or unread, "unread only" ignored
    const int starredBefore = storage->starred();          // earlier tests starred one
    model.setStarredById(102, true);
    CHECK(waitFor([&] { return storage->starred() == starredBefore + 1; }));
    model.setUnreadOnly(true);
    model.selectStarred();
    CHECK(model.starredOnly());
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == starredBefore + 1; }));
    CHECK(model.rowOfId(102) >= 0);
    bool allStarred = true;
    for (int r = 0; r < model.count(); ++r)
        allStarred = allStarred && role(r, ArticleModel::StarredRole).toBool();
    CHECK(allStarred);
    model.setUnreadById(102, true);
    CHECK(waitFor([&] { return storage->totalUnread() == 1; }));
    model.markAllRead();                                  // marks the listed ones
    CHECK(waitFor([&] { return storage->totalUnread() == 0; }));
    CHECK_EQ(model.count(), starredBefore + 1);           // still listed (starred, read)
    model.setUnreadOnly(false);
    model.setSelection(-1, -1);
    CHECK(!model.starredOnly());
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 4; }));
    model.setStarredById(102, false);
    CHECK(waitFor([&] { return storage->starred() == starredBefore; }));

    // Feed management against the server, local database follows
    {
        FeedManager fm;
        QString lastOk, lastErr;
        QObject::connect(&fm, &FeedManager::succeeded, [&](const QString &a) { lastOk = a; });
        QObject::connect(&fm, &FeedManager::failed, [&](const QString &a, const QString &m) { lastErr = a + QLatin1Char(':') + m; });
        auto folderName = [&](int id) {
            for (const QVariant &v : fm.folders())
                if (v.toMap().value(QStringLiteral("id")).toInt() == id)
                    return v.toMap().value(QStringLiteral("name")).toString();
            return QString();
        };
        CHECK_EQ(FeedManager::normalizeUrl(QStringLiteral(" example.org/feed ")), QStringLiteral("https://example.org/feed"));
        CHECK_EQ(FeedManager::normalizeUrl(QStringLiteral("http://a.b/rss")), QStringLiteral("http://a.b/rss"));
        CHECK(FeedManager::normalizeUrl(QStringLiteral("ftp://a.b/rss")).isEmpty());
        CHECK(FeedManager::normalizeUrl(QStringLiteral("   ")).isEmpty());
        CHECK_EQ(fm.folders().size(), 2);

        CHECK(fm.addFolder(QStringLiteral("News")));
        CHECK(fm.busy());
        CHECK(!fm.addFolder(QStringLiteral("Other")));     // one at a time
        CHECK(waitFor([&] { return lastOk == QStringLiteral("addFolder"); }));
        CHECK(!fm.busy());
        CHECK_EQ(folderName(3), QStringLiteral("News"));

        // Feed list rows: folder rows with their feeds; an empty folder at the top
        FeedRowsModel list;
        list.setFeeds(&feeds);
        auto lrole = [&](int row, int role) { return list.data(list.index(row), role); };
        auto rowsText = [&]() {
            QStringList out;
            for (int r = 0; r < list.rowCount(); ++r)
                out << (lrole(r, FeedRowsModel::RowTypeRole).toString() == QStringLiteral("folder") ? QStringLiteral("[") + lrole(r, FeedRowsModel::TitleRole).toString() + QStringLiteral("]")
                                                                                                     : lrole(r, FeedRowsModel::TitleRole).toString());
            return out.join(QLatin1Char(','));
        };
        CHECK(list.roleNames().value(FeedRowsModel::RowTypeRole) == "rowType");
        CHECK(rowsText().startsWith(QStringLiteral("[News],")));          // empty, newest first
        CHECK_EQ(lrole(0, FeedRowsModel::FeedCountRole).toInt(), 0);
        CHECK_EQ(lrole(0, FeedRowsModel::FolderIdRole).toInt(), 3);
        CHECK(rowsText().contains(QStringLiteral("[Android],Android Police")));
        CHECK(rowsText().contains(QStringLiteral("[high-tech],Golem.de")));
        for (int r = 0; r < list.rowCount(); ++r) {
            if (lrole(r, FeedRowsModel::KeyRole).toString() == QStringLiteral("folder:2")) {
                CHECK_EQ(lrole(r, FeedRowsModel::UnreadCountRole).toInt(), feeds.folderUnread(QStringLiteral("high-tech")));
                CHECK_EQ(lrole(r, FeedRowsModel::FeedCountRole).toInt(), 1);
                CHECK_EQ(lrole(r + 1, FeedRowsModel::FeedIdRole).toInt(), 10);
                CHECK_EQ(lrole(r + 1, FeedRowsModel::FolderIdRole).toInt(), 2);
            }
        }
        // Review 16 Nov: the folder row does not count a muted feed, and the
        // muted sum follows read/unread changes without a re-sort
        {
            auto folderRowUnread = [&]() {
                for (int r = 0; r < list.rowCount(); ++r)
                    if (lrole(r, FeedRowsModel::KeyRole).toString() == QStringLiteral("folder:2"))
                        return lrole(r, FeedRowsModel::UnreadCountRole).toInt();
                return -1;
            };
            model.setUnreadById(101, true);
            CHECK(waitFor([&] { return folderRowUnread() == 1; }));
            feeds.setMutedFeeds(QVariantList{ 10 });
            CHECK_EQ(folderRowUnread(), 0);
            CHECK_EQ(feeds.mutedUnread(), 1);
            model.setUnreadById(101, false);
            CHECK(waitFor([&] { return feeds.mutedUnread() == 0; }));   // no resort()
            model.setUnreadById(101, true);
            CHECK(waitFor([&] { return feeds.mutedUnread() == 1; }));
            feeds.setMutedFeeds(QVariantList());
            CHECK_EQ(feeds.mutedUnread(), 0);
            CHECK_EQ(folderRowUnread(), 1);
            model.setUnreadById(101, false);
            CHECK(waitFor([&] { return folderRowUnread() == 0; }));
        }
        // Count changes are dataChanged, not a reset
        int resets = 0, changes = 0;
        QObject::connect(&list, &QAbstractItemModel::modelReset, [&] { ++resets; });
        QObject::connect(&list, &QAbstractItemModel::dataChanged, [&] { ++changes; });

        CHECK(fm.addFeed(QStringLiteral("example.org/feed"), 3));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("addFeed"); }));
        CHECK(waitFor([&] { return feeds.count() == 3; }));
        feeds.reload();
        CHECK(resets > 0);
        CHECK(rowsText().contains(QStringLiteral("[News],Example feed")));   // no longer empty
        CHECK(!rowsText().startsWith(QStringLiteral("[News],[")));
        resets = 0;
        changes = 0;
        feeds.setFavorite(12, true);
        feeds.setFavorite(12, false);
        CHECK(changes > 0);
        Fuoten::Feed *f = storage->getFeed(12);
        CHECK(f && f->folderId() == 3 && f->url().toString() == QStringLiteral("https://example.org/feed"));
        delete f;

        CHECK(fm.renameFeed(12, QStringLiteral("Example")));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("renameFeed"); }));
        f = storage->getFeed(12);
        CHECK(f && f->title() == QStringLiteral("Example"));
        delete f;

        auto feedFolder = [&](int feedId) {
            for (int r = 0; r < feeds.count(); ++r)
                if (frole(r, FeedModel::FeedIdRole).toInt() == feedId)
                    return frole(r, FeedModel::FolderNameRole).toString();
            return QStringLiteral("?");
        };
        CHECK(fm.renameFolder(3, QStringLiteral("Nachrichten")));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("renameFolder"); }));
        CHECK_EQ(folderName(3), QStringLiteral("Nachrichten"));
        feeds.reload();                                     // what the app does after these operations
        CHECK_EQ(feedFolder(12), QStringLiteral("Nachrichten"));

        CHECK(fm.moveFeed(12, 0));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("moveFeed"); }));
        f = storage->getFeed(12);
        CHECK(f && f->folderId() == 0);
        delete f;
        feeds.reload();
        CHECK_EQ(feedFolder(12), FeedModel::noFolderName());
        CHECK(rowsText().endsWith(QStringLiteral("[%1],Example").arg(FeedModel::noFolderName())));
        for (int r = 0; r < list.rowCount(); ++r)
            if (lrole(r, FeedRowsModel::KeyRole).toString() == QStringLiteral("none"))
                CHECK(lrole(r, FeedRowsModel::IsOtherRole).toBool() && lrole(r, FeedRowsModel::FolderIdRole).toInt() == -1);
        CHECK(rowsText().startsWith(QStringLiteral("[Nachrichten],")));   // empty again (renamed News)

        // "Other feeds" (folder 0) can be selected like a folder
        lastOk.clear();
        CHECK(fm.moveFeed(11, 0));                          // Android Police out of its folder
        CHECK(waitFor([&] { return lastOk == QStringLiteral("moveFeed"); }));
        feeds.reload();
        model.setSelection(0, -1);
        CHECK_EQ(model.folderId(), 0);
        CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2; }));   // 103, 104 of feed 11
        bool onlyFeed11 = true;
        for (int r = 0; r < model.count(); ++r)
            onlyFeed11 = onlyFeed11 && role(r, ArticleModel::FeedTitleRole).toString() == QStringLiteral("Android Police");
        CHECK(onlyFeed11);
        model.setUnreadById(104, true);
        CHECK(waitFor([&] { return feeds.folderUnreadForKey(QStringLiteral("none")) == 1; }));
        CHECK(feeds.markFolderRead(0));                     // context menu of "Other feeds"
        CHECK(waitFor([&] { return feeds.folderUnreadForKey(QStringLiteral("none")) == 0; }));
        CHECK(waitFor([&] { return !role(model.rowOfId(104), ArticleModel::UnreadRole).toBool(); }));
        model.setUnreadById(103, true);                     // "Mark all as read" in the article list
        CHECK(waitFor([&] { return model.unreadCount() == 1; }));
        model.markAllRead();
        CHECK(waitFor([&] { return model.unreadCount() == 0 && feeds.folderUnreadForKey(QStringLiteral("none")) == 0; }));
        lastOk.clear();
        CHECK(fm.moveFeed(11, 1));                          // back to "Android"
        CHECK(waitFor([&] { return lastOk == QStringLiteral("moveFeed"); }));
        feeds.reload();
        model.setSelection(-1, -1);
        CHECK(waitFor([&] { return !model.inOperation() && model.folderId() == -1; }));
        // Feeds without a folder come after the own folders in every mode,
        // even with a favorite among them (folders with a favorite go first)
        feeds.setSortMode(FeedModel::MostActive);
        feeds.setFavorite(12, true);
        CHECK_EQ(frole(feeds.count() - 1, FeedModel::FolderNameRole).toString(), FeedModel::noFolderName());
        CHECK(frole(0, FeedModel::FolderNameRole).toString() != FeedModel::noFolderName());
        for (int mode = FeedModel::UnreadFirst; mode <= FeedModel::UnreadThenActivity; ++mode) {
            feeds.setSortMode(mode);
            CHECK_EQ(frole(feeds.count() - 1, FeedModel::FolderNameRole).toString(), FeedModel::noFolderName());
        }
        feeds.setFavorite(12, false);
        feeds.setSortMode(FeedModel::UnreadFirst);
        // Sections by folder key, not by name: an own folder named like the
        // group of feeds without folder stays a separate section
        auto rowOfFeed = [&](int feedId) {
            for (int r = 0; r < feeds.count(); ++r)
                if (frole(r, FeedModel::FeedIdRole).toInt() == feedId)
                    return r;
            return -1;
        };
        CHECK(feeds.roleNames().value(FeedModel::FolderKeyRole) == "folderKey");
        CHECK_EQ(frole(rowOfFeed(12), FeedModel::FolderKeyRole).toString(), QStringLiteral("none"));
        CHECK_EQ(frole(rowOfFeed(11), FeedModel::FolderKeyRole).toString(), QStringLiteral("folder:1"));
        CHECK_EQ(feeds.folderNameForKey(QStringLiteral("none")), FeedModel::noFolderName());
        CHECK_EQ(feeds.folderNameForKey(QStringLiteral("folder:1")), QStringLiteral("Android"));
        CHECK_EQ(feeds.folderIdForKey(QStringLiteral("folder:1")), 1);
        CHECK_EQ(feeds.folderIdForKey(QStringLiteral("none")), -1);
        CHECK_EQ(feeds.folderUnreadForKey(QStringLiteral("folder:1")), feeds.folderUnread(QStringLiteral("Android")));
        CHECK(fm.renameFolder(1, FeedModel::noFolderName()));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("renameFolder") && !fm.busy(); }));
        feeds.reload();
        CHECK_EQ(feeds.folderNameForKey(QStringLiteral("folder:1")), FeedModel::noFolderName());
        CHECK_EQ(frole(rowOfFeed(11), FeedModel::FolderKeyRole).toString(), QStringLiteral("folder:1"));
        CHECK_EQ(frole(feeds.count() - 1, FeedModel::FolderKeyRole).toString(), QStringLiteral("none"));
        CHECK(rowOfFeed(11) < rowOfFeed(12));
        lastOk.clear();
        CHECK(fm.renameFolder(1, QStringLiteral("Android")));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("renameFolder") && !fm.busy(); }));
        feeds.reload();
        CHECK_EQ(feeds.folderNameForKey(QStringLiteral("folder:1")), QStringLiteral("Android"));

        CHECK(fm.deleteFeed(12));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("deleteFeed"); }));
        CHECK(waitFor([&] { return feeds.count() == 2; }));

        CHECK(fm.deleteFolder(3));
        CHECK(waitFor([&] { return lastOk == QStringLiteral("deleteFolder"); }));
        CHECK_EQ(fm.folders().size(), 2);

        lastOk.clear();
        CHECK(fm.addFeed(QStringLiteral("bad.example/feed"), 0));
        CHECK(waitFor([&] { return !lastErr.isEmpty(); }));
        CHECK(lastErr.startsWith(QStringLiteral("addFeed:")));
        CHECK(lastOk.isEmpty());
        CHECK(!fm.busy());
        CHECK_EQ(feeds.count(), 2);
    }

    // Load older (already read) articles of a feed from the server
    model.setSelection(-1, 10);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2; }));
    CHECK(model.canFetchOlder());
    int olderNew = -1;
    QObject::connect(&model, &ArticleModel::olderFetched, [&](int n) { olderNew = n; });
    model.fetchOlder();
    CHECK(model.fetchingOlder());
    CHECK(waitFor([&] { return !model.fetchingOlder(); }));
    CHECK_EQ(olderNew, 5);
    CHECK(waitFor([&] { return model.count() == 7; }));
    CHECK_EQ(role(model.count() - 1, ArticleModel::ItemIdRole).toInt(), 50);     // oldest at the end
    CHECK(model.canFetchOlder());
    model.fetchOlder();
    CHECK(waitFor([&] { return !model.fetchingOlder(); }));
    CHECK_EQ(olderNew, 0);
    CHECK(!model.canFetchOlder());                          // server has no more
    CHECK_EQ(model.count(), 7);
    model.setSelection(-1, 11);                             // other feed: none of the old ones
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 2; }));
    CHECK(model.canFetchOlder());
    model.setSelection(-1, -1);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 9; }));

    // "Unread only": read articles of a selection hidden, shown once on request
    model.setUnreadOnly(true);
    model.setSelection(-1, 10);
    CHECK(waitFor([&] { return !model.inOperation() && model.hiddenCount() == 7; }));
    CHECK_EQ(model.count(), 0);
    CHECK(!model.showRead());
    model.setShowRead(true);
    CHECK_EQ(model.count(), 7);
    CHECK_EQ(model.hiddenCount(), 0);
    model.setSelection(-1, 11);                             // other selection: back to unread only
    CHECK(!model.showRead());
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 0 && model.hiddenCount() == 2; }));
    model.setUnreadOnly(false);
    model.setSelection(-1, -1);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 9; }));

    // 1.4: new articles go to their place in the order (they were appended:
    // after a sync the newest ones were at the end of a newest-first list)
    {
        auto sortedBy = [&](bool newestFirst) {
            for (int r = 1; r < model.count(); ++r) {
                const QDateTime a = role(r - 1, ArticleModel::PubDateRole).toDateTime();
                const QDateTime b = role(r, ArticleModel::PubDateRole).toDateTime();
                if (newestFirst ? a < b : a > b)
                    return false;
            }
            return true;
        };
        CHECK(sortedBy(true));
        auto newItem = [](qint64 id, qint64 feed, qint64 age) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
            QJsonObject o;
            o.insert(QStringLiteral("id"), static_cast<double>(id));
            o.insert(QStringLiteral("guid"), QStringLiteral("guid-%1").arg(id));
            o.insert(QStringLiteral("guidHash"), QStringLiteral("hash-%1").arg(id));
            o.insert(QStringLiteral("url"), QStringLiteral("https://example.com/a/%1").arg(id));
            o.insert(QStringLiteral("title"), QStringLiteral("Inserted %1").arg(id));
            o.insert(QStringLiteral("author"), QStringLiteral("A"));
            o.insert(QStringLiteral("pubDate"), static_cast<double>(now - age));
            o.insert(QStringLiteral("body"), QStringLiteral("<p>Inserted.</p>"));
            o.insert(QStringLiteral("feedId"), static_cast<double>(feed));
            o.insert(QStringLiteral("unread"), false);
            o.insert(QStringLiteral("starred"), false);
            o.insert(QStringLiteral("lastModified"), static_cast<double>(now));
            o.insert(QStringLiteral("rtl"), false);
            o.insert(QStringLiteral("fingerprint"), QStringLiteral("fp%1").arg(id));
            QJsonObject root;
            root.insert(QStringLiteral("items"), QJsonArray{ o });
            return QJsonDocument(root);
        };
        int resetsBefore = resets, layoutBefore = layoutChanges;
        storage->itemsRequested(newItem(201, 11, 86400 * 8 + 43200));     // 8.5 days: between 52 and 51
        CHECK(waitFor([&] { return model.count() == 10; }));
        CHECK_EQ(model.rowOfId(201), 7);
        CHECK(sortedBy(true));
        CHECK_EQ(resets, resetsBefore);                  // inserted, not reset
        CHECK_EQ(layoutChanges, layoutBefore);
        model.setOldestFirst(true);
        CHECK(sortedBy(false));
        storage->itemsRequested(newItem(202, 10, 86400 * 9 + 43200));     // 9.5 days: between 50 and 51
        CHECK(waitFor([&] { return model.count() == 11; }));
        CHECK_EQ(model.rowOfId(202), 1);
        CHECK(sortedBy(false));
        model.setOldestFirst(false);
        CHECK(sortedBy(true));
        CHECK_EQ(model.rowOfId(202), 9);
        // their excerpts are indexed after the sync
        CHECK(waitFor([&] { return role(model.rowOfId(201), ArticleModel::ExcerptRole).toString() == QStringLiteral("Inserted."); }));
    }

    // 1.4: a selection changed while the list is loading ends with that
    // selection (libfuoten ignored the second load)
    model.setSelection(-1, 10);
    model.setSelection(-1, 11);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 3
                               && role(0, ArticleModel::FeedTitleRole).toString() == QStringLiteral("Android Police"); }));
    {
        QElapsedTimer t; t.start();
        while (t.elapsed() < 300) QCoreApplication::processEvents();
        CHECK_EQ(model.count(), 3);                          // stays
        CHECK_EQ(model.feedId(), 11);
    }
    model.setSelection(-1, -1);
    CHECK(waitFor([&] { return !model.inOperation() && model.count() == 11; }));

    // 1.4: database in WAL mode, indexes for the counts, the index table
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test-wal"));
        db.setDatabaseName(dataDir + QStringLiteral("/database.sqlite"));
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        CHECK(db.open());
        {
            QSqlQuery q(db);
            CHECK(q.exec(QStringLiteral("PRAGMA journal_mode")) && q.next());
            CHECK_EQ(q.value(0).toString(), QStringLiteral("wal"));
            CHECK(q.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT COUNT(id) FROM items WHERE unread = 1")) && q.next());
            CHECK(q.value(3).toString().contains(QStringLiteral("COVERING INDEX")));
            CHECK(q.exec(QStringLiteral("SELECT COUNT(*) FROM sailfeed_extra")) && q.next());
            CHECK_EQ(q.value(0).toInt(), 11);
        }
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("test-wal"));
    }

    // Feed list sort modes (103 of Android Police is starred, Golem.de has 7 recent articles)
    auto firstFeed = [&]() { return frole(0, FeedModel::TitleRole).toString(); };
    feeds.setSortMode(FeedModel::Alphabetical);
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    feeds.setSortMode(FeedModel::MostActive);
    CHECK_EQ(firstFeed(), QStringLiteral("Golem.de"));
    feeds.setSortMode(FeedModel::MostStarred);
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    // Unread first, then activity incl. stars: Golem 7 recent vs. Android Police 2 + 1 star x 3 = 5
    feeds.setSortMode(FeedModel::UnreadThenActivity);
    CHECK_EQ(firstFeed(), QStringLiteral("Golem.de"));
    model.setUnreadById(104, true);                          // Android Police gets an unread article
    CHECK(waitFor([&] { return storage->totalUnread() == 1; }));
    feeds.resort();
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    model.setUnreadById(104, false);
    CHECK(waitFor([&] { return storage->totalUnread() == 0; }));
    model.setStarredById(104, true);                         // 2 + 2 stars x 3 = 8 > 7
    CHECK(waitFor([&] { return role(model.rowOfId(104), ArticleModel::StarredRole).toBool(); }));
    feeds.resort();
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    model.setStarredById(104, false);
    CHECK(waitFor([&] { return !role(model.rowOfId(104), ArticleModel::StarredRole).toBool(); }));
    feeds.resort();
    CHECK_EQ(firstFeed(), QStringLiteral("Golem.de"));
    // Favorite feeds: first in the activity modes, kept in the settings
    CHECK(!feeds.isFavorite(11));
    feeds.setFavorite(11, true);                             // re-sorts at once
    CHECK(feeds.isFavorite(11));
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    CHECK(frole(0, FeedModel::FavoriteRole).toBool());
    CHECK(feeds.roleNames().value(FeedModel::FavoriteRole) == "favorite");
    {
        FeedModel other;                                     // loaded from the settings
        CHECK(other.isFavorite(11));
        CHECK(!other.isFavorite(10));
    }
    feeds.setSortMode(FeedModel::MostActive);
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    feeds.setSortMode(FeedModel::Alphabetical);              // not an activity mode: no effect
    feeds.setFavorite(10, true);
    feeds.setFavorite(11, false);
    CHECK_EQ(firstFeed(), QStringLiteral("Android Police"));
    feeds.setSortMode(FeedModel::UnreadThenActivity);
    CHECK_EQ(firstFeed(), QStringLiteral("Golem.de"));
    feeds.setFavorite(10, false);
    CHECK(!feeds.isFavorite(10));
    // Favorites as a list (settings backup)
    feeds.setFavoriteIds(QVariantList{ 11, 10.0, -3 });
    CHECK(feeds.isFavorite(10) && feeds.isFavorite(11));
    {
        const QVariantList favs = feeds.favoriteFeeds();
        CHECK_EQ(favs.size(), 2);
        CHECK_EQ(favs.value(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Golem.de"));
        FeedModel other;                                     // stored in the settings
        CHECK(other.isFavorite(10) && other.isFavorite(11));
    }
    feeds.setFavoriteIds(QVariantList());
    CHECK(!feeds.isFavorite(10) && !feeds.isFavorite(11));
    feeds.setSortMode(99);                                   // ignored
    CHECK_EQ(feeds.sortMode(), static_cast<int>(FeedModel::UnreadThenActivity));
    feeds.setSortMode(FeedModel::UnreadFirst);

    // Flush queue to server
    synced = false;
    std::printf("SYNC2\n");
    std::fflush(stdout);
    sync->sync();
    CHECK(waitFor([&] { return synced && !sync->inOperation(); }, 20000));

    // Prefetcher: charset decoding, non-HTML rejected
    {
        Prefetcher pf;
        bool done = false, okHtml = false;
        QObject::connect(&pf, &Prefetcher::finished, [&](const QUrl &, bool ok) { done = true; okHtml = ok; });
        const QUrl page(QStringLiteral("http://127.0.0.1:%1/public/article.html").arg(port));
        pf.prefetch(page);
        CHECK(pf.isPending(page));
        CHECK(waitFor([&] { return done; }));
        CHECK(okHtml);
        CHECK(!pf.isPending(page));
        CHECK(pf.html(page).contains(QString::fromUtf8("Grüße aus Kärnten")));
        CHECK(pf.html(QUrl(page.toString() + QStringLiteral("#frag"))).contains(QStringLiteral("Testartikel")));
        // 1.4: lean copy made with the download, reader results kept
        CHECK(pf.leanHtml(page).contains(QStringLiteral("sailfeed-lean")));
        CHECK(pf.leanHtml(page).contains(QString::fromUtf8("Grüße aus Kärnten")));
        CHECK(pf.article(page).isEmpty());
        pf.setArticle(page, QStringLiteral("{\"content\":\"x\"}"));
        CHECK_EQ(pf.article(QUrl(page.toString() + QStringLiteral("#frag"))), QStringLiteral("{\"content\":\"x\"}"));
        done = false;
        const QUrl img(QStringLiteral("http://127.0.0.1:%1/public/image.jpg").arg(port));
        pf.prefetch(img);
        CHECK(waitFor([&] { return done; }));
        CHECK(!okHtml);
        CHECK(pf.html(img).isEmpty());
        pf.prefetch(QUrl(QStringLiteral("ftp://example.com/x")));
        CHECK(!pf.isPending(QUrl(QStringLiteral("ftp://example.com/x"))));
        pf.clear();
        CHECK(pf.html(page).isEmpty());
        CHECK(pf.leanHtml(page).isEmpty() && pf.article(page).isEmpty());
        // 1.4: cancel a running download (the preview was left)
        {
            const QUrl slow2(QStringLiteral("http://127.0.0.1:%1/public/slow.html?c").arg(port));
            bool got = false, gotOk = true;
            auto c = QObject::connect(&pf, &Prefetcher::finished, [&](const QUrl &u, bool ok) {
                if (u == slow2) { got = true; gotOk = ok; }
            });
            pf.prefetch(slow2);
            CHECK(pf.isPending(slow2));
            pf.cancel(slow2);
            CHECK(waitFor([&] { return got; }));
            CHECK(!gotOk);
            CHECK(!pf.isPending(slow2));
            CHECK(pf.html(slow2).isEmpty());
            pf.cancel(QUrl(QStringLiteral("http://nothing.example/")));     // nothing running: no-op
            QObject::disconnect(c);
        }

        // Slow page (1.2): idleMs stays small while data comes in, -1 when done
        done = false;
        const QUrl slow(QStringLiteral("http://127.0.0.1:%1/public/slow.html").arg(port));
        CHECK_EQ(pf.idleMs(slow), -1);
        QElapsedTimer t;
        t.start();
        pf.prefetch(slow);
        CHECK(pf.idleMs(slow) >= 0);
        int maxIdle = 0;
        CHECK(waitFor([&] { maxIdle = qMax(maxIdle, pf.idleMs(slow)); return done; }));
        std::printf("     slow page: %lld ms, longest pause %d ms\n", static_cast<long long>(t.elapsed()), maxIdle);
        CHECK(t.elapsed() >= 2000);
        CHECK(maxIdle < 1500);
        CHECK(okHtml);
        CHECK(pf.html(slow).contains(QStringLiteral("Teil 2")));
        CHECK_EQ(pf.idleMs(slow), -1);
    }

    // Prefetcher sends the in-app browser's (Gecko) cookies: consent walls
    {
        const QString dbPath = QDir::temp().filePath(QStringLiteral("sailfeed-test-cookies.sqlite"));
        QFile::remove(dbPath);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("mkcookies"));
            db.setDatabaseName(dbPath);
            CHECK(db.open());
            QSqlQuery q(db);
            CHECK(q.exec(QStringLiteral("CREATE TABLE moz_cookies (id INTEGER PRIMARY KEY, originAttributes TEXT NOT NULL DEFAULT '', "
                                        "name TEXT, value TEXT, host TEXT, path TEXT, expiry INTEGER, lastAccessed INTEGER, "
                                        "creationTime INTEGER, isSecure INTEGER, isHttpOnly INTEGER)")));
            const qint64 future = QDateTime::currentMSecsSinceEpoch() / 1000 + 86400;
            auto add = [&](const QString &origin, const QString &name, const QString &host, const QString &path, qint64 expiry, int secure) {
                QSqlQuery i(db);
                i.prepare(QStringLiteral("INSERT INTO moz_cookies (originAttributes, name, value, host, path, expiry, isSecure) VALUES (?,?,?,?,?,?,?)"));
                i.addBindValue(origin); i.addBindValue(name); i.addBindValue(QStringLiteral("1"));
                i.addBindValue(host); i.addBindValue(path); i.addBindValue(expiry); i.addBindValue(secure);
                return i.exec();
            };
            CHECK(add(QStringLiteral(""), QStringLiteral("consent"), QStringLiteral("127.0.0.1"), QStringLiteral("/"), future, 0));
            CHECK(add(QStringLiteral(""), QStringLiteral("msexp"), QStringLiteral("127.0.0.1"), QStringLiteral("/public"), future * 1000, 0));
            CHECK(add(QStringLiteral(""), QStringLiteral("old"), QStringLiteral("127.0.0.1"), QStringLiteral("/"), 1000, 0));
            CHECK(add(QStringLiteral(""), QStringLiteral("sec"), QStringLiteral("127.0.0.1"), QStringLiteral("/"), future, 1));
            CHECK(add(QStringLiteral("^partitionKey=x"), QStringLiteral("part"), QStringLiteral("127.0.0.1"), QStringLiteral("/"), future, 0));
            CHECK(add(QStringLiteral(""), QStringLiteral("other"), QStringLiteral("127.0.0.1"), QStringLiteral("/other"), future, 0));
            CHECK(add(QStringLiteral(""), QStringLiteral("foreign"), QStringLiteral("example.org"), QStringLiteral("/"), future, 0));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("mkcookies"));

        Prefetcher pf;
        pf.setCookieDatabase(dbPath);
        CHECK(pf.cookieInfo().contains(QStringLiteral("7")));
        bool done = false, okHtml = false;
        QObject::connect(&pf, &Prefetcher::finished, [&](const QUrl &, bool ok) { done = true; okHtml = ok; });
        const QUrl echo(QStringLiteral("http://127.0.0.1:%1/public/echo.html").arg(port));
        pf.prefetch(echo);
        CHECK(waitFor([&] { return done; }));
        CHECK(okHtml);
        const QString got = pf.html(echo);
        CHECK(got.contains(QStringLiteral("consent=1")));
        CHECK(got.contains(QStringLiteral("msexp=1")));
        CHECK(!got.contains(QStringLiteral("old=")));
        CHECK(!got.contains(QStringLiteral("sec=")));
        CHECK(!got.contains(QStringLiteral("part=")));
        CHECK(!got.contains(QStringLiteral("other=")));
        CHECK(!got.contains(QStringLiteral("foreign=")));

        // Privacy: without sharing, the downloads get no browser cookies
        CHECK(pf.shareBrowserCookies());
        pf.setShareBrowserCookies(false);
        CHECK(!pf.shareBrowserCookies());
        done = false;
        pf.prefetch(echo);
        CHECK(waitFor([&] { return done; }));
        CHECK(!pf.html(echo).contains(QStringLiteral("consent=1")));
        pf.setShareBrowserCookies(true);

        // Privacy: delete cookies and site data at the next start
        {
            const QString profile = QDir::temp().filePath(QStringLiteral("sailfeed-test-profile"));
            QDir(profile).removeRecursively();
            QDir().mkpath(profile + QStringLiteral("/storage/default"));
            const QString cdb = profile + QStringLiteral("/cookies.sqlite");
            for (const QString &f : QStringList { cdb, profile + QStringLiteral("/webappsstore.sqlite"), profile + QStringLiteral("/prefs.js") }) {
                QFile file(f); file.open(QIODevice::WriteOnly); file.write("x");
            }
            CHECK(!Prefetcher::clearWebDataIfRequested(cdb));          // nothing scheduled
            CHECK(QFile::exists(cdb));
            pf.scheduleClearWebData();
            CHECK(pf.clearScheduled());
            CHECK(Prefetcher::clearWebDataIfRequested(cdb));
            CHECK(!pf.clearScheduled());                               // once only
            CHECK(!QFile::exists(cdb));
            CHECK(!QFile::exists(profile + QStringLiteral("/webappsstore.sqlite")));
            CHECK(!QDir(profile + QStringLiteral("/storage")).exists());
            CHECK(QFile::exists(profile + QStringLiteral("/prefs.js")));   // the rest of the profile stays
            pf.setClearAtStart(true);
            QFile f2(cdb); f2.open(QIODevice::WriteOnly); f2.write("x"); f2.close();
            CHECK(Prefetcher::clearWebDataIfRequested(cdb));           // every start
            CHECK(!QFile::exists(cdb));
            pf.setClearAtStart(false);
            QDir(profile).removeRecursively();
        }

        Prefetcher none;
        none.setCookieDatabase(QDir::temp().filePath(QStringLiteral("sailfeed-missing.sqlite")));
        CHECK(!none.cookieInfo().isEmpty());
        QFile::remove(dbPath);
    }

    testFeedTransfer(config, storage);

    // Sign-in in the browser (Login Flow v2): server hands over user and app password
    {
        account.logout();
        CHECK(!config->isAccountValid());
        account.setBrowserLoginPollingInterval(1);
        QUrl gotUrl;
        bool flowOk = false;
        QString flowErr;
        auto c1 = QObject::connect(&account, &AccountManager::loginUrl, [&](const QUrl &u) { gotUrl = u; });
        auto c2 = QObject::connect(&account, &AccountManager::loginSucceeded, [&]() { flowOk = true; });
        auto c3 = QObject::connect(&account, &AccountManager::loginFailed, [&](const QString &e) { flowErr = e; });
        account.loginWithBrowser(QStringLiteral("http://127.0.0.1:%1").arg(port));
        CHECK(account.browserLoginActive());
        CHECK(waitFor([&] { return gotUrl.isValid(); }));
        CHECK(gotUrl.path() == QStringLiteral("/login/v2/flow/abc"));
        CHECK(waitFor([&] { return flowOk || !flowErr.isEmpty(); }, 15000));
        CHECK(flowOk);
        CHECK(!account.browserLoginActive());
        CHECK_EQ(config->getUsername(), QStringLiteral("thomas"));
        CHECK_EQ(config->getPassword(), QStringLiteral("secret"));
        CHECK(config->isAccountValid());
        QObject::disconnect(c1); QObject::disconnect(c2); QObject::disconnect(c3);
        // cancel while waiting
        account.loginWithBrowser(QStringLiteral("http://127.0.0.1:%1").arg(port));
        CHECK(account.browserLoginActive());
        account.cancelBrowserLogin();
        CHECK(!account.browserLoginActive());
        CHECK(!account.busy());
    }

    // Self-signed certificates option
    CHECK(!config->getIgnoreSSLErrors());
    config->setIgnoreSslErrors(true);
    CHECK(config->getIgnoreSSLErrors());
    config->setIgnoreSslErrors(false);

    // Understandable errors
    {
        failed = ok = false;
        account.login(QStringLiteral("http://127.0.0.1:%1").arg(port), QStringLiteral("thomas"), QStringLiteral("wrong"));
        CHECK(waitFor([&] { return failed || ok; }, 15000));
        CHECK(account.errorString().startsWith(QStringLiteral("Sign-in failed")));
    }

    // Wrong server
    failed = ok = false;
    account.login(QStringLiteral("http://127.0.0.1:1"), QStringLiteral("thomas"), QStringLiteral("secret"));
    CHECK(waitFor([&] { return failed || ok; }, 15000));
    CHECK(failed);
    CHECK(account.errorString().startsWith(QStringLiteral("Server not reachable")));
    // The other server cleared the database; it is recreated in a worker thread.
    CHECK(waitFor([&] { return storage->ready(); }));

    std::printf("\n%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

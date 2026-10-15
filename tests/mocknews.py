#!/usr/bin/env python3
"""Minimal Nextcloud News API v1-2 mock for testing harbour-sailfeed's backend.

Accepts user "thomas" / app password "secret". Logs every request to stdout
as JSON lines so the test can check what the client sent.

Also a small WebDAV server for the settings backup: the user's files are at
/remote.php/dav/files/tsappl/ – the internal user ID "tsappl" deliberately
differs from the login name "thomas" (as when signing in with an e-mail
address); the OCS API (/ocs/v2.php/cloud/user) tells the ID.
"""
import base64, email.utils, hashlib, json, sys, time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs, quote, unquote

BASE = "/index.php/apps/news/api/v1-2"
NOW = int(time.time())
# Yesterday 12:00 local time (a fixed "30 h ago" is two days ago early in the morning)
_y = time.localtime(NOW - 86400)
YESTERDAY_AGE = NOW - int(time.mktime((_y.tm_year, _y.tm_mon, _y.tm_mday, 12, 0, 0, 0, 0, -1)))

FOLDERS = [{"id": 1, "name": "Android"}, {"id": 2, "name": "high-tech"}]
FEEDS = [
    {"id": 10, "url": "https://www.golem.de/rss", "title": "Golem.de", "faviconLink": None, "added": NOW - 10**6,
     "folderId": 2, "unreadCount": 0, "ordering": 0, "link": "https://www.golem.de", "pinned": False,
     "updateErrorCount": 0, "lastUpdateError": None},
    {"id": 11, "url": "https://www.androidpolice.com/feed", "title": "Android Police", "faviconLink": None,
     "added": NOW - 10**6, "folderId": 1, "unreadCount": 0, "ordering": 0, "link": "https://www.androidpolice.com",
     "pinned": False, "updateErrorCount": 0, "lastUpdateError": None},
]

def item(i, feed, title, body, age, enc_mime=None, enc_link=None, thumb=None):
    guid = f"guid-{i}"
    return {"id": i, "guid": guid, "guidHash": hashlib.md5(guid.encode()).hexdigest(),
            "url": f"https://example.com/a/{i}", "title": title, "author": "A", "pubDate": NOW - age,
            "body": body, "enclosureMime": enc_mime, "enclosureLink": enc_link, "mediaThumbnail": thumb,
            "mediaDescription": None, "feedId": feed, "unread": True, "starred": False,
            "lastModified": NOW - age, "rtl": False, "fingerprint": f"fp{i}"}

ITEMS = [
    item(101, 10, "Porsches E-R&uuml;ckzug", '<p><img src="https://img.golem.de/p.jpg" width="1" height="1">'
         '<img src="/thumb/porsche.jpg"> Porsche will wieder mehr Geld &amp; so.</p>', 30),
    item(102, 10, "Ring: Fünf neue Kameras", "<p>Ring erweitert das Sortiment.</p>", 120,
         "image/jpeg", "https://cdn.example.com/ring.jpg"),
    item(103, 11, "Pixel update", "<p>No image here.</p>", YESTERDAY_AGE, thumb="https://cdn.example.com/pixel.jpg"),
    item(104, 11, "Android 17 beta", "<p>Text only.</p>", 3600 * 2),
]

# Older, already read articles of Golem.de (feed 10): not part of the first
# sync (unread + starred only), reachable with offset requests ("load older").
OLD_ITEMS = []
for _i in range(50, 55):
    _it = item(_i, 10, f"Old Golem article {_i}", "<p>Old.</p>", 86400 * (60 - _i))
    _it["unread"] = False
    OLD_ITEMS.append(_it)

# --- Feed finder test sites ---------------------------------------------------
def _rss(title, items, media=False, desc="About things"):
    out = [f'<?xml version="1.0" encoding="UTF-8"?><rss version="2.0"><channel><title>{title}</title>'
           f'<description>{desc}</description><link>http://x/</link>']
    for i, (t, date) in enumerate(items):
        enc = '<enclosure url="http://x/a.mp3" type="audio/mpeg" length="1"/>' if media else ""
        out.append(f"<item><title>{t}</title><pubDate>{date}</pubDate>{enc}</item>")
    out.append("</channel></rss>")
    return "".join(out)

_DATES = ["Mon, 12 Oct 2026 10:00:00 GMT", "Sun, 11 Oct 2026 09:00:00 +0200", "Fri, 09 Oct 2026 08:00:00 GMT",
          "Thu, 08 Oct 2026 08:00:00 GMT"]
FINDER_PAGES = {
    "/site/": (200, "text/html; charset=utf-8",
               '<html><head><title>Site</title>'
               '<link rel="alternate" type="application/rss+xml" title="News" href="/site/feed.xml">'
               '<link rel="alternate" type="application/rss+xml" title="Comments" href="comments/feed/">'
               '<link rel="alternate" hreflang="de" href="/de/">'
               '</head><body><a href="/site/feedback">Feedback</a> <a href="/site/podcast.rss">Podcast RSS</a></body></html>'),
    "/site/feed.xml": (200, "application/rss+xml", _rss("Site News", [("First &amp; best", _DATES[0]), ("Second", _DATES[1]),
                                                                       ("Third", _DATES[2]), ("Fourth", _DATES[3])])),
    "/site/comments/feed/": (200, "application/rss+xml", _rss("Comments for Site", [("Re: First", _DATES[0])])),
    "/site/podcast.rss": (200, "application/rss+xml", _rss("Site Podcast", [("Episode 1", _DATES[1]), ("Episode 2", _DATES[2])], media=True)),
    "/site/2026/10/post.html": (200, "text/html", "<html><head><title>Post</title></head><body><p>No feed links.</p></body></html>"),
    "/direct.atom": (200, "application/atom+xml",
                     '<?xml version="1.0" encoding="utf-8"?><feed xmlns="http://www.w3.org/2005/Atom"><title type="text">Direct Atom</title>'
                     '<subtitle>Sub</subtitle><entry><title>Entry A</title><updated>2026-10-13T08:00:00Z</updated></entry>'
                     '<entry><title>Entry B</title><updated>2026-10-10T08:00:00+02:00</updated></entry></feed>'),
    "/redir": (301, "text/plain", "/site/feed.xml"),
    "/blog/2026/hello": (200, "text/html", "<html><body>Plain blog post, no links.</body></html>"),
    "/blog/feed/": (200, "application/rss+xml", _rss("Sub-folder blog", [("Hello", _DATES[0])])),
    # "Already subscribed": announced with a tracking parameter, the feed
    # names its canonical address in <atom:link rel="self">
    "/self/": (200, "text/html", '<html><head><link rel="alternate" type="application/rss+xml" '
                                 'href="/self/feed.rss?utm_source=page&amp;wt_mc=rss.x"></head><body></body></html>'),
    "/self/feed.rss": (200, "application/rss+xml",
                       '<?xml version="1.0"?><rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom"><channel>'
                       '<title>Self News</title><link>https://self.example.org/</link>'
                       '<atom:link rel="self" type="application/rss+xml" href="/canonical/feed/"/>'
                       '<item><title>S1</title></item></channel></rss>'),
    # same title and website as a subscribed feed, other address
    "/likely.xml": (200, "application/atom+xml",
                    '<?xml version="1.0"?><feed xmlns="http://www.w3.org/2005/Atom"><title>Likely  News</title>'
                    '<link rel="alternate" href="https://likely.example.org/"/><link rel="self" href="https://likely.example.org/atom"/>'
                    '<entry><title>L1</title></entry></feed>'),
}

USER_ID = "tsappl"
DAV_ROOT = f"/remote.php/dav/files/{USER_ID}/"
DAV_FILES = {}          # path below DAV_ROOT -> (bytes, mtime, etag)
DAV_DIRS = {""}         # folders below DAV_ROOT ("" = the root, "Sailfeed" ...)
_etag_counter = [0]

def _dav_propstat_xml(href, is_dir, size=0, mtime=0, etag=""):
    if is_dir:
        return (f"<d:response><d:href>{href}</d:href><d:propstat><d:prop>"
                f"<d:getlastmodified>{email.utils.formatdate(time.time(), usegmt=True)}</d:getlastmodified>"
                f"<d:getetag>&quot;dir&quot;</d:getetag><d:resourcetype><d:collection/></d:resourcetype>"
                f"</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>"
                f"<d:propstat><d:prop><d:getcontentlength/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>"
                f"</d:response>")
    return (f"<d:response><d:href>{href}</d:href><d:propstat><d:prop>"
            f"<d:getlastmodified>{email.utils.formatdate(mtime, usegmt=True)}</d:getlastmodified>"
            f"<d:getcontentlength>{size}</d:getcontentlength><d:getetag>&quot;{etag}&quot;</d:getetag>"
            f"<d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>")

class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _auth_ok(self):
        h = self.headers.get("Authorization", "")
        if not h.startswith("Basic "):
            return False
        return base64.b64decode(h[6:]).decode() == "thomas:secret"

    def _send(self, code, obj=None):
        data = json.dumps(obj).encode() if obj is not None else b""
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _handle(self, method):
        u = urlparse(self.path)
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length).decode() if length else ""
        print(json.dumps({"method": method, "path": u.path, "query": u.query, "body": body}), flush=True)
        # --- Websites for the feed finder ("Subscribe to website") ---
        site = FINDER_PAGES.get(u.path + (("?" + u.query) if u.query else ""))
        if site is None and u.path in FINDER_PAGES and (not u.query or u.path.startswith("/self/")):
            site = FINDER_PAGES[u.path]
        if site is not None:
            status, ctype, data = site
            self.send_response(status)
            if status in (301, 302):
                self.send_header("Location", data)
                data = ""
            self.send_header("Content-Type", ctype)
            raw = data.encode()
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
            return
        if u.path.startswith("/site/") or u.path.startswith("/blog/"):
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if u.path == "/public/article.html":
            data = "<html><head><title>Testartikel</title></head><body><p>Grüße aus Kärnten</p></body></html>".encode("iso-8859-1")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=ISO-8859-1")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if u.path == "/public/echo.html":
            data = ("<html><body>COOKIES:" + (self.headers.get("Cookie") or "") + "</body></html>").encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if u.path == "/public/image.jpg":
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", "3")
            self.end_headers()
            self.wfile.write(b"abc")
            return
        # Nextcloud Login Flow v2 (no authentication)
        if method == "POST" and u.path == "/index.php/login/v2":
            port = self.server.server_port
            return self._send(200, {"poll": {"token": "tok", "endpoint": f"http://127.0.0.1:{port}/index.php/login/v2/poll"},
                                    "login": f"http://127.0.0.1:{port}/login/v2/flow/abc"})
        if method == "POST" and u.path == "/index.php/login/v2/poll":
            H.polls = getattr(H, "polls", 0) + 1
            if H.polls < 2 or "token=tok" not in body:
                return self._send(404, [])
            return self._send(200, {"server": f"http://127.0.0.1:{self.server.server_port}",
                                    "loginName": "thomas", "appPassword": "secret"})
        if not self._auth_ok():
            return self._send(401, {"message": "Unauthorized"})
        if u.path == "/ocs/v2.php/cloud/user":
            if self.headers.get("OCS-APIRequest") != "true":
                return self._send(400, {"message": "CSRF check failed"})
            return self._send(200, {"ocs": {"meta": {"status": "ok", "statuscode": 200},
                                            "data": {"id": USER_ID, "display-name": "Thomas Sappl"}}})
        if u.path.startswith("/remote.php/dav/files/"):
            return self._dav(method, unquote(u.path), body)
        p = u.path[len(BASE):] if u.path.startswith(BASE) else None
        if p == "/version":
            return self._send(200, {"version": "25.3.4"})
        if p == "/status":
            return self._send(200, {"version": "25.3.4", "warnings": {"improperlyConfiguredCron": False, "incorrectDbCharset": False}})
        if method == "GET" and p == "/folders":
            return self._send(200, {"folders": FOLDERS})
        if method == "GET" and p == "/feeds":
            return self._send(200, {"feeds": FEEDS, "starredCount": 0, "newestItemId": 104})
        if p == "/items":
            q = {k: v[0] for k, v in parse_qs(u.query).items()}
            offset = int(q.get("offset", "0") or 0)
            if offset > 0:          # "load older": items below offset of the feed/folder/all
                typ, pid = int(q.get("type", "3")), int(q.get("id", "0"))
                feed_folder = {f["id"]: f["folderId"] for f in FEEDS}
                res = [it for it in ITEMS + OLD_ITEMS if it["id"] < offset
                       and (typ == 3 or (typ == 0 and it["feedId"] == pid)
                            or (typ == 1 and feed_folder.get(it["feedId"]) == pid))]
                res.sort(key=lambda it: -it["id"])
                return self._send(200, {"items": res[:int(q.get("batchSize", "-1")) if int(q.get("batchSize", "-1")) > 0 else None]})
            return self._send(200, {"items": ITEMS})
        if p == "/items/updated":
            return self._send(200, {"items": ITEMS})
        if method == "PUT" and p in ("/items/read/multiple", "/items/unread/multiple",
                                    "/items/star/multiple", "/items/unstar/multiple", "/items/read"):
            return self._send(200, None)
        if method == "PUT" and p.startswith("/folders/") and p.endswith("/read"):
            return self._send(200, None)
        # --- feed and folder management (stateful) ---
        data = json.loads(body) if body else {}
        if method == "POST" and p == "/folders":
            if any(f["name"] == data.get("name") for f in FOLDERS):
                return self._send(409, {"message": "folder exists"})
            f = {"id": max(x["id"] for x in FOLDERS) + 1, "name": data.get("name")}
            FOLDERS.append(f)
            return self._send(200, {"folders": [f]})
        if method == "POST" and p == "/feeds":
            if "bad.example" in data.get("url", ""):
                return self._send(422, {"message": "Feed could not be parsed"})
            f = dict(FEEDS[0]); f.update({"id": max(x["id"] for x in FEEDS) + 1, "url": data["url"],
                     "title": "Example feed", "folderId": data.get("folderId") or 0, "link": data["url"]})
            FEEDS.append(f)
            return self._send(200, {"feeds": [f], "newestItemId": 104})
        parts = p.strip("/").split("/")
        if len(parts) >= 2 and parts[0] in ("feeds", "folders") and parts[1].isdigit():
            coll = FEEDS if parts[0] == "feeds" else FOLDERS
            obj = next((x for x in coll if x["id"] == int(parts[1])), None)
            if obj is None:
                return self._send(404, {"message": "not found"})
            if method == "DELETE" and len(parts) == 2:
                coll.remove(obj)
                if coll is FOLDERS:
                    FEEDS[:] = [x for x in FEEDS if x["folderId"] != obj["id"]]
                return self._send(200, None)
            if method == "PUT" and parts[0] == "feeds" and parts[2:] == ["rename"]:
                obj["title"] = data.get("feedTitle"); return self._send(200, None)
            if method == "PUT" and parts[0] == "feeds" and parts[2:] == ["move"]:
                obj["folderId"] = data.get("folderId"); return self._send(200, None)
            if method == "PUT" and parts[0] == "folders" and len(parts) == 2:
                obj["name"] = data.get("name"); return self._send(200, None)
        return self._send(404, {"message": "not found"})

    def _dav(self, method, path, body):
        if not path.startswith(DAV_ROOT):
            return self._send(404, {"message": "principal not found"})   # wrong user ID
        rel = path[len(DAV_ROOT):]
        is_dir_req = rel == "" or rel.endswith("/")
        rel = rel.rstrip("/")
        parent = rel.rsplit("/", 1)[0] if "/" in rel else ""
        if method == "PROPFIND":
            depth = self.headers.get("Depth", "1")
            if rel in DAV_DIRS:
                parts = [_dav_propstat_xml(quote(DAV_ROOT + rel + ("/" if rel else "")), True)]
                if depth != "0":
                    for f, (data, mtime, etag) in sorted(DAV_FILES.items()):
                        if (f.rsplit("/", 1)[0] if "/" in f else "") == rel:
                            parts.append(_dav_propstat_xml(quote(DAV_ROOT + f), False, len(data), mtime, etag))
                    for d in sorted(DAV_DIRS):
                        if d and (d.rsplit("/", 1)[0] if "/" in d else "") == rel:
                            parts.append(_dav_propstat_xml(quote(DAV_ROOT + d + "/"), True))
            elif rel in DAV_FILES and not is_dir_req:
                data, mtime, etag = DAV_FILES[rel]
                parts = [_dav_propstat_xml(quote(DAV_ROOT + rel), False, len(data), mtime, etag)]
            else:
                return self._send(404, {"message": "not found"})
            xml = ('<?xml version="1.0"?><d:multistatus xmlns:d="DAV:">' + "".join(parts) + "</d:multistatus>").encode()
            self.send_response(207)
            self.send_header("Content-Type", "application/xml; charset=utf-8")
            self.send_header("Content-Length", str(len(xml)))
            self.end_headers()
            self.wfile.write(xml)
            return
        if method == "MKCOL":
            if rel in DAV_DIRS or rel in DAV_FILES:
                return self._send(405, {"message": "exists"})
            if parent not in DAV_DIRS:
                return self._send(409, {"message": "parent missing"})
            DAV_DIRS.add(rel)
            return self._send(201)
        if method == "PUT":
            if parent not in DAV_DIRS:
                return self._send(409, {"message": "parent missing"})
            created = rel not in DAV_FILES
            _etag_counter[0] += 1
            etag = f"e{_etag_counter[0]}"
            DAV_FILES[rel] = (body.encode(), time.time(), etag)
            self.send_response(201 if created else 204)
            self.send_header("ETag", f'"{etag}"')
            self.send_header("OC-ETag", f'"{etag}"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if method == "GET":
            if rel not in DAV_FILES:
                return self._send(404, {"message": "not found"})
            data, mtime, etag = DAV_FILES[rel]
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("ETag", f'"{etag}"')
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if method == "COPY":
            dest = unquote(urlparse(self.headers.get("Destination", "")).path)
            if not dest.startswith(DAV_ROOT) or rel not in DAV_FILES:
                return self._send(404, {"message": "not found"})
            drel = dest[len(DAV_ROOT):]
            if drel in DAV_FILES and self.headers.get("Overwrite", "T") == "F":
                return self._send(412, {"message": "exists"})
            data, _mtime, _etag = DAV_FILES[rel]
            _etag_counter[0] += 1
            DAV_FILES[drel] = (data, time.time(), f"e{_etag_counter[0]}")
            return self._send(201)
        if method == "DELETE":
            if rel in DAV_FILES:
                del DAV_FILES[rel]
                return self._send(204)
            if rel in DAV_DIRS and rel:
                DAV_DIRS.discard(rel)
                for f in [f for f in DAV_FILES if f.startswith(rel + "/")]:
                    del DAV_FILES[f]
                return self._send(204)
            return self._send(404, {"message": "not found"})
        return self._send(405, {"message": "method not allowed"})

    def do_GET(self): self._handle("GET")
    def do_PROPFIND(self): self._handle("PROPFIND")
    def do_MKCOL(self): self._handle("MKCOL")
    def do_COPY(self): self._handle("COPY")
    def do_PUT(self): self._handle("PUT")
    def do_POST(self): self._handle("POST")
    def do_DELETE(self): self._handle("DELETE")

if __name__ == "__main__":
    HTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()

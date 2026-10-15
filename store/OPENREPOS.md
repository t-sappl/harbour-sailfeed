# OpenRepos listing (to copy into openrepos.net)

**Title:** Sailfeed

**Category:** Applications › News (if not available: Internet) – check the category list on openrepos.net when adding the app

**Tags:** nextcloud, news, rss, feed reader, reader mode

**Platform:** Sailfish OS 5.x (tested on 5.1, Jolla Phone (2026)); packages for aarch64, armv7hl and i486

**Icon:** `design/harbour-sailfeed-512.png`

**Source code:** https://github.com/t-sappl/harbour-sailfeed

**License:** GPL-3.0-or-later

## Description for the OpenRepos page (HTML, 1.1)

> **Note for the author – do not paste this note.** The line "New in 1.1" is replaced with every version. Paste only the HTML inside the code block below into the description field. Reworked 15 Oct (bold label per line and slashes looked unsettled; clipboard → "Subscribe to website", search added, only the text size is per feed); order and wording as the author put it on the page. Also upload the 512 px icon (`design/harbour-sailfeed-512.png`) and screenshots (see below).

```html
<p><strong>Feed reader for Nextcloud News on Sailfish OS, with a clean reader mode</strong></p>

<p>I built Sailfeed for myself after moving partly from Android to Sailfish OS: my RSS feeds should be in the same state everywhere – read on the phone, read on the tablet. <a href="https://apps.nextcloud.com/apps/news">Nextcloud News</a> keeps them in sync, Sailfeed is the native reader for it, based on <a href="https://github.com/Huessenbergnetz/libfuoten">libfuoten</a>. I use it every day and share it for anyone who finds it useful.</p>

<p><strong>New in 1.1:</strong> subscribe to websites and feeds, search, links in reader mode, a calmer reader mode and various refinements.</p>

<p><strong>Feeds</strong></p>
<ul>
<li>Subscribe to website: paste the address of a site or an article – Sailfeed finds its feeds and shows their newest articles</li>
<li>Search in the articles on your device and in feed titles</li>
<li>Folders, favorite feeds, bookmarks, several sort orders; rename, move, delete</li>
</ul>

<p><strong>Reading</strong></p>
<ul>
<li>Article list grouped by day, page-wise scrolling with one hand, read articles greyed out automatically</li>
<li>Swipe from the preview to the full article in a clean reader mode – no ads, no cookie banners; font, colors and spacing as you like, text size remembered per feed</li>
<li>Bottom bar for one-handed reading: previous / next, text size, bookmark, light / dark, reader mode or original website</li>
<li>Links in articles open in reader mode or in the browser</li>
<li>Mark as read with one hand (push-up button) or by long press – always with undo</li>
<li>Works offline: read and bookmark changes are sent with the next sync</li>
<li>Settings backup in your Nextcloud files, app password in Sailfish Secrets, Sailjail, cover actions, portrait and landscape, English and German</li>
</ul>

<p><strong>Requirements:</strong> a Nextcloud server with the <a href="https://apps.nextcloud.com/apps/news">News</a> app installed and running. Sailfeed is only the reader and fetches no feeds itself.</p>

<p><strong>Use of AI:</strong> Developed with the assistance of an AI model (Claude by Anthropic). The developer specifies all features, makes all design and technical decisions, tests every change on a Jolla Phone (2026) and uses the app every day.</p>

<p><strong>Privacy:</strong> no tracking, no ads, no data to the developer (<a href="https://github.com/t-sappl/harbour-sailfeed/blob/main/PRIVACY.md">privacy policy</a>).<br>
<strong>License:</strong> GPL-3.0-or-later · <strong>Source and issues:</strong> <a href="https://github.com/t-sappl/harbour-sailfeed">github.com/t-sappl/harbour-sailfeed</a></p>
```

## Description (English, plain text)

Feed reader for Nextcloud News on Sailfish OS, with a clean reader mode. Reads the feeds of your own Nextcloud server in a native Silica interface.

Why it exists: I moved partly from Android to Sailfish OS and wanted my RSS feeds in the same state on both – read on the phone, read on the tablet. For that I now use Nextcloud News on my own Nextcloud, and for Sailfish OS I built this reader for myself, on the basis of libfuoten. After using it every day for a while, I want to share it with everyone and added a few comfort features for that.

A personal project – built for my own use and the features I need most. Bug reports are welcome on GitHub, but I can't promise support or feature requests.

Features:
- Article list with excerpt and thumbnail, grouped by day, page-wise scrolling, read articles greyed out; optionally unread only (read ones on request); older articles loadable
- Feed list (swipe right): bookmarks, folders and feeds with favicons and unread counts, "Other feeds" for feeds without a folder, empty folders too; sort by unread, name or activity; favorite feeds
- Subscribe to website: finds the feeds of a website or article (also from the web view), shows them with their newest articles; comment feeds and podcasts marked
- Search: articles on the device (titles and text, also read ones) and feed titles
- Feed management: folders, rename, move, delete
- Mark as read automatically (when scrolling on, or at once), with a push-up button for one hand, by long press (read/unread, bookmark, all above/below) or per feed and folder – all with undo
- Reader mode without ads, cookie banners and overlays (Mozilla Readability), preloaded for quick opening; font, text size (also per feed), line spacing, margins, colors (light/dark/sepia, quick light/dark toggle), images; sites that always open in the original; links in an article open in reader mode or in the browser (long press: the other way)
- Full article in reader mode, as the original website in the app, or in the browser; the preview can be skipped
- One-handed bar in preview and reader mode: previous/next article, text size, bookmark, open in browser
- Portrait and landscape
- Sign-in with the browser (Nextcloud Login Flow) or an app password; understandable error hints
- Offline-friendly: read and bookmark changes are queued and synced
- Settings backup in your own Nextcloud files: automatic after changes, earlier states kept, restore on another device with undo
- Privacy settings for the in-app browser's cookies and website data
- App password stored in Sailfish Secrets; runs in the Sailjail sandbox
- English and German

Requirements: a Nextcloud server with the **Nextcloud News** app installed – Sailfeed is only a client and does not fetch feeds itself. If News is not installed yet: as an admin, open Apps in your Nextcloud and enable "News" (section Multimedia); on hosted Nextclouds this works the same way if the provider allows apps. News needs Nextcloud's background jobs via cron so the feeds are updated. App page: https://apps.nextcloud.com/apps/news – installation guide: https://nextcloud.github.io/news/install/
Sign in with the browser, or with an app password (Nextcloud → Settings → Security → Devices & sessions).

Privacy: no tracking, no ads, no data to the developer. The app talks to your Nextcloud server (also for the settings backup in your files) and loads images and article pages from the websites of the articles. Details: https://github.com/t-sappl/harbour-sailfeed/blob/main/PRIVACY.md

Developed with the assistance of an AI model (Claude by Anthropic). The developer specifies all features, makes all design and technical decisions, tests every change on a Jolla Phone (2026) and uses the app every day.

Credits: libfuoten by Matthias Fehring (LGPL-3.0-or-later), Mozilla Readability (Apache-2.0); thanks to Fuoten, the original Nextcloud News client for Sailfish OS.

## Beschreibung (Deutsch, optional)

Feedreader für Nextcloud News unter Sailfish OS, mit einem aufgeräumten Lesemodus. Liest die Feeds des eigenen Nextcloud-Servers in einer nativen Silica-Oberfläche.

Warum es Sailfeed gibt: Ich bin teilweise von Android auf Sailfish OS umgestiegen und wollte meine RSS-Feeds auf beiden Systemen auf dem gleichen Stand haben – am Handy gelesen, am Tablet gelesen. Dafür nutze ich jetzt Nextcloud News auf meiner eigenen Nextcloud, und für Sailfish OS habe ich mir diesen Reader selbst gebaut, auf Basis von libfuoten. Nachdem ich ihn eine Zeit lang täglich verwendet habe, stelle ich ihn allen zur Verfügung und habe dafür noch einige Komfortfunktionen eingebaut.

Ein persönliches Projekt – für den eigenen Gebrauch und die Funktionen entwickelt, die ich am meisten brauche. Fehlermeldungen gerne auf GitHub, Support oder Funktionswünsche kann ich aber nicht versprechen.

Voraussetzung: ein Nextcloud-Server mit installierter App **Nextcloud News** – Sailfeed ist nur der Client und lädt selbst keine Feeds. Ist News noch nicht installiert: als Admin in der Nextcloud unter „Apps“ die App „News“ (Bereich Multimedia) aktivieren; bei gehosteten Nextclouds genauso, sofern der Anbieter Apps erlaubt. News braucht die Hintergrundjobs der Nextcloud per Cron, damit die Feeds aktualisiert werden. App-Seite: https://apps.nextcloud.com/apps/news – Installationsanleitung (englisch): https://nextcloud.github.io/news/install/

Funktionen (kurz): Artikelliste mit Vorschaubild und seitenweisem Scrollen, Feedliste mit Ordnern, Lesezeichen und Favoriten, Feeds und Ordner verwalten, automatisch und einhändig als gelesen markieren, Lesemodus ohne Werbung und Cookie-Banner (Schrift, Größe, Farben einstellbar), Hoch- und Querformat, Anmeldung im Browser, offline nutzbar, Sicherung der Einstellungen in der eigenen Nextcloud. Deutsch und Englisch.

## Changelog 1.1.0 (paste into the OpenRepos changelog field)

- New: Subscribe to website – finds the feeds of a website or article and shows them with their newest articles; recognises feeds you already subscribe to; also from the web view pulley
- New: Search in the articles on the device (titles and text, also read ones) and in feed titles – in the pulley of the article list and the feed list
- New: links in articles open in reader mode or in the browser, long press for the other way
- New icon
- Easier first start: explanations on the sign-in page and in empty lists, tips for the hidden gestures
- Reading: reader mode ↔ original website in the bottom bar, shorter pulley with share; calmer reader mode, smoother previous/next; calmer article preview
- Article list: better paging, reliable mark-as-read at the end
- Settings: main page with sub-pages (incl. "Nextcloud" for account and backup), live preview of the reader mode
- Better readable in light ambiences

## Changelog 1.0.0 (paste into the OpenRepos changelog field)

First public release.
- Article list, feed list with folders, bookmarks and favorite feeds, feed management
- Automatic and one-handed "mark as read", all with undo
- Reader mode with preloading and many display settings; preview can be skipped
- Sign-in with the browser or an app password
- Settings backup in your own Nextcloud files
- Portrait and landscape; English and German

## Screenshots (in `screenshots/`, taken 11 and 15 Oct; also shown in the README)

Upload in this order (as in the README): `article-list.png`, `feeds.png`, `search.png`, `preview.png`, `reader.png`, `reader2.png` (reader mode with light colors), `settings.png`.

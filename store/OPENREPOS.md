# OpenRepos listing (to copy into openrepos.net)

**Title:** Sailfeed

**Category:** Applications › News (if not available: Internet) – check the category list on openrepos.net when adding the app

**Tags:** nextcloud, news, rss, feed reader, reader mode

**Platform:** Sailfish OS 5.x (tested on 5.1, Jolla Phone (2026)); packages for aarch64 (and armv7hl, if built – see TODO.md "Publishing")

**Icon:** `design/harbour-sailfeed-512.png`

**Source code:** https://github.com/t-sappl/harbour-sailfeed

**License:** GPL-3.0-or-later

## Description (English)

Sailfeed is a client for the Nextcloud News app: read the feeds of your own Nextcloud server in a native Silica interface, with a clean reader mode instead of cluttered websites.

Why it exists: I moved partly from Android to Sailfish OS and wanted my RSS feeds in the same state on both – read on the phone, read on the tablet. For that I now use Nextcloud News on my own Nextcloud, and for Sailfish OS I built this reader for myself, on the basis of libfuoten. After using it every day for a while, I want to share it with everyone and added a few comfort features for that.

A personal project – built for my own use and the features I need most. Bug reports are welcome on GitHub, but I can't promise support or feature requests.

Features:
- Article list with excerpt and thumbnail, grouped by day, page-wise scrolling, read articles greyed out; optionally unread only (read ones on request); older articles loadable
- Feed list (swipe right): bookmarks, folders and feeds with favicons and unread counts, "Other feeds" for feeds without a folder, empty folders too; sort by unread, name or activity; favorite feeds
- Feed management: add feeds (address from the clipboard) and folders, rename, move, delete
- Mark as read automatically (when scrolling on, or at once), with a push-up button for one hand, by long press (read/unread, bookmark, all above/below) or per feed and folder – all with undo
- Reader mode without ads, cookie banners and overlays (Mozilla Readability), preloaded for quick opening; font, text size (also per feed), line spacing, margins, colors (light/dark/sepia, quick light/dark toggle), images; sites that always open in the original
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

Developed with the assistance of an AI model (Claude by Anthropic). The author specifies all features, makes all design and technical decisions, tests every change on a Jolla Phone (2026) and uses the app every day.

Credits: libfuoten by Matthias Fehring (LGPL-3.0-or-later), Mozilla Readability (Apache-2.0); thanks to Fuoten, the original Nextcloud News client for Sailfish OS.

## Beschreibung (Deutsch, optional)

Sailfeed ist ein Client für die Nextcloud-News-App: die Feeds des eigenen Nextcloud-Servers in einer nativen Silica-Oberfläche lesen, mit einem aufgeräumten Lesemodus statt überladener Websites.

Warum es Sailfeed gibt: Ich bin teilweise von Android auf Sailfish OS umgestiegen und wollte meine RSS-Feeds auf beiden Systemen auf dem gleichen Stand haben – am Handy gelesen, am Tablet gelesen. Dafür nutze ich jetzt Nextcloud News auf meiner eigenen Nextcloud, und für Sailfish OS habe ich mir diesen Reader selbst gebaut, auf Basis von libfuoten. Nachdem ich ihn eine Zeit lang täglich verwendet habe, stelle ich ihn allen zur Verfügung und habe dafür noch einige Komfortfunktionen eingebaut.

Ein persönliches Projekt – für den eigenen Gebrauch und die Funktionen entwickelt, die ich am meisten brauche. Fehlermeldungen gerne auf GitHub, Support oder Funktionswünsche kann ich aber nicht versprechen.

Voraussetzung: ein Nextcloud-Server mit installierter App **Nextcloud News** – Sailfeed ist nur der Client und lädt selbst keine Feeds. Ist News noch nicht installiert: als Admin in der Nextcloud unter „Apps“ die App „News“ (Bereich Multimedia) aktivieren; bei gehosteten Nextclouds genauso, sofern der Anbieter Apps erlaubt. News braucht die Hintergrundjobs der Nextcloud per Cron, damit die Feeds aktualisiert werden. App-Seite: https://apps.nextcloud.com/apps/news – Installationsanleitung (englisch): https://nextcloud.github.io/news/install/

Funktionen (kurz): Artikelliste mit Vorschaubild und seitenweisem Scrollen, Feedliste mit Ordnern, Lesezeichen und Favoriten, Feeds und Ordner verwalten, automatisch und einhändig als gelesen markieren, Lesemodus ohne Werbung und Cookie-Banner (Schrift, Größe, Farben einstellbar), Hoch- und Querformat, Anmeldung im Browser, offline nutzbar, Sicherung der Einstellungen in der eigenen Nextcloud. Deutsch und Englisch.

## Changelog 1.0.0 (paste into the OpenRepos changelog field)

First public release.
- Article list, feed list with folders, bookmarks and favorite feeds, feed management
- Automatic and one-handed "mark as read", all with undo
- Reader mode with preloading and many display settings; preview can be skipped
- Sign-in with the browser or an app password
- Settings backup in your own Nextcloud files
- Portrait and landscape; English and German

## Screenshots (to take on the device: power + volume down)

1. Article list
2. Feed list
3. Article preview
4. Reader mode
5. Settings
6. Optional: landscape (article list or reader mode)

Also put them into `screenshots/` and uncomment the block in the README.

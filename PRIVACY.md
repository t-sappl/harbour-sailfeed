# Sailfeed – Privacy Policy

Last updated: 10 October 2026

Sailfeed is a client for the Nextcloud News app on Sailfish OS. This policy explains which data the app stores on your device and which online services it contacts.

## Summary

- No tracking, no analytics, no advertising.
- The developer does not run any server and does **not** receive any data from the app.
- Your account and your articles are exchanged only with **your own** Nextcloud server. The optional backup of your settings is stored there too, in your own files.
- To show images and the reader mode, the app loads content directly from the websites of the articles. Like every internet connection, each of them sees your device's IP address.

## Data stored on your device

Files are inside the app's own sandbox folders (`~/.config`, `~/.local/share` and `~/.cache`, each under `org.sappl/harbour-sailfeed`). Most settings of the app are kept in the system's settings database (dconf, under `/apps/harbour-sailfeed`).

| Data | Purpose | How to delete |
|---|---|---|
| Server address, user name, your internal user ID on the server, time of the last sync and backup | Sign-in to your Nextcloud, settings backup | Settings → Nextcloud → "Sign out", or uninstalling |
| App password | Sign-in to your Nextcloud | Stored encrypted in the system's secure storage (Sailfish Secrets), only readable by this app; removed with "Sign out" |
| Folders, feeds and articles (SQLite database), including read and bookmark changes not yet sent | Reading, also offline | Read articles (not bookmarks) older than the chosen time are deleted at the start (Settings → Article list → "Delete read articles after"); "Sign out" empties the database; uninstalling removes it |
| Settings (dconf and the settings file), favorite feeds | Your choices in the app | The settings file goes with uninstalling; the dconf values with `dconf reset -f /apps/harbour-sailfeed/` in a terminal |
| Settings before the last restore (`settings-before-restore.json`) | Undoing a restore of the settings backup | Uninstalling |
| Image cache (thumbnails, article images, favicons; max. 100 MB, adjustable 50–500 MB) | Faster display, less data traffic | Settings → Article list → "Clear image cache"; uninstalling |
| Data of the in-app browser (Sailfish WebView: cookies, site data, cache) | Websites opened in the app, e.g. your choice on a consent banner | Settings → Privacy → "Delete cookies and website data" (done at the next start), or "Delete cookies at every start"; uninstalling |
| Preloaded web pages for the reader mode | Faster reader mode | Kept in memory only (last 8 pages), gone when the app closes |

## Online services the app contacts

### Your Nextcloud server
- **When:** signing in, every sync (at start, "Refresh", the cover action, and when the app goes to the background with unsent changes). "Sign in with the browser" opens your Nextcloud's login page in the system browser; after you grant access there, your Nextcloud creates an app password for Sailfeed (listed and revocable in Nextcloud → Settings → Security).
- **What:** your user name and app password (HTTP Basic authentication, use `https://`), the request for folders, feeds and new or changed articles, and the read and bookmark status you changed. The app identifies itself as `Sailfeed/<version> (Sailfish OS)`.
- Nobody else receives your account data.

### Settings backup in your Nextcloud files
- **When:** "Back up now" and "Restore …" in Settings → Nextcloud; automatically a few seconds after you change a backed-up setting and when the app goes to the background (setting "Back up automatically", on by default; only after the first backup or restore on this device); after signing in, the app looks whether a backup exists and offers it.
- **Where:** the folder `Sailfeed` in your own Nextcloud files (WebDAV, `remote.php/dav/files/<your user ID>/Sailfeed/`): `sailfeed-settings.json` (the latest backup) and copies of earlier states (`sailfeed-settings-YYYY-MM-DD_HHMMSS.json`: before a backup replaces the latest one, the previous state is kept, at most one copy per hour, the newest 10). To find the WebDAV path, the app asks your server once for your internal user ID (`ocs/v2.php/cloud/user`). The same user name and app password as for the News app are used.
- **What:** your settings in the app (mark as read, sort order, how articles open, reader mode look, privacy switches), the IDs and titles of your favorite feeds, the sites you open in the original, optionally the text sizes (also per feed, by feed ID), the time of the backup, the app version and the device model (e.g. "Jolla C2"). No articles, no account data, no password.
- **How to delete:** delete the folder `Sailfeed` in your Nextcloud; turn off "Back up automatically" to stop further backups.

### The websites of the articles and their image servers
- **Thumbnails, images and favicons:** loaded from the addresses stored in the articles and feeds when the article list, the feed list or an article is shown.
- **Reader mode (default for "Full article"):** while the article preview is open (or right away with "Skip the preview"), the article's web page is downloaded in the background; when you open it, it is shown without its scripts, ads and banners. The following article is preloaded the same way. These downloads identify themselves like a Firefox browser on Sailfish OS.
- **Cookies:** the in-app browser stores cookies like any web browser. By default they are also sent with the background downloads of the same website, so that e.g. your choice on a consent banner ("with ads or subscription") is respected there, too. The app only reads them; it does not send them anywhere else. Settings → Privacy: turn this off, delete cookies and website data, or delete them at every start.
- **Original website:** if you choose "Show original website" (or set it as default), the complete page is loaded in the in-app browser, including its scripts, ads and trackers, under the website's own privacy policy.
- **Subscribe to website:** the address you enter (or the page shown in the web view) is loaded directly from that website, and – to find its feeds – its parent pages and a few usual feed addresses of the same site (at most 30 requests per search). Nothing of this goes to the developer; subscribing then happens on your Nextcloud.
- **Links in articles:** a link you tap in an article opens in reader mode in the app (default; setting "Links in articles") – that website is contacted the same way as an article's – or in the external browser (setting, or a long press on the link).

### External browser and sharing
"Open in browser", "External browser", links opened in the browser and "Share" hand the link to the system browser or to the sharing target you pick; from then on their privacy policies apply.

## Contact

Questions and problems: https://github.com/t-sappl/harbour-sailfeed/issues

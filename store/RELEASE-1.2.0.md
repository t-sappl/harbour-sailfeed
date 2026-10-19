# GitHub release text – Sailfeed 1.2.0

Tag `v1.2.0`, title "Sailfeed 1.2.0". Paste everything below the line into the release description and attach the RPMs (aarch64, armv7hl, i486).

---

### New
- **Feed settings** – long press a feed in the feed list ("Feed settings …") or use "Feed settings" in the pulley of a feed's article list. Name and folder (rename and move moved here), and the feed's own list view, preview (show or skip) and full article mode (reader mode, original website, browser) – each "like the general setting" until you change it. Also: reset the feed's text size, copy the feed's address.
- **Compact list view** – title and source only with a small thumbnail, about twice as many articles per page (Settings → Article list, or per feed).
- **Oldest first** – the article list can start with the oldest article (Settings → Article list → Order).
- **Swipe to the next article** – in reader mode, swipe left for the next article; swiping right still goes back. A one-time tip shows it.

### Reading
- Articles whose feed sends no text (e.g. news.ORF.at) open the full article directly instead of an empty preview.
- Slow connections: the reader mode keeps waiting while the page is still arriving instead of starting over. After 10 seconds it offers to read the text from the feed or to open the browser. No more blank page with busy indicators while the reader page loads its images.

### Storage
- Read articles older than 1, 3 or 6 months or 1 year are deleted when the app starts, at most once a day. Bookmarks, unread articles and changes not yet sent to the server always stay. New installations: 3 months; updates: "Never", with a one-time hint. The settings show the number of articles and the database size.
- Image cache: size adjustable (50–500 MB) and "Clear image cache".

### Other
- The new settings are part of the settings backup in your Nextcloud.

### Install
Through OpenRepos / Storeman, or download the RPM for your device below (aarch64 for current devices such as the Jolla Phone (2026), armv7hl for older 32-bit devices, i486 for the emulator and Intel tablets). Sailfeed needs a Nextcloud server with the **News** app.

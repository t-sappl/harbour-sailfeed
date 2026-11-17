Sailfeed 1.4.0 – feed reader for Nextcloud News on Sailfish OS.

**New**
- Import and export your feeds as an OPML file (Settings → Import and export feeds) – in your Nextcloud files, in Downloads or shared; import a file of another feed reader with its folders, feeds you have already are skipped
- feed:// links open "Subscribe to website" with the feed
- Mute a feed (long press or feed settings): its articles stay out of "All articles" and its folder and are not counted as unread; the feed itself and the search still show them
- New folder: choose the feeds that belong into it; deleting a folder with feeds asks whether to unsubscribe its feeds too or keep them under "Other feeds"
- Sort field "Sorted: … ›" under the header of the article list and the feed list

**Improved**
- Faster reader mode: the article is read from a lean copy of the page, going back shows it at once, the next and the previous article are loaded in advance
- Faster lists and search, smaller thumbnails, a quicker database; new articles of a sync appear at their place in the list
- Tidier buttons and settings pages; search results open as set in the settings and start in the current feed or folder

**Fixed**
- Reader mode could show the previous article's text after next/previous
- Unread counts with muted feeds, auto mark-as-read in short lists, the article list position after returning from the reader, the feed list position, a stray banner at start

New permission: Downloads (only for the OPML files there).

**Install:** `aarch64` for current devices, `armv7hl` for older 32-bit ones, `i486` for the emulator and Intel tablets. Also on OpenRepos (Storeman: "Sailfeed").

Developed by Thomas Sappl together with Claude (Anthropic) – analysis, design, code, reviews and tests.

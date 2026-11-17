# GitHub release text – Sailfeed 1.4.0

Tag `v1.4.0`, title "Sailfeed 1.4.0". Paste everything below the line into the release description and attach the RPMs (aarch64, armv7hl, i486).

---

### New
- **Import and export feeds** (Settings → Import and export feeds) – your feeds and folders as an **OPML file**, the format other feed readers use:
  - save it in your Nextcloud files (folder `Sailfeed`, next to the settings backup), in **Downloads**, or share it, e.g. by e-mail;
  - import an OPML file of another feed reader from the same Nextcloud folder or from Downloads. Sailfeed shows what is new and what you have already, keeps the folders if you like, subscribes the feeds one after another on your Nextcloud – with progress, a stop button and a list of the feeds that could not be added. The import goes on when you leave the page.
- **Feed links** – a `feed://` link opens **"Subscribe to website"** with that feed, also when Sailfeed is closed.

- **Faster reader mode** – the article is read from a lean copy of the page (no scripts, style sheets or ads loading) with shorter waits; going back to an article shows it at once, the next and the previous article are loaded in advance, and images further down load while you scroll.

- **Faster lists** – articles are loaded without their texts, excerpts and thumbnails are prepared once in the background, list thumbnails are smaller files, the database waits less while syncing and marking, the search runs in the background, and the web view is only created when you open it.

### Fixes
- New articles of a sync appear at their place in the list (they came at the end).
- Changing the selection while the list loads shows the right articles.
- Articles dated in the future (wrong time zone in a feed) are shown instead of only counted.
- "Load older articles" counts correctly while a sync runs.

### Other
- Action rows that cannot be used right now (e.g. "Clear image cache" when it is empty) are dimmed.
- New permission **Downloads** – only used for the OPML files there, when you choose it.

### Install
Through OpenRepos / Storeman, or download the RPM for your device below (aarch64 for current devices such as the Jolla Phone (2026), armv7hl for older 32-bit devices, i486 for the emulator and Intel tablets). Sailfeed needs a Nextcloud server with the **News** app.

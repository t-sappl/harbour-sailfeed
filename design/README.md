# Design

- `harbour-sailfeed.svg` – app icon (source, since 13 Oct 2026): a boat whose sail is the feed
  symbol (dot and two arcs) in the Sailfish OS "drop" (three rounded corners, bottom left pointed).
  The arcs run nearly parallel to the drop's round part. Subtle depth: outer rim with a diagonal
  gradient (`#ffa53c` → `#d0610f`), slightly smaller inner drop (`#f99327` → `#dd6e14`) with a
  faint shine, soft shadow under the white motif. Replaced the teal "sail above three lines",
  which was too close to the Sailwave icon.
- `proposals/` – the icon proposals of 13 Oct (A text sail, B newspaper boat, C feed sail and its
  refinements C1/C2) with comparison images; C2 was chosen.
- `harbour-sailfeed-512.png` – 512 px version for store listings (OpenRepos).
- `render-icons.py` – renders the SVG to `icons/*/harbour-sailfeed.png` (86, 108, 128, 172 px).
  Run it after changing the SVG.

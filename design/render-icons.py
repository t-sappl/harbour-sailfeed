#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Renders design/harbour-sailfeed.svg to the app icons (86, 108, 128, 172 px)
# and a 512 px PNG for store listings. Renders large with Chromium (Playwright)
# and scales down with ImageMagick, which gives cleaner small sizes.
#
#   python3 design/render-icons.py        (from the project directory)
#
# Alternative with Inkscape, per size:
#   inkscape -w 172 -h 172 design/harbour-sailfeed.svg -o icons/172x172/harbour-sailfeed.png
import os, subprocess, tempfile
from playwright.sync_api import sync_playwright

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
svg = open(os.path.join(here, 'harbour-sailfeed.svg')).read()
big = 1024
with tempfile.TemporaryDirectory() as tmp:
    master = os.path.join(tmp, 'master.png')
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page(viewport={'width': big, 'height': big})
        pg.set_content('<html><body style="margin:0;background:transparent">'
                       + svg.replace('width="172" height="172"', 'width="%d" height="%d"' % (big, big))
                       + '</body></html>')
        pg.screenshot(path=master, omit_background=True, clip={'x': 0, 'y': 0, 'width': big, 'height': big})
        b.close()
    for s in (86, 108, 128, 172):
        out = os.path.join(root, 'icons', '%dx%d' % (s, s), 'harbour-sailfeed.png')
        subprocess.check_call(['convert', master, '-filter', 'Lanczos', '-resize', '%dx%d' % (s, s), '-strip', 'PNG32:' + out])
    subprocess.check_call(['convert', master, '-resize', '512x512', '-strip', 'PNG32:' + os.path.join(here, 'harbour-sailfeed-512.png')])
print('icons rendered')

# Desktop backend test

Tests the C++ layer (HtmlUtils, ArticleModel, FeedModel, FeedManager, Configuration, AccountManager incl. Login Flow v2, Prefetcher/GeckoCookieJar, NextcloudFiles) and libfuoten's sync/queue against `mocknews.py`, a minimal Nextcloud News API v1-2 mock (user `thomas`, app password `secret`; also feed/folder management, "load older" offset requests, Login Flow v2, a cookie echo page, the OCS user ID and a small WebDAV server whose user ID "tsappl" differs from the login name). The QML UI is not covered.

```
cmake -S tests -B build-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test -j
python3 tests/mocknews.py 8765 > mock.log &
rm -rf /tmp/sfhome && mkdir -p /tmp/sfhome
HOME=/tmp/sfhome XDG_CONFIG_HOME=/tmp/sfhome/.config XDG_DATA_HOME=/tmp/sfhome/.local/share \
    XDG_CACHE_HOME=/tmp/sfhome/.cache QT_QPA_PLATFORM=offscreen \
    MOCK_PORT=8765 ./build-test/backendtest 2>/dev/null
```

Start every run with an empty home directory and a freshly started mock server (the mock keeps state, and its article ages are relative to its start time). `mock.log` lists every request the client sent, e.g. the bundled `items/read/multiple` call.

After changes that touch the storage or threads, run the test several times in a row (a threading bug in libfuoten showed up only in about one run in ten).

## qmake pitfalls

`tests/check-qmake-pitfalls.sh` (from the project root, `QTDIR` = a Qt 5 with `moc`) catches what only the SDK build would show: source file names also used by libfuoten (one object folder in qmake) and headers whose `moc_*.cpp` does not compile on its own. Must print `ok`.

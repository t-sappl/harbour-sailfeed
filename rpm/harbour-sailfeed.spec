# SPDX-License-Identifier: GPL-3.0-or-later
#
# Personal project, published on GitHub and OpenRepos (not in the Jolla
# Store) - but kept within the Harbour rules (allowed Requires, libraries and
# QML imports; see DESIGN.md).
Name:       harbour-sailfeed
Summary:    Feed reader for Nextcloud News on Sailfish OS
Version:    1.3.0
Release:    1
# App GPL-3.0-or-later; bundled libfuoten LGPL-3.0-or-later (compiled in),
# Readability.js Apache-2.0 (Qt resource)
License:    GPL-3.0-or-later AND LGPL-3.0-or-later AND Apache-2.0
URL:        https://github.com/t-sappl/harbour-sailfeed
Source0:    %{name}-%{version}.tar.bz2
# Unversioned on purpose: Harbour only allows unversioned Requires
Requires:   sailfishsilica-qt5
# thumbnails and article images
Requires:   qt5-plugin-imageformat-jpeg
Requires:   qt5-plugin-imageformat-gif
# No WebP plugin here: Harbour does not allow qt5-qtimageformats-plugin-webp
# ("Dependency not allowed", sfdk check 15 Oct); it is installed on the
# Jolla Phone anyway
# in-app website view (Sailfish.WebView, Gecko)
Requires:   sailfish-components-webview-qt5
BuildRequires: pkgconfig(sailfishapp) >= 1.0.2
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5Sql)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(sailfishsecrets)
BuildRequires: qt5-qttools-linguist
BuildRequires: desktop-file-utils

%description
Sailfeed reads the feeds of your own Nextcloud server (News app) in a native
Silica interface: article list with thumbnails, feed list with unread counts,
automatic and one-handed "mark as read", a reader mode without ads and
cookie banners, and a settings backup in your own Nextcloud files.
A personal project, developed for the author's own use.
Uses libfuoten (LGPL-3.0-or-later) by Matthias Fehring and Mozilla
Readability (Apache-2.0).

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install
desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications \
    %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

Name:    latte-dock-qt6
Version: 0.10.77
Release: 1%{?dist}
Summary: Dock and panel for the Plasma 6 desktop

# Derived from the tree, restricted to what actually ships in the binary RPM:
# GPL-2.0-or-later for the bulk, LGPL-2.0-or-later for the files borrowed from
# Plasma, LGPL-2.1-only for the shell package's .svgz artwork (that rule lives
# only in .reuse/dep5, the files carry no SPDX header), and the dual arm on
# app/wm/tasktools.cpp. Upstream writes that last one with a third option,
# LicenseRef-KDE-Accepted-LGPL, which Fedora has no approved identifier for;
# Fedora's own KDE packages collapse it to the concrete pair, so this does too.
License: GPL-2.0-or-later AND LGPL-2.0-or-later AND LGPL-2.1-only AND (LGPL-2.1-only OR LGPL-3.0-only)
URL:     https://github.com/CaptSilver/latte-dock-qt6

# tools/scripts/make-srpm.sh packs this from `git archive` and rewrites the
# Version above to the real stamp before rpmbuild ever parses the spec. The
# in-tree value is the base version from CMakeLists.txt, so `rpmspec -q` and
# `dnf builddep rpm/latte-dock-qt6.spec` still work on a plain checkout.
Source0: %{name}-%{version}.tar.gz

# The test suite is gated behind BUILD_TESTING and pulls deps nothing else needs.
# Opt in with `rpmbuild --with check`.
%bcond_with check

# This package owns /usr/bin/latte-dock. Fedora ships no latte-dock today, but
# the two could not coexist if one ever returns.
Provides:  latte-dock = %{version}-%{release}
Conflicts: latte-dock

BuildRequires: cmake
BuildRequires: gcc-c++
# KF6I18nMacros does find_package(Gettext REQUIRED) at include time.
BuildRequires: gettext
# Fedora's cmake macro hard-codes -G Ninja, so the generator is not optional.
# Fedora calls the package ninja-build and everyone else calls it ninja; the
# file is the one name all of them agree on.
BuildRequires: /usr/bin/ninja
# openSUSE ships this as kf6-extra-cmake-modules, which provides the bare name.
BuildRequires: extra-cmake-modules

# Everything below is named after the CMake package the build actually calls
# find_package() on, not after a distro's package. The same library is
# qt6-qtbase-devel on Fedora, qt6-core-devel on openSUSE and lib64qt6core-devel
# on Mageia, but all three generate cmake(Qt6Core) out of the installed
# Qt6CoreConfig.cmake, so the virtual provide is the only spelling that resolves
# everywhere. Keep this list in step with CMakeLists.txt.
BuildRequires: cmake(Qt6DBus)
BuildRequires: cmake(Qt6Gui)
BuildRequires: cmake(Qt6Qml)
BuildRequires: cmake(Qt6Quick)
BuildRequires: cmake(Qt6Widgets)
BuildRequires: cmake(Qt6WaylandClient)
# Qt6Test is in the unconditional COMPONENTS list, so it is needed even without
# --with check.
BuildRequires: cmake(Qt6Test)

BuildRequires: cmake(KF6Archive)
BuildRequires: cmake(KF6Config)
BuildRequires: cmake(KF6CoreAddons)
BuildRequires: cmake(KF6Crash)
BuildRequires: cmake(KF6DBusAddons)
BuildRequires: cmake(KF6GlobalAccel)
BuildRequires: cmake(KF6GuiAddons)
BuildRequires: cmake(KF6I18n)
BuildRequires: cmake(KF6IconThemes)
BuildRequires: cmake(KF6KIO)
BuildRequires: cmake(KF6KirigamiPlatform)
BuildRequires: cmake(KF6NewStuff)
BuildRequires: cmake(KF6Notifications)
BuildRequires: cmake(KF6Package)
BuildRequires: cmake(KF6Service)
BuildRequires: cmake(KF6Svg)
BuildRequires: cmake(KF6WidgetsAddons)
BuildRequires: cmake(KF6WindowSystem)
BuildRequires: cmake(KF6XmlGui)
# Not named anywhere in this project's CMake. LibNotificationManagerConfig.cmake
# does find_dependency(KF6ItemModels) but plasma-workspace-devel never requires
# it, so configure fails without this line.
BuildRequires: cmake(KF6ItemModels)
%if 0%{?fedora}
# Fedora 44 GA ships 6.25.0 while its LibNotificationManager wants 6.26.0, so a
# bad resolution should fail while solving deps rather than halfway through
# cmake. The floor is Fedora's problem alone -- openSUSE Leap is on 6.16 and
# Mageia on 6.22, and both carry a LibNotificationManager matched to them.
BuildRequires: cmake(KF6ItemModels) >= 6.26.0
%endif

# One package, three configs on Fedora: Plasma, PlasmaQuick and the
# plasma_install_package() macro the containment, plasmoid and shell packages
# are installed with.
BuildRequires: cmake(Plasma)
BuildRequires: cmake(PlasmaQuick)
BuildRequires: cmake(KSysGuard)
BuildRequires: cmake(PlasmaActivities)
BuildRequires: cmake(PlasmaActivitiesStats)
# The launcher badge/progress code.
BuildRequires: cmake(LibNotificationManager)

# find_package(KWayland) resolves to a Plasma::KWaylandClient target.
BuildRequires: cmake(KWayland)
BuildRequires: cmake(LayerShellQt)
BuildRequires: cmake(PlasmaWaylandProtocols)
# Wayland::Client plus wayland-scanner, which ECM needs next to qtwaylandscanner.
BuildRequires: pkgconfig(wayland-client)

%if %{with check}
# tests/sceneprobe is added unconditionally under BUILD_TESTING and wants
# Qt6::GuiPrivate and find_package(Vulkan REQUIRED). Vulkan would resolve
# through qtbase's pkgconfig(vulkan) chain by luck; name both halves instead.
# Fedora spellings, which EPEL shares; the only place the suite is actually run.
%if 0%{?fedora} || 0%{?rhel}
BuildRequires: qt6-qtbase-private-devel
BuildRequires: vulkan-headers
BuildRequires: vulkan-loader-devel
%endif
%endif

# The QML imports below have no ELF linkage anywhere in the package, so nothing
# in the automatic dependency generator can reach them, and unlike the build
# deps there is no cmake() provide to hide the naming differences behind -- an
# import is satisfied by whichever package happens to ship that directory. So
# they are spelled per distro, and a distro only gets the names verified to
# exist in its repos. Anything unlisted below is a gap, not a decision that the
# import is unnecessary.
# EPEL 10 ships this stack under Fedora's names, and defines %%rhel rather than %%fedora.
%if 0%{?fedora} || 0%{?rhel}
# org.kde.taskmanager and org.kde.plasma.private.shell:
Requires: plasma-workspace
# org.kde.kquickcontrolsaddons, org.kde.draganddrop, org.kde.graphicaleffects:
Requires: kf6-kdeclarative
# org.kde.plasma.plasma5support, imported by the task manager applet's main.qml:
Requires: plasma5support
# Both themes own the directories this package drops icons into, and both carry
# the file trigger that rebuilds the icon cache, which is why this package needs
# no icon-cache scriptlet of its own.
Requires: breeze-icon-theme
Requires: hicolor-icon-theme
# org.kde.plasma.private.volume, used by the per-task audio indicator only.
Recommends: plasma-pa
# org.kde.pipewire, loaded lazily for live window-thumbnail previews.
Recommends: kpipewire
%endif

%if 0%{?suse_version}
Requires: plasma6-workspace
Requires: kf6-kdeclarative-imports
Requires: plasma5support6
Requires: hicolor-icon-theme
Recommends: plasma6-pa
%endif

%if 0%{?mgaversion}
Requires: plasma-workspace
Requires: plasma5support
Requires: hicolor-icon-theme
Recommends: plasma-pa
Recommends: kpipewire
%endif

%description
A dock and panel for the Plasma desktop, with parabolic icon zoom, its own
layout system, per-dock running indicators, and a task manager that keeps its
state separate from the Plasma shell's.

This is the Qt 6 / KDE Frameworks 6 / Plasma 6 port of Latte Dock, which
upstream stopped maintaining at Plasma 5. It is developed against Wayland
sessions; X11 is not tested.

%prep
%autosetup -n %{name}-%{version}

%build
# BUILD_TESTING defaults to ON via ECM's KDECMakeSettings, so it has to be
# turned off explicitly or every build drags in the test tree.
%cmake -DBUILD_TESTING:BOOL=%{?with_check:ON}%{!?with_check:OFF}
%if %{with check}
# Twelve tests link the latte-dock executable's own object files, which
# tests/CMakeLists.txt collects with a file(GLOB_RECURSE) over the build tree.
# That glob runs at configure time, so in a fresh build directory it matches
# nothing and those tests link against an empty object list -- undefined
# references to everything they were supposed to exercise. Build the app first,
# then reconfigure so the glob sees the objects. Incremental developer trees
# never hit this because the objects are already there from a previous build.
%cmake_build --target latte-dock
%cmake -DBUILD_TESTING:BOOL=ON
%endif
%cmake_build

%install
%cmake_install

# ki18n_install() returns silently when po/ is missing or KF_SKIP_PO_PROCESSING
# is set, and the catalogs come from an ALL custom target that a targeted build
# never runs. Each of those paths installs an empty locale tree, %%find_lang
# writes an empty list, %%files accepts it, and the package ships no translations
# without a single error. 40 languages x 6 domains = 235 catalogs; anything far
# below that means the generation step did not happen.
mo_count=$(find %{buildroot}%{_datadir}/locale -name '*.mo' 2>/dev/null | wc -l)
if [ "$mo_count" -lt 200 ]; then
    echo "only $mo_count .mo files installed, expected ~235 -- catalogs were not generated" >&2
    exit 1
fi

# --all-name because there are six gettext domains, not one: latte-dock,
# two latte_indicator_*, two plasma_applet_*, and plasma_containmentactions_*.
%find_lang %{name} --all-name

%check
%if %{with check}
# The suite is Qt Quick throughout and has no QPA plugin in a chroot. Several
# tests build a real Latte::Corona, which resolves its shell package through
# KPackage -- point XDG_DATA_DIRS at the tree just staged into the buildroot or
# those tests fail on a null layouts manager.
export QT_QPA_PLATFORM=offscreen
export XDG_DATA_DIRS=%{buildroot}%{_datadir}:%{_datadir}

# A private, empty HOME, and not just for hygiene: Latte treats any layout
# template found under $HOME or the temp dir as a user template rather than a
# shipped one (Data::Layout::isSystemTemplate). mock's builder home is an
# ancestor of the buildroot, so with the real HOME the templates that were just
# installed come back classified as custom and templatesmanagertest fails.
export HOME=$(mktemp -d)
unset XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME XDG_STATE_HOME

# Deliberately bus-free rather than wrapped in dbus-run-session. Every test here
# passes without a session bus, and with one, D-Bus-activated daemons inherit the
# test's stdout and can leave the build waiting on a pipe that never closes.
# Unsetting the address makes that true on a developer machine too, where the
# build may well have inherited a live bus. --timeout is the backstop.
unset DBUS_SESSION_BUS_ADDRESS

# tests/CMakeLists.txt sets no ctest labels, so the two suites that cannot run
# here are excluded by name:
#   qmlloadcompile installs the tree to a scratch dir and compiles every shipped
#     QML file, which needs the runtime imports (org.kde.taskmanager,
#     org.kde.kquickcontrolsaddons, org.kde.plasma.plasma5support) -- runtime
#     Requires, not build deps.
#   qmlinteraction has cases importing org.kde.latte.components, which only
#     resolves once the package is installed for real.
# Everything else runs: 69 of the 71 registered tests.
%ctest --timeout 300 --exclude-regex '^(qmlloadcompile|qmlinteraction)$'
%endif

%files -f %{name}.lang
# CMake installs no licence text at all, so these are staged out of the unpacked
# source. The glob flattens the REUSE tree into /usr/share/licenses/latte-dock-qt6/;
# CC0-1.0 and LicenseRef-KDE-Accepted-LGPL ride along rather than hand-picking
# four of six every time upstream touches its REUSE metadata.
%license LICENSES/*
%doc README.md CHANGELOG.md NEWFEATURES.md

%{_bindir}/latte-dock
%{_datadir}/applications/org.kde.latte-dock.desktop
# plasma_install_package() also emits per-package appdata for the shell and the
# plasmoid on some Plasma versions (openSUSE Leap 16.0 does, Fedora and Mageia do
# not), so claim the family rather than the one file everyone has.
%{_datadir}/metainfo/org.kde.latte*.appdata.xml
%{_datadir}/dbus-1/interfaces/org.kde.LatteDock.xml
%{_datadir}/knotifications6/lattedock.notifyrc
%{_datadir}/knsrcfiles/latte-indicators.knsrc
%{_datadir}/knsrcfiles/latte-layouts.knsrc

# The icon themes own every directory here, so claim the leaves only.
%{_datadir}/icons/hicolor/*/apps/latte-dock.svg
%{_datadir}/icons/breeze/applets/256/org.kde.latte.plasmoid.svg

# kf6-kpackage owns the packagestructure directory. The containmentactions one
# is owned by nobody, but plasma-workspace-libs fills it with six plugins of its
# own -- a pre-existing Fedora wart, not one to adopt by claiming the directory.
%{_libdir}/qt6/plugins/kpackage/packagestructure/latte_indicator.so
%{_libdir}/qt6/plugins/plasma/containmentactions/org.kde.latte.contextmenu.so

# Directory trees only this package creates, so own them whole. That also covers
# two things in the shell package a glob would miss: three template files with
# spaces in their names, and .multiple-layouts_hidden.layout.latte.
%{_libdir}/qt6/qml/org/kde/latte/
%{_datadir}/latte/
%{_datadir}/plasma/plasmoids/org.kde.latte.containment/
%{_datadir}/plasma/plasmoids/org.kde.latte.plasmoid/
%{_datadir}/plasma/shells/org.kde.latte.shell/

%changelog
* Fri Aug 28 2026 packager - 0.10.77-1
- Qt 6 / KDE Frameworks 6 / Plasma 6 port of Latte Dock, for Wayland sessions


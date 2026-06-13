# Latte live widget add/remove e2e

`run.sh` launches the real `latte-dock` headlessly (nested `kwin_wayland`, seeded throwaway
HOME, current-source-staged QML), adds a widget (default: analog clock) via the
`org.kde.LatteDock` DBus interface, then removes it through the **real** remove action
(`triggerAppletAction … "remove"`) — the same `QAction` the edit-handle fires.

It asserts the widget is gone three ways: the `[Applets][<id>]` group leaves the
`*.layout.latte` config, the DBus `appletIds` list no longer contains it, and the dock's
rendered pixels change (captured via KWin ScreenShot2). On failure it prints which assertion
failed and the dock log tail — the first failing assertion points at the broken chain link.

Run inside the fedora distrobox: `tests/e2e/run.sh`. Override the widget with `WIDGET=…`.
Prereqs: a built `latte-dock` and `latte-imgdiff`, and `python3-dbus` in the box.

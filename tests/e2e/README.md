# Latte live widget add/remove e2e

`run.sh` launches the real `latte-dock` headlessly (nested `kwin_wayland`, seeded throwaway
HOME, current-source-staged QML), adds a widget (default: analog clock) via the
`org.kde.LatteDock` DBus interface, then removes it through the **real** remove action
(`triggerAppletAction … "remove"`) — the same `QAction` the edit-handle fires.

Removal is asserted by two conclusive witnesses: the DBus `appletIds` list no longer
containing the applet, and the dock's rendered pixels changing (captured via KWin ScreenShot2).
The `[Applets][<id>]` config group is checked and logged informational only — Latte flushes
applet changes lazily and not to the seeded legacy layout file in-session, so its absence is
not a reliable removal witness. On failure it prints which assertion failed and the dock log
tail — the first failing assertion points at the broken chain link.

Run inside the fedora distrobox: `tests/e2e/run.sh`. Override the widget with `WIDGET=…`.
Prereqs: a built `latte-dock` and `latte-imgdiff`, and `python3-dbus` in the box.

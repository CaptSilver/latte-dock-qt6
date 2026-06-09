# Live-only QML units

Units/files that cannot be covered headlessly (and why). These are out of scope for the
headless QML gate and are the target of the live-dock capture (P3). Add entries during
curation/expansion instead of gaming a headless test.

- `plasmoid/package/contents/ui/ContextMenu.qml` — SIGSEGVs in C++ during QML finalize on construct.
- `shell/package/contents/views/WidgetExplorer.qml` — SIGSEGVs in C++ during construction.
- `plasmoid/package/contents/ui/main.qml` — `Plasmoid` attached object (PlasmoidAttached)
  can't be created without a live Applet/Containment; object can't be retained.
- `plasmoid/package/contents/ui/task/TaskItem.qml` (partial) — handlers reading `root`,
  `tasksModel`, `scrollableList`, and the `ListView` delegate `index` need a live
  containment/ListView; the pure helpers are headless-coverable.

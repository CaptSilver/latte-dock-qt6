// BadgeText is installed into the shared components module and loaded by hosts
// that carry their own translation domain -- the containment and plasmoid
// packages. A bare i18nc() looks its message up in whatever domain the host set,
// not the one the string is extracted into, so the lookup has to name it.
//
// This asserts the domain rather than the returned string: the message text is
// the translators' business, and asserting it would just pin the msgid.

import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "BadgeTextDomain"
    when: windowShown

    readonly property url targetUrl: Stage.qmlModule("org/kde/latte/components/BadgeText.qml")

    property string lastDomain: "UNSET"

    // Creation-context shims: the component resolves i18n* unqualified and there is
    // no localized context under qmltestrunner.
    function i18nc(ctx, msg) { return msg; }
    function i18ndc(domain, ctx, msg) { root.lastDomain = domain; return msg; }

    function test_badge_names_the_application_domain() {
        var comp = Qt.createComponent(root.targetUrl);
        compare(comp.status, Component.Ready, comp.errorString());

        var badge = comp.createObject(root, {showNumber: true, numberValue: 10000});
        verify(badge !== null, "BadgeText did not instantiate");

        compare(root.lastDomain, "latte-dock");
        badge.destroy();
    }
}

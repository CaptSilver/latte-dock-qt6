// Drives the previews PlasmaCoreThumbnail onWinIdChanged workaround: when the
// bound winId changes, the handler bounces `visible` off and back on so the
// previews model re-instantiates. Assert the bounce through a visibleChanged
// SignalSpy (two edges) plus the final visible state.
//
// The only unqualified creation-context name is `thumbnailSourceItem`, whose
// winId feeds the root binding; mock it with a writable winId.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PlasmaCoreThumbnail"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/previews/PlasmaCoreThumbnail.qml")

    QtObject {
        id: thumbnailSourceItem
        property int winId: 0
    }

    Component {
        id: spyComp
        SignalSpy {}
    }

    function test_winIdChangeBouncesVisible() {
        thumbnailSourceItem.winId = 0;
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        verify(obj.visible);

        const spy = createTemporaryObject(spyComp, root, { target: obj, signalName: "visibleChanged" });
        verify(spy.valid, "visibleChanged spy invalid");

        thumbnailSourceItem.winId = 12345;
        compare(obj.winId, 12345);   // the binding forwarded the new id
        compare(spy.count, 2);       // handler: visible=false then visible=true
        verify(obj.visible);         // net state restored
    }
}

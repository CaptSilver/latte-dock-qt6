import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "CovSelf"
    when: windowShown

    readonly property url covUrl: Qt.resolvedUrl("_covself/CovSelf.qml")

    function test_callsOnlyCovered() {
        const c = Qt.createComponent(covUrl);
        verify(c.status === Component.Ready, "CovSelf.qml failed to compile: " + c.errorString());
        const obj = createTemporaryObject(c, testCase);
        verify(obj, "CovSelf.qml failed to instantiate");
        compare(obj.covSelfCovered(), 1);
        // covSelfUncovered() is deliberately never called.
    }
}

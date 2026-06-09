import QtQuick

// Coverage harness self-test target. covSelfCovered() is called by tst_covself;
// covSelfUncovered() is not, so CovSelf.qml must report LOC-weighted coverage
// strictly between 0 and 1.
Item {
    function covSelfCovered() {
        var total = 0;
        total += 1;
        return total;
    }

    function covSelfUncovered() {
        var total = 0;
        total -= 1;
        return total;
    }
}

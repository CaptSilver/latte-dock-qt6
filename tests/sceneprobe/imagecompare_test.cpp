#include <QtTest>
#include "imagecompare.h"
using namespace LatteProbe;

class ImageCompareTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void invariants_blankFails();
    void invariants_uniformFails();
    void invariants_contentPasses();
    // compareImages
    void compare_identicalExact();
    void compare_withinTolerance();
    void compare_exceedsBudget();
    void compare_sizeMismatch();
    // checkExpectations
    void expect_pixelOk();
    void expect_pixelWrong();
    void expect_regionMean();
    // amplifiedDiff + verdictLine
    void diff_imageHighlightsDelta();
    void diff_clampsAndForcesAlpha();
    void diff_sizeMismatchIsRed();
    void verdict_formatsMismatch();
    void verdict_formatsMatch();
    // compareImages — bounds + worst-pixel
    void compare_reportsWorstAndBounds();
    // checkExpectations — edge cases
    void expect_minOpaqueFractionShape();
    void expect_pixelOutOfBounds();
    void expect_unrecognizedEntry();
};

void ImageCompareTest::invariants_blankFails() {
    QImage img(16, 16, QImage::Format_RGBA8888);
    img.fill(QColor(0, 0, 0, 0)); // fully transparent
    QVERIFY(!checkInvariants(img, 0.01).isEmpty());
}
void ImageCompareTest::invariants_uniformFails() {
    QImage img(16, 16, QImage::Format_RGBA8888);
    img.fill(QColor(10, 20, 30, 255)); // opaque but one flat colour
    QVERIFY(!checkInvariants(img, 0.01).isEmpty());
}
void ImageCompareTest::invariants_contentPasses() {
    QImage img(16, 16, QImage::Format_RGBA8888);
    img.fill(QColor(10, 20, 30, 255));
    img.setPixelColor(8, 8, QColor(200, 100, 50, 255)); // some variation
    QVERIFY(checkInvariants(img, 0.01).isEmpty());
}

// --- compareImages ---

void ImageCompareTest::compare_identicalExact() {
    QImage a(8, 8, QImage::Format_RGBA8888); a.fill(QColor(40, 80, 120, 255));
    QImage b = a.copy();
    auto r = compareImages(a, b, {0, 0.0});
    QVERIFY(r.match);
    QCOMPARE(r.diffPixels, 0);
}
void ImageCompareTest::compare_withinTolerance() {
    QImage a(8, 8, QImage::Format_RGBA8888); a.fill(QColor(40, 80, 120, 255));
    QImage b(8, 8, QImage::Format_RGBA8888); b.fill(QColor(42, 80, 120, 255)); // Δ=2
    QVERIFY(compareImages(a, b, {2, 0.005}).match);
    QVERIFY(!compareImages(a, b, {0, 0.0}).match);
}
void ImageCompareTest::compare_exceedsBudget() {
    QImage a(10, 10, QImage::Format_RGBA8888); a.fill(QColor(0, 0, 0, 255));
    QImage b = a.copy();
    for (int i = 0; i < 60; ++i) b.setPixelColor(i % 10, i / 10, QColor(255, 255, 255, 255));
    auto r = compareImages(a, b, {2, 0.005});
    QVERIFY(!r.match);
    QCOMPARE(r.diffPixels, 60);
    QCOMPARE(r.maxDelta, 255);
}
void ImageCompareTest::compare_sizeMismatch() {
    QImage a(8, 8, QImage::Format_RGBA8888); a.fill(Qt::black);
    QImage b(4, 4, QImage::Format_RGBA8888); b.fill(Qt::black);
    QVERIFY(!compareImages(a, b, {2, 0.005}).match);
}

// --- checkExpectations ---

void ImageCompareTest::expect_pixelOk() {
    QImage img(16, 16, QImage::Format_RGBA8888); img.fill(QColor(0,0,0,255));
    img.setPixelColor(4, 4, QColor(60, 130, 233, 255));
    QVariantList exps{ QVariantMap{{QStringLiteral("x"),4},{QStringLiteral("y"),4},
                                   {QStringLiteral("rgba"),QStringLiteral("#3c82e9")},
                                   {QStringLiteral("tol"),0.1}} };
    QVERIFY(checkExpectations(img, exps).isEmpty());
}
void ImageCompareTest::expect_pixelWrong() {
    QImage img(16, 16, QImage::Format_RGBA8888); img.fill(QColor(0,0,0,255));
    QVariantList exps{ QVariantMap{{QStringLiteral("x"),4},{QStringLiteral("y"),4},
                                   {QStringLiteral("rgba"),QStringLiteral("#ffffff")},
                                   {QStringLiteral("tol"),0.05}} };
    QVERIFY(!checkExpectations(img, exps).isEmpty());
}
void ImageCompareTest::expect_regionMean() {
    QImage img(16, 16, QImage::Format_RGBA8888); img.fill(QColor(100,100,100,255));
    QVariantList region{ QVariant(0), QVariant(0), QVariant(16), QVariant(16) };
    QVariantList exps{ QVariantMap{{QStringLiteral("region"), region},
                                   {QStringLiteral("meanRgba"),QStringLiteral("#646464")},
                                   {QStringLiteral("tol"),0.05}} };
    QVERIFY(checkExpectations(img, exps).isEmpty());
}

// --- amplifiedDiff + verdictLine ---

void ImageCompareTest::diff_imageHighlightsDelta() {
    QImage a(4, 4, QImage::Format_RGBA8888); a.fill(QColor(0,0,0,255));
    QImage b = a.copy(); b.setPixelColor(1, 1, QColor(10,0,0,255));
    QImage d = amplifiedDiff(a, b, 8);
    QCOMPARE(d.size(), a.size());
    QVERIFY(d.pixelColor(1,1).red() > a.pixelColor(1,1).red());
}
void ImageCompareTest::verdict_formatsMismatch() {
    CompareResult r; r.match = false; r.diffPixels = 5; r.diffFraction = 0.05;
    r.maxDelta = 30; r.maxX = 2; r.maxY = 3;
    r.expectedAtMax = QColor(0,0,0,255); r.actualAtMax = QColor(30,0,0,255);
    const QString line = verdictLine(QStringLiteral("foo"), QStringLiteral("lavapipe"), r);
    QVERIFY(line.contains(QStringLiteral("foo")));
    QVERIFY(line.contains(QStringLiteral("(2,3)")));
}

// --- compare_reportsWorstAndBounds ---

// diffBounds inclusive corners + worst-pixel location/colours from a REAL compareImages call.
// actual = the rendered frame (diverges); expected = the reference (black).
void ImageCompareTest::compare_reportsWorstAndBounds() {
    QImage expected(10, 10, QImage::Format_RGBA8888); expected.fill(QColor(0,0,0,255));
    QImage actual = expected.copy();
    actual.setPixelColor(2, 3, QColor(10, 0, 0, 255));   // small diff (bbox top-left)
    actual.setPixelColor(7, 8, QColor(255, 0, 0, 255));  // largest diff (bbox bottom-right)
    auto r = compareImages(actual, expected, {0, 0.0});
    QVERIFY(!r.match);
    QCOMPARE(r.diffPixels, 2);
    QCOMPARE(r.maxDelta, 255);
    QCOMPARE(r.maxX, 7);
    QCOMPARE(r.maxY, 8);
    QCOMPARE(r.expectedAtMax, QColor(0, 0, 0, 255));    // reference pixel
    QCOMPARE(r.actualAtMax,   QColor(255, 0, 0, 255));  // rendered pixel
    QCOMPARE(r.diffBounds, QRect(QPoint(2, 3), QPoint(7, 8)));
}

// --- amplifiedDiff edge cases ---

void ImageCompareTest::diff_clampsAndForcesAlpha() {
    QImage a(2, 2, QImage::Format_RGBA8888); a.fill(QColor(0, 0, 0, 0));
    QImage b(2, 2, QImage::Format_RGBA8888); b.fill(QColor(100, 0, 0, 0)); // Δred=100, ×8=800
    QImage d = amplifiedDiff(a, b, 8);
    QCOMPARE(d.pixelColor(0, 0).red(), 255);    // clamped to 255
    QCOMPARE(d.pixelColor(0, 0).alpha(), 255);  // alpha forced opaque
}
void ImageCompareTest::diff_sizeMismatchIsRed() {
    QImage a(4, 4, QImage::Format_RGBA8888); a.fill(Qt::black);
    QImage b(2, 2, QImage::Format_RGBA8888); b.fill(Qt::black);
    QImage d = amplifiedDiff(a, b);
    QCOMPARE(d.size(), QSize(4, 4));
    QCOMPARE(d.pixelColor(0, 0), QColor(255, 0, 0, 255)); // red fallback
}

// --- verdictLine match branch ---

void ImageCompareTest::verdict_formatsMatch() {
    CompareResult r; r.match = true;
    const QString line = verdictLine(QStringLiteral("foo"), QStringLiteral("lavapipe"), r);
    QVERIFY(line.contains(QStringLiteral("pixel match")));
}

// --- checkExpectations edge cases ---

void ImageCompareTest::expect_minOpaqueFractionShape() {
    QImage img(8, 8, QImage::Format_RGBA8888); img.fill(QColor(0,0,0,0)); // transparent
    QVariantList exps{ QVariantMap{{QStringLiteral("minOpaqueFraction"), 0.5}} };
    QVERIFY(!checkExpectations(img, exps).isEmpty());
}
void ImageCompareTest::expect_pixelOutOfBounds() {
    QImage img(8, 8, QImage::Format_RGBA8888); img.fill(QColor(0,0,0,255));
    QVariantList exps{ QVariantMap{{QStringLiteral("x"),99},{QStringLiteral("y"),0},{QStringLiteral("rgba"),QStringLiteral("#000000")}} };
    QVERIFY(checkExpectations(img, exps).contains(QStringLiteral("out of bounds")));
}
void ImageCompareTest::expect_unrecognizedEntry() {
    QImage img(8, 8, QImage::Format_RGBA8888); img.fill(QColor(0,0,0,255));
    QVariantList exps{ QVariantMap{{QStringLiteral("bogus"), 1}} };
    QVERIFY(checkExpectations(img, exps).contains(QStringLiteral("unrecognized")));
}

QTEST_GUILESS_MAIN(ImageCompareTest)
#include "imagecompare_test.moc"

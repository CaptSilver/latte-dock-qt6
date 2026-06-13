#include <QtTest>
#include "imagecompare.h"
using namespace LatteProbe;

class ImageCompareTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void invariants_blankFails();
    void invariants_uniformFails();
    void invariants_contentPasses();
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

QTEST_GUILESS_MAIN(ImageCompareTest)
#include "imagecompare_test.moc"

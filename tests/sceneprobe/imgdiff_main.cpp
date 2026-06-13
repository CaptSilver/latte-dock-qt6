// latte-imgdiff <actual.png> <expected.png> [--delta N] [--budget F] [--out diff.png]
// Exit 0 on match, 1 on mismatch, 2 on usage/load error. Prints the verdict line to stderr.
#include <QGuiApplication>
#include <QImage>
#include <cstdio>
#include "imagecompare.h"
using namespace LatteProbe;

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    const QStringList a = app.arguments();
    if (a.size() < 3) {
        std::fprintf(stderr, "usage: latte-imgdiff actual.png expected.png [--delta N] [--budget F] [--out diff.png]\n");
        return 2;
    }
    CompareTolerance tol{2, 0.005};
    QString outPath;
    for (int i = 3; i < a.size(); ++i) {
        if (a[i] == QLatin1String("--delta") && i + 1 < a.size()) tol.perChannelDelta = a[++i].toInt();
        else if (a[i] == QLatin1String("--budget") && i + 1 < a.size()) tol.maxExceedFraction = a[++i].toDouble();
        else if (a[i] == QLatin1String("--out") && i + 1 < a.size()) outPath = a[++i];
    }
    QImage actual(a[1]), expected(a[2]);
    if (actual.isNull() || expected.isNull()) {
        std::fprintf(stderr, "latte-imgdiff: failed to load input(s)\n");
        return 2;
    }
    const CompareResult r = compareImages(actual, expected, tol);
    std::fprintf(stderr, "%s\n", qPrintable(verdictLine(a[1], QStringLiteral("imgdiff"), r)));
    if (!outPath.isEmpty()) amplifiedDiff(actual, expected).save(outPath);
    return r.match ? 0 : 1;
}

/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../../app/wm/waylandlayershell.h"

#include <QGuiApplication>
#include <QRasterWindow>
#include <QPainter>

namespace LS = Latte::WindowSystem::LayerShell;

class Bar : public QRasterWindow
{
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(QRect(QPoint(0, 0), size()), QColor(40, 120, 220));
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    Bar bar;
    bar.resize(0, 48); // width comes from the Justify anchors; height = thickness
    LS::configureView(&bar, app.primaryScreen(), Plasma::Types::BottomEdge, Latte::Types::Justify);
    LS::setExclusiveZone(&bar, 48);
    bar.show();

    qInfo() << "PASS-IF: a 48px blue bar is flush to the bottom edge, full width,"
            << "and maximised windows stop 48px above the bottom (exclusive zone).";
    return app.exec();
}

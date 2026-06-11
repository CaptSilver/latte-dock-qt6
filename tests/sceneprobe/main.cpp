// latte-sceneprobe: render a QML scene offscreen on Vulkan for N frames.
// Runs under a nested kwin_wayland session (wayland QPA) so QVulkanInstance works.
#include <QGuiApplication>
#include <QVulkanInstance>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickRenderTarget>
#include <rhi/qrhi.h>
#include <cstdio>

int main(int argc, char **argv)
{
    qputenv("QSG_RHI_BACKEND", "vulkan");
    QGuiApplication app(argc, argv);

    if (app.arguments().size() < 2) { std::fprintf(stderr, "usage: latte-sceneprobe scene.qml\n"); return 2; }
    const QString scenePath = app.arguments().at(1);
    const int frames = 5;
    const QSize size(256, 256);

    QVulkanInstance inst;
    inst.setLayers({ QByteArrayLiteral("VK_LAYER_KHRONOS_validation") });
    inst.setExtensions({ QByteArrayLiteral("VK_EXT_debug_utils") });
    if (!inst.create()) { std::fprintf(stderr, "FATAL: QVulkanInstance::create failed (err %d)\n", inst.errorCode()); return 2; }

    QQuickRenderControl renderControl;
    QQuickWindow window(&renderControl);
    window.setVulkanInstance(&inst);
    window.setColor(Qt::black);

    if (!renderControl.initialize()) { std::fprintf(stderr, "FATAL: renderControl.initialize failed\n"); return 2; }
    QRhi *rhi = renderControl.rhi();
    if (!rhi) { std::fprintf(stderr, "FATAL: no QRhi (backend not vulkan?)\n"); return 2; }

    QScopedPointer<QRhiTexture> tex(rhi->newTexture(QRhiTexture::RGBA8, size, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    tex->create();
    QScopedPointer<QRhiRenderBuffer> ds(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    ds->create();
    QRhiTextureRenderTargetDescription rtDesc(QRhiColorAttachment(tex.data()));
    rtDesc.setDepthStencilBuffer(ds.data());
    QScopedPointer<QRhiTextureRenderTarget> rt(rhi->newTextureRenderTarget(rtDesc));
    QScopedPointer<QRhiRenderPassDescriptor> rp(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.data());
    rt->create();
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.data()));

    QQmlEngine engine;
    const QByteArray extra = qgetenv("LATTE_QML_IMPORT_PATH");
    if (!extra.isEmpty()) engine.addImportPath(QString::fromLocal8Bit(extra));
    QQmlComponent component(&engine, QUrl::fromLocalFile(scenePath));
    QObject *root = component.create();
    if (!root) {
        for (const QQmlError &e : component.errors()) std::fprintf(stderr, "QML: %s\n", qPrintable(e.toString()));
        return 2;
    }
    QQuickItem *rootItem = qobject_cast<QQuickItem *>(root);
    if (!rootItem) { std::fprintf(stderr, "FATAL: scene root is not a QQuickItem\n"); return 2; }
    rootItem->setParentItem(window.contentItem());
    window.contentItem()->setSize(size);
    window.setGeometry(0, 0, size.width(), size.height());
    rootItem->setSize(size);

    for (int i = 0; i < frames; ++i) {
        renderControl.polishItems();
        renderControl.beginFrame();
        renderControl.sync();
        renderControl.render();
        renderControl.endFrame();
        QCoreApplication::processEvents();
    }
    std::printf("rendered %d frames of %s\n", frames, qPrintable(scenePath));
    return 0;
}

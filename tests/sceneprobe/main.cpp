// latte-sceneprobe: render a QML scene offscreen on Vulkan for N frames.
// Runs under a nested kwin_wayland session (wayland QPA) so QVulkanInstance works.
#include <QGuiApplication>
#include <QScopeGuard>
#include <QVulkanInstance>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickRenderTarget>
#include <QImage>
#include <QFileInfo>
#include <rhi/qrhi.h>
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include "imagecompare.h"

static std::atomic_bool g_shaderError{false};

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (msg.contains(QLatin1String("Failed to deserialize QShader"))
        || msg.contains(QLatin1String("shader preparation failed"))
        || (ctx.category && QLatin1String(ctx.category) == QLatin1String("qt.scenegraph.general")
            && type >= QtWarningMsg)) {
        g_shaderError = true;
    }
    std::fprintf(stderr, "[qt] %s\n", qPrintable(msg)); // keep Qt output visible
}

static std::atomic_bool g_validationError{false};
static bool g_outputError = false;
static std::set<std::string> g_vkSuppress;

static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCb(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *data, void *)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        const std::string id = data->pMessageIdName ? data->pMessageIdName : "";
        for (const std::string &s : g_vkSuppress) {
            if (!s.empty() && id.find(s) != std::string::npos) {
                std::fprintf(stderr, "[vk-validation SUPPRESSED] %s\n", data->pMessage);
                return VK_FALSE;
            }
        }
        g_validationError = true;
        std::fprintf(stderr, "[vk-validation ERROR] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

// Read the rendered colour texture back into an 8-bit RGBA QImage. Runs a dedicated
// offscreen frame after the render loop; the texture persists from the last rendered frame.
static QImage readbackTexture(QRhi *rhi, QRhiTexture *tex)
{
    QRhiCommandBuffer *cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess) return {};
    QRhiResourceUpdateBatch *u = rhi->nextResourceUpdateBatch();
    QRhiReadbackResult rb;
    bool done = false;
    rb.completed = [&done] { done = true; };
    u->readBackTexture(QRhiReadbackDescription(tex), &rb);
    cb->resourceUpdate(u);
    rhi->endOffscreenFrame(); // submits and waits; completed fires
    if (!done || rb.data.isEmpty()) return {};
    QImage img(reinterpret_cast<const uchar *>(rb.data.constData()),
               rb.pixelSize.width(), rb.pixelSize.height(), QImage::Format_RGBA8888);
    return img.copy(); // deep-copy out of rb.data
}

int main(int argc, char **argv)
{
    qInstallMessageHandler(messageHandler);
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

    auto createMsgr = (PFN_vkCreateDebugUtilsMessengerEXT)
        inst.getInstanceProcAddr("vkCreateDebugUtilsMessengerEXT");
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    if (createMsgr) {
        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        ci.pfnUserCallback = vkDebugCb;
        createMsgr(inst.vkInstance(), &ci, nullptr, &messenger);
    }
    auto messengerGuard = qScopeGuard([&] {
        if (messenger != VK_NULL_HANDLE) {
            auto destroyMsgr = (PFN_vkDestroyDebugUtilsMessengerEXT)
                inst.getInstanceProcAddr("vkDestroyDebugUtilsMessengerEXT");
            if (destroyMsgr) destroyMsgr(inst.vkInstance(), messenger, nullptr);
        }
    });

    QQuickRenderControl renderControl;
    QQuickWindow window(&renderControl);
    window.setVulkanInstance(&inst);
    window.setColor(Qt::black);

    if (const QByteArray supPath = qgetenv("LATTE_VK_SUPPRESSIONS"); !supPath.isEmpty()) {
        std::ifstream f(supPath.constData());
        std::string line;
        while (std::getline(f, line)) {
            const auto a = line.find_first_not_of(" \t\r\n");
            if (a == std::string::npos || line[a] == '#') continue;
            const auto b = line.find_last_not_of(" \t\r\n");
            g_vkSuppress.insert(line.substr(a, b - a + 1));
        }
    }

    if (!renderControl.initialize()) { std::fprintf(stderr, "FATAL: renderControl.initialize failed\n"); return 2; }
    QRhi *rhi = renderControl.rhi();
    if (!rhi) { std::fprintf(stderr, "FATAL: no QRhi (backend not vulkan?)\n"); return 2; }

    const QRhiDriverInfo di = rhi->driverInfo();
    std::printf("render device: %s\n", di.deviceName.constData());

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

    QImage frame = readbackTexture(rhi, tex.data());
    if (frame.isNull()) { std::fprintf(stderr, "FATAL: texture read-back failed\n"); return 2; }

    {
        const QString inv = LatteProbe::checkInvariants(frame, 0.005);
        if (!inv.isEmpty()) {
            std::fprintf(stderr, "OUTPUT FAIL (invariants): %s\n", qPrintable(inv));
            g_outputError = true;
        }
        const QVariantList exps = root->property("probeExpect").toList();
        const QString exp = LatteProbe::checkExpectations(frame, exps);
        if (!exp.isEmpty()) {
            std::fprintf(stderr, "OUTPUT FAIL (probeExpect): %s\n", qPrintable(exp));
            g_outputError = true;
        }
    }

    {
        const QByteArray dev = qgetenv("SCENEPROBE_DEVICE");
        const QString device = dev.isEmpty() ? QStringLiteral("lavapipe") : QString::fromLocal8Bit(dev);
        QString base = scenePath; base.chop(4); // drop ".qml"
        const QString refPath = base + QStringLiteral(".expected.") + device + QStringLiteral(".png");
        const QString artDir = QString::fromLocal8Bit(qgetenv("SCENEPROBE_ARTIFACTS"));
        const QString stem = artDir.isEmpty() ? base : artDir + QStringLiteral("/") + QFileInfo(scenePath).completeBaseName();

        QImage ref(refPath);
        if (ref.isNull()) {
            const QString cand = stem + QStringLiteral(".actual.png");
            frame.save(cand);
            std::fprintf(stderr, "no reference for %s (%s) — baseline written to %s; bless to enable pixel compare\n",
                         qPrintable(QFileInfo(scenePath).fileName()), qPrintable(device), qPrintable(cand));
        } else {
            LatteProbe::CompareTolerance tol = (device == QLatin1String("lavapipe"))
                ? LatteProbe::CompareTolerance{0, 0.0}
                : LatteProbe::CompareTolerance{2, 0.005};
            const LatteProbe::CompareResult r = LatteProbe::compareImages(frame, ref, tol);
            std::fprintf(stderr, "%s\n",
                         qPrintable(LatteProbe::verdictLine(QFileInfo(scenePath).fileName(), device, r)));
            if (!r.match) {
                std::fprintf(stderr, "  diff bbox: (%d,%d %dx%d)\n",
                             r.diffBounds.x(), r.diffBounds.y(), r.diffBounds.width(), r.diffBounds.height());
                frame.save(stem + QStringLiteral(".actual.png"));
                ref.save(stem + QStringLiteral(".expected.png"));
                LatteProbe::amplifiedDiff(frame, ref).save(stem + QStringLiteral(".diff.png"));
                g_outputError = true;
            }
        }
    }

    if (g_validationError) { std::fprintf(stderr, "GATE FAIL: Vulkan validation error\n"); return 1; }
    if (g_shaderError)     { std::fprintf(stderr, "GATE FAIL: Qt shader/scene-graph error\n"); return 1; }
    if (g_outputError)     { std::fprintf(stderr, "GATE FAIL: rendered output assertion failed\n"); return 1; }
    std::printf("rendered %d frames of %s — clean\n", frames, qPrintable(scenePath));
    return 0;
}

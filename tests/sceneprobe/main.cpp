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
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdio>

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

static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCb(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *data, void *)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        g_validationError = true;
        std::fprintf(stderr, "[vk-validation ERROR] %s\n", data->pMessage);
    }
    return VK_FALSE;
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
    auto destroyMsgr = (PFN_vkDestroyDebugUtilsMessengerEXT)
        inst.getInstanceProcAddr("vkDestroyDebugUtilsMessengerEXT");
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
    // Destroy messenger before the QVulkanInstance goes out of scope; validation
    // fires VUID-vkDestroyInstance-instance-00629 if we don't.
    if (messenger != VK_NULL_HANDLE && destroyMsgr)
        destroyMsgr(inst.vkInstance(), messenger, nullptr);

    if (g_validationError) { std::fprintf(stderr, "GATE FAIL: Vulkan validation error\n"); return 1; }
    if (g_shaderError)     { std::fprintf(stderr, "GATE FAIL: Qt shader/scene-graph error\n"); return 1; }
    std::printf("rendered %d frames of %s — clean\n", frames, qPrintable(scenePath));
    return 0;
}

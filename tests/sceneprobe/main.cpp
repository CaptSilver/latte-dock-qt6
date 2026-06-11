// latte-sceneprobe: headless Vulkan render gate. Task 0 = instance smoke test only.
//
// QVulkanInstance requires the QPA to advertise Vulkan support, which the offscreen
// platform does not do.  We load Vulkan directly and hand the VkInstance to Qt only
// for the function-pointer table (QVulkanFunctions), keeping everything else headless.
#include <QGuiApplication>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstring>

static bool hasLayer(const char *name)
{
    uint32_t n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    QVector<VkLayerProperties> props(n);
    vkEnumerateInstanceLayerProperties(&n, props.data());
    for (const auto &p : props)
        if (std::strcmp(p.layerName, name) == 0)
            return true;
    return false;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    const char *layerName = "VK_LAYER_KHRONOS_validation";
    const bool validationAvailable = hasLayer(layerName);
    std::printf("VK_LAYER_KHRONOS_validation available: %s\n",
                validationAvailable ? "yes" : "no (skipped)");

    const char *layers[] = {layerName};
    VkApplicationInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &ai;
    if (validationAvailable) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = layers;
    }

    VkInstance vkInst = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ci, nullptr, &vkInst);
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "FATAL: vkCreateInstance failed (VkResult %d)\n", r);
        return 2;
    }
    std::printf("validation layer requested; instance up\n");

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(vkInst, &count, nullptr);
    QVector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(vkInst, &count, devs.data());
    for (VkPhysicalDevice d : devs) {
        VkPhysicalDeviceProperties p = {};
        vkGetPhysicalDeviceProperties(d, &p);
        std::printf("device: %s (type %d)\n", p.deviceName, static_cast<int>(p.deviceType));
    }
    std::fflush(stdout);
    vkDestroyInstance(vkInst, nullptr);
    return count > 0 ? 0 : 3;
}

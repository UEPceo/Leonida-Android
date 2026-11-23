#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "LeonidaBringup", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "LeonidaBringup", __VA_ARGS__)

static bool has_ext(const std::vector<VkExtensionProperties>& exts, const char* name) {
    for (const auto& e : exts) {
        if (std::string(e.extensionName) == name) return true;
    }
    return false;
}

extern "C" JNIEXPORT jstring JNICALL
Java_company_pescai_leonida_launcher_NativeBridge_runNativeV0Probe(JNIEnv* env, jobject) {
    std::ostringstream out;
    long page_size = sysconf(_SC_PAGESIZE);

    uint32_t inst_ver = VK_API_VERSION_1_0;
    auto eiv = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
    if (eiv) eiv(&inst_ver);

    uint32_t app_ver = inst_ver < VK_API_VERSION_1_3 ? inst_ver : VK_API_VERSION_1_3;
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "LeonidaLauncherV0";
    ai.apiVersion = app_ver;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS) {
        return env->NewStringUTF("{\"error\": \"vkCreateInstance failed\"}");
    }

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    std::vector<VkPhysicalDevice> pds(nd);
    if (nd > 0) vkEnumeratePhysicalDevices(inst, &nd, pds.data());

    out << "{\n";
    out << "  \"schema\": \"leonida-mobile/apk-v0/1\",\n";
    out << "  \"page_size_bytes\": " << page_size << ",\n";
    out << "  \"instance_version\": \"" << VK_API_VERSION_MAJOR(inst_ver) << "."
        << VK_API_VERSION_MINOR(inst_ver) << "." << VK_API_VERSION_PATCH(inst_ver) << "\",\n";
    out << "  \"devices\": [\n";

    for (uint32_t i = 0; i < nd; ++i) {
        VkPhysicalDevice pd = pds[i];
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        uint32_t next = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &next, nullptr);
        std::vector<VkExtensionProperties> exts(next);
        if (next > 0) vkEnumerateDeviceExtensionProperties(pd, nullptr, &next, exts.data());

        VkPhysicalDeviceSubgroupProperties sg{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        VkPhysicalDeviceVulkan12Properties v12p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
        VkPhysicalDeviceVulkan13Properties v13p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
        VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        p2.pNext = &sg;
        if (props.apiVersion >= VK_API_VERSION_1_2) { sg.pNext = &v12p; }
        if (props.apiVersion >= VK_API_VERSION_1_3) { v12p.pNext = &v13p; }
        vkGetPhysicalDeviceProperties2(pd, &p2);

        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceMeshShaderFeaturesEXT fmesh{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
        VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fbary{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
        VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        void** tail = &f2.pNext;
        if (props.apiVersion >= VK_API_VERSION_1_2) { *tail = &f12; tail = &f12.pNext; }
        if (props.apiVersion >= VK_API_VERSION_1_3) { *tail = &f13; tail = &f13.pNext; }
        if (has_ext(exts, "VK_EXT_mesh_shader")) { *tail = &fmesh; tail = &fmesh.pNext; }
        if (has_ext(exts, "VK_KHR_fragment_shader_barycentric")) { *tail = &fbary; tail = &fbary.pNext; }
        vkGetPhysicalDeviceFeatures2(pd, &f2);

        bool wave64_native = (props.apiVersion >= VK_API_VERSION_1_3 && v13p.maxSubgroupSize >= 64) || (sg.subgroupSize == 64);
        bool mesh_hw = fmesh.meshShader == VK_TRUE;
        bool bary_hw = fbary.fragmentShaderBarycentric == VK_TRUE;
        bool cond_ren = has_ext(exts, "VK_EXT_conditional_rendering");
        bool cust_border = has_ext(exts, "VK_EXT_custom_border_color");
        bool pass_min = (f13.dynamicRendering == VK_TRUE) &&
                        (f13.synchronization2 == VK_TRUE) &&
                        (f12.bufferDeviceAddress == VK_TRUE) &&
                        (f12.runtimeDescriptorArray == VK_TRUE) &&
                        (f12.descriptorBindingPartiallyBound == VK_TRUE) &&
                        (f13.subgroupSizeControl == VK_TRUE) &&
                        cond_ren && cust_border;

        if (i > 0) out << ",\n";
        out << "    {\n";
        out << "      \"name\": \"" << props.deviceName << "\",\n";
        out << "      \"api_version\": \"" << VK_API_VERSION_MAJOR(props.apiVersion) << "."
            << VK_API_VERSION_MINOR(props.apiVersion) << "." << VK_API_VERSION_PATCH(props.apiVersion) << "\",\n";
        out << "      \"driver_name\": \"" << (props.apiVersion >= VK_API_VERSION_1_2 ? v12p.driverName : "unknown") << "\",\n";
        out << "      \"driver_info\": \"" << (props.apiVersion >= VK_API_VERSION_1_2 ? v12p.driverInfo : "unknown") << "\",\n";
        out << "      \"subgroup_size\": " << sg.subgroupSize << ",\n";
        out << "      \"wave64_native\": " << (wave64_native ? "true" : "false") << ",\n";
        out << "      \"mesh_shader_hw\": " << (mesh_hw ? "true" : "false") << ",\n";
        out << "      \"barycentric_hw\": " << (bary_hw ? "true" : "false") << ",\n";
        out << "      \"dynamic_rendering\": " << (f13.dynamicRendering ? "true" : "false") << ",\n";
        out << "      \"synchronization2\": " << (f13.synchronization2 ? "true" : "false") << ",\n";
        out << "      \"buffer_device_address\": " << (f12.bufferDeviceAddress ? "true" : "false") << ",\n";
        out << "      \"conditional_rendering\": " << (cond_ren ? "true" : "false") << ",\n";
        out << "      \"custom_border_color\": " << (cust_border ? "true" : "false") << ",\n";
        out << "      \"passes_leonida_minimum\": " << (pass_min ? "true" : "false") << ",\n";
        out << "      \"recommended_flags\": {\n";
        out << "        \"wave64_emulate\": " << (!wave64_native ? "true" : "false") << ",\n";
        out << "        \"mesh_compute\": " << (!mesh_hw ? "true" : "false") << ",\n";
        out << "        \"bary_fallback\": " << (!bary_hw ? "true" : "false") << ",\n";
        out << "        \"texel_offset_fallback\": true\n";
        out << "      }\n";
        out << "    }";
    }
    out << "\n  ]\n}\n";
    vkDestroyInstance(inst, nullptr);
    return env->NewStringUTF(out.str().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_company_pescai_leonida_launcher_NativeBridge_presentSynthFrameOnSurface(JNIEnv* env, jobject, jobject surfaceObj) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surfaceObj);
    if (!window) {
        return env->NewStringUTF("FAIL: ANativeWindow_fromSurface null");
    }

    const char* iext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "LeonidaApkSynthFrame";
    ai.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = iext;

    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS) {
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: vkCreateInstance");
    }

    VkAndroidSurfaceCreateInfoKHR asci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    asci.window = window;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateAndroidSurfaceKHR(inst, &asci, nullptr, &surface) != VK_SUCCESS) {
        vkDestroyInstance(inst, nullptr);
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: vkCreateAndroidSurfaceKHR");
    }

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    if (nd == 0) {
        vkDestroySurfaceKHR(inst, surface, nullptr);
        vkDestroyInstance(inst, nullptr);
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: no physical devices");
    }
    std::vector<VkPhysicalDevice> pds(nd);
    vkEnumeratePhysicalDevices(inst, &nd, pds.data());
    VkPhysicalDevice phys = pds[0];

    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, nullptr);
    std::vector<VkQueueFamilyProperties> qps(nq);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qps.data());
    int qf = -1;
    for (uint32_t i = 0; i < nq; ++i) {
        VkBool32 sup = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &sup);
        if ((qps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && sup == VK_TRUE) {
            qf = (int)i;
            break;
        }
    }
    if (qf < 0) {
        vkDestroySurfaceKHR(inst, surface, nullptr);
        vkDestroyInstance(inst, nullptr);
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: no present queue");
    }

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = (uint32_t)qf;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    const char* dext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dext;

    VkDevice dev = VK_NULL_HANDLE;
    if (vkCreateDevice(phys, &dci, nullptr, &dev) != VK_SUCCESS) {
        vkDestroySurfaceKHR(inst, surface, nullptr);
        vkDestroyInstance(inst, nullptr);
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: vkCreateDevice");
    }
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(dev, (uint32_t)qf, 0, &queue);

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    uint32_t nfmt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &nfmt, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(nfmt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &nfmt, fmts.data());
    VkSurfaceFormatKHR fmt = nfmt > 0 ? fmts[0] : VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR scci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    scci.surface = surface;
    scci.minImageCount = imgCount;
    scci.imageFormat = fmt.format;
    scci.imageColorSpace = fmt.colorSpace;
    scci.imageExtent = caps.currentExtent;
    scci.imageArrayLayers = 1;
    scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    scci.preTransform = caps.currentTransform;
    scci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    scci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    scci.clipped = VK_TRUE;

    VkSwapchainKHR swap = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(dev, &scci, nullptr, &swap) != VK_SUCCESS) {
        vkDestroyDevice(dev, nullptr);
        vkDestroySurfaceKHR(inst, surface, nullptr);
        vkDestroyInstance(inst, nullptr);
        ANativeWindow_release(window);
        return env->NewStringUTF("FAIL: vkCreateSwapchainKHR");
    }

    uint32_t nimg = 0;
    vkGetSwapchainImagesKHR(dev, swap, &nimg, nullptr);
    std::vector<VkImage> imgs(nimg);
    vkGetSwapchainImagesKHR(dev, swap, &nimg, imgs.data());

    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = (uint32_t)qf;
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(dev, &pci, nullptr, &pool);

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(dev, &cbai, &cb);

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semImg = VK_NULL_HANDLE, semRen = VK_NULL_HANDLE;
    vkCreateSemaphore(dev, &sci, nullptr, &semImg);
    vkCreateSemaphore(dev, &sci, nullptr, &semRen);

    uint32_t idx = 0;
    vkAcquireNextImageKHR(dev, swap, UINT64_MAX, semImg, VK_NULL_HANDLE, &idx);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cb, &bi);

    VkImageMemoryBarrier b1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.image = imgs[idx];
    b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b1);

    VkClearColorValue blue{{0.08f, 0.35f, 0.85f, 1.0f}};
    VkImageSubresourceRange rng{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cb, imgs[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &blue, 1, &rng);

    VkImageMemoryBarrier b2 = b1;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b2.dstAccessMask = 0;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &b2);

    vkEndCommandBuffer(cb);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &semImg;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &semRen;
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &semRen;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swap;
    pi.pImageIndices = &idx;
    VkResult pres = vkQueuePresentKHR(queue, &pi);
    vkQueueWaitIdle(queue);

    vkDestroySemaphore(dev, semImg, nullptr);
    vkDestroySemaphore(dev, semRen, nullptr);
    vkDestroyCommandPool(dev, pool, nullptr);
    vkDestroySwapchainKHR(dev, swap, nullptr);
    vkDestroyDevice(dev, nullptr);
    vkDestroySurfaceKHR(inst, surface, nullptr);
    vkDestroyInstance(inst, nullptr);
    ANativeWindow_release(window);

    if (pres == VK_SUCCESS) {
        LOGI("synth_frame APK: first_present OK");
        return env->NewStringUTF("synth_frame APK: first_present OK (ANativeWindow + Vulkan Clear Azul)");
    }
    return env->NewStringUTF("FAIL: vkQueuePresentKHR error");
}

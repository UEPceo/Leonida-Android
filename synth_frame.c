/* synth_frame.c - primer entregable móvil (fotograma sintético pulido).
 *
 * Ventana Win32 + superficie Vulkan + swapchain + clear azul continuo.
 * Log explícito a stdout para record_run.py.
 *
 * Compilar (mingw-w64):
 *   x86_64-w64-mingw32-gcc -O2 -o synth_frame.exe probe/synth_frame.c \
 *     -lvulkan-1 -luser32 -lgdi32
 *
 * Ejecutar bajo Wine: wine synth_frame.exe
 *
 * Referencia: ProjectLeonida/Leonida @ e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1
 */

#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH  800
#define HEIGHT 600

static VkInstance       inst;
static VkPhysicalDevice phys;
static VkDevice         dev;
static VkQueue          queue;
static uint32_t         qf;
static VkSurfaceKHR     surface;
static VkSwapchainKHR   swap;
static VkImage         *images;
static uint32_t         nimages;
static VkCommandPool    pool;
static VkCommandBuffer  cb;
static VkSemaphore      sem_image, sem_render;
static VkFence          fence;
static int              running = 1;
static int              frames  = 0;

static void log_msg(const char *s) {
    printf("synth_frame: %s\n", s);
    fflush(stdout);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY || m == WM_CLOSE) {
        running = 0;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int find_present_queue(VkPhysicalDevice pd, VkSurfaceKHR s) {
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, NULL);
    if (!n) return -1;
    VkQueueFamilyProperties *qp = calloc(n, sizeof *qp);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, qp);
    for (uint32_t i = 0; i < n; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, s, &present);
        if ((qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
            free(qp);
            return (int)i;
        }
    }
    free(qp);
    return -1;
}

static VkSurfaceFormatKHR choose_surface_format(VkPhysicalDevice pd, VkSurfaceKHR s) {
    uint32_t n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, s, &n, NULL);
    VkSurfaceFormatKHR fallback = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    if (!n) return fallback;
    VkSurfaceFormatKHR *fmts = calloc(n, sizeof *fmts);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, s, &n, fmts);
    if (n == 1 && fmts[0].format == VK_FORMAT_UNDEFINED) {
        free(fmts);
        return fallback;
    }
    for (uint32_t i = 0; i < n; i++) {
        if ((fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
            fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            VkSurfaceFormatKHR chosen = fmts[i];
            free(fmts);
            return chosen;
        }
    }
    VkSurfaceFormatKHR first = fmts[0];
    free(fmts);
    return first;
}

static int init_vulkan(HWND hwnd) {
    const char *iext[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "synth_frame",
        .apiVersion = VK_API_VERSION_1_1
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = iext
    };
    if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS) {
        log_msg("FAIL create_instance");
        return 0;
    }
    log_msg("instance OK");

    VkWin32SurfaceCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandleA(NULL),
        .hwnd = hwnd
    };
    if (vkCreateWin32SurfaceKHR(inst, &sci, NULL, &surface) != VK_SUCCESS) {
        log_msg("FAIL create_surface");
        return 0;
    }
    log_msg("surface OK");

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, NULL);
    if (!nd) {
        log_msg("FAIL no_devices");
        return 0;
    }
    VkPhysicalDevice *pds = calloc(nd, sizeof *pds);
    vkEnumeratePhysicalDevices(inst, &nd, pds);
    phys = pds[0];
    free(pds);
    log_msg("physical_device OK");

    int q = find_present_queue(phys, surface);
    if (q < 0) {
        log_msg("FAIL no_present_queue");
        return 0;
    }
    qf = (uint32_t)q;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf,
        .queueCount = 1,
        .pQueuePriorities = &prio
    };
    const char *dext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &qci,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = dext
    };
    if (vkCreateDevice(phys, &dci, NULL, &dev) != VK_SUCCESS) {
        log_msg("FAIL create_device");
        return 0;
    }
    vkGetDeviceQueue(dev, qf, 0, &queue);
    log_msg("device OK");

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
    VkSurfaceFormatKHR fmt = choose_surface_format(phys, surface);

    VkExtent2D extent = { WIDTH, HEIGHT };
    if (caps.currentExtent.width != UINT32_MAX)
        extent = caps.currentExtent;

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
        img_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR scci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = img_count,
        .imageFormat = fmt.format,
        .imageColorSpace = fmt.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE
    };
    if (vkCreateSwapchainKHR(dev, &scci, NULL, &swap) != VK_SUCCESS) {
        log_msg("FAIL create_swapchain");
        return 0;
    }
    log_msg("swapchain OK");

    vkGetSwapchainImagesKHR(dev, swap, &nimages, NULL);
    images = calloc(nimages, sizeof *images);
    vkGetSwapchainImagesKHR(dev, swap, &nimages, images);

    VkCommandPoolCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = qf,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    vkCreateCommandPool(dev, &pci, NULL, &pool);

    VkCommandBufferAllocateInfo cb_ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(dev, &cb_ai, &cb);

    VkSemaphoreCreateInfo sci2 = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(dev, &sci2, NULL, &sem_image);
    vkCreateSemaphore(dev, &sci2, NULL, &sem_render);

    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    vkCreateFence(dev, &fci, NULL, &fence);
    log_msg("sync objects OK");
    return 1;
}

static void draw_frame(void) {
    vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(dev, 1, &fence);

    uint32_t idx;
    if (vkAcquireNextImageKHR(dev, swap, UINT64_MAX, sem_image, VK_NULL_HANDLE, &idx) != VK_SUCCESS) {
        log_msg("FAIL acquire");
        return;
    }

    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &bi);

    VkImageMemoryBarrier b1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = images[idx],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 1, &b1);

    VkClearColorValue color = {{ 0.1f, 0.3f, 0.8f, 1.0f }};
    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(cb, images[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    VkImageMemoryBarrier b2 = b1;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b2.dstAccessMask = 0;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, NULL, 0, NULL, 1, &b2);

    vkEndCommandBuffer(cb);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem_image,
        .pWaitDstStageMask = &wait,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &sem_render
    };
    vkQueueSubmit(queue, 1, &si, fence);

    VkPresentInfoKHR pi = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem_render,
        .swapchainCount = 1,
        .pSwapchains = &swap,
        .pImageIndices = &idx
    };
    if (vkQueuePresentKHR(queue, &pi) != VK_SUCCESS) {
        log_msg("FAIL present");
        running = 0;
        return;
    }

    frames++;
    if (frames == 1) log_msg("first_present OK");
    if (frames % 120 == 0)
        printf("synth_frame: frames=%d\n", frames);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
    (void)hp; (void)cmd; (void)show;
    log_msg("start (ProjectLeonida/Leonida @ e56d97950ff8c47f85b96b77b5eb65c0bb1d6fe1)");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = "SynthFrame";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("SynthFrame",
        "Leonida Mobile - fotograma sintetico",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
        NULL, NULL, hi, NULL);
    if (!hwnd) {
        log_msg("FAIL create_window");
        return 1;
    }
    log_msg("window OK");

    if (!init_vulkan(hwnd)) {
        MessageBoxA(hwnd, "Fallo al inicializar Vulkan", "synth_frame", MB_OK | MB_ICONERROR);
        return 1;
    }
    log_msg("init complete, entering present loop");

    MSG msg;
    while (running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        draw_frame();
    }

    printf("synth_frame: exit frames=%d\n", frames);
    vkDeviceWaitIdle(dev);

    vkDestroySemaphore(dev, sem_image, NULL);
    vkDestroySemaphore(dev, sem_render, NULL);
    vkDestroyFence(dev, fence, NULL);
    vkDestroyCommandPool(dev, pool, NULL);
    free(images);
    vkDestroySwapchainKHR(dev, swap, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroySurfaceKHR(inst, surface, NULL);
    vkDestroyInstance(inst, NULL);
    return 0;
}

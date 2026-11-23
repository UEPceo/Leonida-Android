/* vkprobe.c - sonda Vulkan para Leonida Mobile (F1 / V0).
 *
 * Imprime un JSON en stdout: instancia, extensiones, features y límites que
 * importan para los shaders traducidos (wave64, mesh, baricéntricas, bindless,
 * BC/ASTC), y PRUEBA crear el dispositivo y enviar un command buffer vacío
 * (enumerar extensiones no basta). Cada intento queda registrado por separado.
 *
 * No prueba la presentación: eso necesita una superficie (ANativeWindow o
 * Wine) y va en el primer entregable móvil.
 *
 * Requiere cabeceras Vulkan >= 1.3.250. Código de salida: 0 si al menos un
 * dispositivo completó el envío de cola; 1 en otro caso.
 */
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- escritor JSON mínimo ---------- */
static int L = 0;
static int comma[32];

static void jstr(const char *s) {
    putchar('"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}
static void sep(const char *key) {
    if (comma[L]) putchar(',');
    comma[L] = 1;
    putchar('\n');
    for (int i = 0; i <= L; i++) fputs("  ", stdout);
    if (key) { jstr(key); fputs(": ", stdout); }
}
static void open_c(const char *key, char c) { sep(key); putchar(c); L++; comma[L] = 0; }
static void close_c(char c) {
    L--;
    putchar('\n');
    for (int i = 0; i <= L; i++) fputs("  ", stdout);
    putchar(c);
}
#define OBJ(k) open_c((k), '{')
#define ARR(k) open_c((k), '[')
#define END_OBJ() close_c('}')
#define END_ARR() close_c(']')
static void kv_s(const char *k, const char *v) { sep(k); jstr(v ? v : ""); }
static void kv_u(const char *k, unsigned long long v) { sep(k); printf("%llu", v); }
static void kv_i(const char *k, long long v) { sep(k); printf("%lld", v); }
static void kv_b(const char *k, int v) { sep(k); fputs(v ? "true" : "false", stdout); }
static void item_s(const char *v) { sep(NULL); jstr(v); }
static void kv_hex(const char *k, unsigned v) { char b[16]; snprintf(b, sizeof b, "0x%04x", v); kv_s(k, b); }
static void kv_ver(const char *k, uint32_t v) {
    char b[48];
    snprintf(b, sizeof b, "%u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
    kv_s(k, b);
}

static const char *rs(VkResult r) {
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    default: return "VkResult(otro)";
    }
}
static void kv_result(const char *k, VkResult r) {
    OBJ(k);
    kv_s("code", rs(r));
    kv_i("value", (long long)r);
    kv_b("ok", r == VK_SUCCESS);
    END_OBJ();
}

static int has_ext(const VkExtensionProperties *e, uint32_t n, const char *name) {
    for (uint32_t i = 0; i < n; i++)
        if (strcmp(e[i].extensionName, name) == 0) return 1;
    return 0;
}

/* ---------- pruebas de dispositivo ---------- */
static VkResult make_device(VkPhysicalDevice pd, uint32_t qf, const char **ex,
                            uint32_t nex, const void *pnext, VkDevice *dev) {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = qf;
    q.queueCount = 1;
    q.pQueuePriorities = &prio;
    VkDeviceCreateInfo ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.pNext = pnext;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &q;
    ci.enabledExtensionCount = nex;
    ci.ppEnabledExtensionNames = ex;
    return vkCreateDevice(pd, &ci, NULL, dev);
}

static VkResult submit_test(VkDevice dev, uint32_t qf) {
    VkQueue q;
    vkGetDeviceQueue(dev, qf, 0, &q);
    VkCommandPool pool;
    VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = qf;
    VkResult r = vkCreateCommandPool(dev, &pci, NULL, &pool);
    if (r != VK_SUCCESS) return r;
    VkCommandBuffer cb;
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    r = vkAllocateCommandBuffers(dev, &ai, &cb);
    VkFence f = VK_NULL_HANDLE;
    if (r == VK_SUCCESS) {
        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        r = vkBeginCommandBuffer(cb, &bi);
        if (r == VK_SUCCESS) r = vkEndCommandBuffer(cb);
    }
    if (r == VK_SUCCESS) {
        VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        r = vkCreateFence(dev, &fi, NULL, &f);
    }
    if (r == VK_SUCCESS) {
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        r = vkQueueSubmit(q, 1, &si, f);
    }
    if (r == VK_SUCCESS) r = vkWaitForFences(dev, 1, &f, VK_TRUE, 5000000000ull);
    if (f != VK_NULL_HANDLE) vkDestroyFence(dev, f, NULL);
    vkDestroyCommandPool(dev, pool, NULL);
    return r;
}

static const char *dev_type(VkPhysicalDeviceType t) {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
    }
}

/* Devuelve 1 si el envío de cola terminó bien. */
static int probe_device(VkPhysicalDevice pd, uint32_t app_ver) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(pd, &p);
    uint32_t dv = p.apiVersion;
    int has2 = app_ver >= VK_API_VERSION_1_1 && dv >= VK_API_VERSION_1_1;
    int v12 = has2 && app_ver >= VK_API_VERSION_1_2 && dv >= VK_API_VERSION_1_2;
    int v13 = has2 && app_ver >= VK_API_VERSION_1_3 && dv >= VK_API_VERSION_1_3;

    uint32_t next = 0;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &next, NULL);
    VkExtensionProperties *ext = calloc(next ? next : 1, sizeof *ext);
    vkEnumerateDeviceExtensionProperties(pd, NULL, &next, ext);

    /* propiedades */
    VkPhysicalDeviceSubgroupProperties sg = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceVulkan12Properties v12p = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceVulkan13Properties v13p = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
    VkPhysicalDeviceProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    if (has2) {
        void **t = &p2.pNext;
        *t = &sg; t = &sg.pNext;
        if (v12) { *t = &v12p; t = &v12p.pNext; }
        if (v13) { *t = &v13p; t = &v13p.pNext; }
        vkGetPhysicalDeviceProperties2(pd, &p2);
    }

    /* features */
    VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
#ifdef VK_EXT_mesh_shader
    VkPhysicalDeviceMeshShaderFeaturesEXT fmesh = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    int has_mesh = has_ext(ext, next, "VK_EXT_mesh_shader");
#endif
#ifdef VK_KHR_fragment_shader_barycentric
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fbary =
        {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
    int has_bary = has_ext(ext, next, "VK_KHR_fragment_shader_barycentric");
#endif
    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    if (has2) {
        void **t = &f2.pNext;
        if (v12) { *t = &f12; t = &f12.pNext; }
        if (v13) { *t = &f13; t = &f13.pNext; }
#ifdef VK_EXT_mesh_shader
        if (has_mesh) { *t = &fmesh; t = &fmesh.pNext; }
#endif
#ifdef VK_KHR_fragment_shader_barycentric
        if (has_bary) { *t = &fbary; t = &fbary.pNext; }
#endif
        vkGetPhysicalDeviceFeatures2(pd, &f2);
    } else {
        vkGetPhysicalDeviceFeatures(pd, &f2.features);
    }
    const VkPhysicalDeviceFeatures *f = &f2.features;

    /* ---- salida ---- */
    sep(NULL); putchar('{'); L++; comma[L] = 0;
    kv_s("name", p.deviceName);
    kv_s("type", dev_type(p.deviceType));
    kv_hex("vendor_id", p.vendorID);
    kv_hex("device_id", p.deviceID);
    kv_ver("api_version", dv);
    kv_u("driver_version_raw", p.driverVersion);
    if (v12) {
        kv_u("driver_id", (unsigned long long)v12p.driverID);
        kv_s("driver_name", v12p.driverName);
        kv_s("driver_info", v12p.driverInfo);
    }

    OBJ("subgroup");
    if (has2) {
        kv_u("size", sg.subgroupSize);
        kv_u("supported_stages", sg.supportedStages);
        kv_u("supported_operations", sg.supportedOperations);
    }
    if (v13) {
        kv_u("min_size", v13p.minSubgroupSize);
        kv_u("max_size", v13p.maxSubgroupSize);
        kv_u("required_size_stages", v13p.requiredSubgroupSizeStages);
    }
    /* derivado: ¿puede ejecutar wave64 sin emulación? */
    kv_b("wave64_native_possible",
         (v13 && v13p.maxSubgroupSize >= 64) || (has2 && sg.subgroupSize == 64));
    END_OBJ();

    OBJ("features");
    kv_b("geometryShader", f->geometryShader);
    kv_b("tessellationShader", f->tessellationShader);
    kv_b("shaderInt64", f->shaderInt64);
    kv_b("shaderInt16", f->shaderInt16);
    kv_b("shaderFloat64", f->shaderFloat64);
    kv_b("textureCompressionBC", f->textureCompressionBC);
    kv_b("textureCompressionASTC_LDR", f->textureCompressionASTC_LDR);
    kv_b("textureCompressionETC2", f->textureCompressionETC2);
    kv_b("fragmentStoresAndAtomics", f->fragmentStoresAndAtomics);
    kv_b("vertexPipelineStoresAndAtomics", f->vertexPipelineStoresAndAtomics);
    kv_b("multiDrawIndirect", f->multiDrawIndirect);
    kv_b("drawIndirectFirstInstance", f->drawIndirectFirstInstance);
    kv_b("shaderStorageImageExtendedFormats", f->shaderStorageImageExtendedFormats);
    kv_b("depthClamp", f->depthClamp);
    kv_b("samplerAnisotropy", f->samplerAnisotropy);
    if (v12) {
        kv_b("descriptorIndexing", f12.descriptorIndexing);
        kv_b("runtimeDescriptorArray", f12.runtimeDescriptorArray);
        kv_b("descriptorBindingPartiallyBound", f12.descriptorBindingPartiallyBound);
        kv_b("descriptorBindingVariableDescriptorCount", f12.descriptorBindingVariableDescriptorCount);
        kv_b("shaderSampledImageArrayNonUniformIndexing", f12.shaderSampledImageArrayNonUniformIndexing);
        kv_b("bufferDeviceAddress", f12.bufferDeviceAddress);
        kv_b("shaderFloat16", f12.shaderFloat16);
        kv_b("shaderInt8", f12.shaderInt8);
        kv_b("timelineSemaphore", f12.timelineSemaphore);
        kv_b("vulkanMemoryModel", f12.vulkanMemoryModel);
        kv_b("drawIndirectCount", f12.drawIndirectCount);
        kv_b("shaderSignedZeroInfNanPreserveFloat32", v12p.shaderSignedZeroInfNanPreserveFloat32);
    }
    if (v13) {
        kv_b("dynamicRendering", f13.dynamicRendering);
        kv_b("synchronization2", f13.synchronization2);
        kv_b("maintenance4", f13.maintenance4);
        kv_b("subgroupSizeControl", f13.subgroupSizeControl);
        kv_b("computeFullSubgroups", f13.computeFullSubgroups);
    }
#ifdef VK_EXT_mesh_shader
    if (has_mesh) {
        kv_b("meshShader", fmesh.meshShader);
        kv_b("taskShader", fmesh.taskShader);
    }
#endif
#ifdef VK_KHR_fragment_shader_barycentric
    if (has_bary) kv_b("fragmentShaderBarycentric", fbary.fragmentShaderBarycentric);
#endif
    END_OBJ();

    OBJ("extensions_of_interest");
    kv_b("VK_EXT_conditional_rendering", has_ext(ext, next, "VK_EXT_conditional_rendering"));
    kv_b("VK_EXT_custom_border_color", has_ext(ext, next, "VK_EXT_custom_border_color"));
    kv_b("VK_EXT_subgroup_size_control", has_ext(ext, next, "VK_EXT_subgroup_size_control"));
    kv_b("VK_KHR_dynamic_rendering", has_ext(ext, next, "VK_KHR_dynamic_rendering"));
    kv_b("VK_KHR_synchronization2", has_ext(ext, next, "VK_KHR_synchronization2"));
    kv_b("VK_KHR_buffer_device_address", has_ext(ext, next, "VK_KHR_buffer_device_address"));
    kv_b("VK_KHR_swapchain_maintenance1", has_ext(ext, next, "VK_KHR_swapchain_maintenance1"));
    kv_b("VK_KHR_surface_maintenance1", has_ext(ext, next, "VK_KHR_surface_maintenance1"));
    kv_b("VK_EXT_mesh_shader", has_ext(ext, next, "VK_EXT_mesh_shader"));
    kv_b("VK_KHR_fragment_shader_barycentric", has_ext(ext, next, "VK_KHR_fragment_shader_barycentric"));
    kv_b("VK_KHR_maintenance8", has_ext(ext, next, "VK_KHR_maintenance8"));
    kv_b("VK_KHR_swapchain", has_ext(ext, next, "VK_KHR_swapchain"));
    kv_b("VK_KHR_shader_float_controls", has_ext(ext, next, "VK_KHR_shader_float_controls"));
    kv_b("VK_KHR_ray_query", has_ext(ext, next, "VK_KHR_ray_query"));
    kv_b("VK_KHR_acceleration_structure", has_ext(ext, next, "VK_KHR_acceleration_structure"));
    END_OBJ();

    ARR("extensions");
    for (uint32_t i = 0; i < next; i++) item_s(ext[i].extensionName);
    END_ARR();

    OBJ("limits");
    kv_u("maxImageDimension2D", p.limits.maxImageDimension2D);
    kv_u("maxPushConstantsSize", p.limits.maxPushConstantsSize);
    kv_u("maxBoundDescriptorSets", p.limits.maxBoundDescriptorSets);
    kv_u("maxPerStageDescriptorSampledImages", p.limits.maxPerStageDescriptorSampledImages);
    kv_u("maxPerStageDescriptorStorageBuffers", p.limits.maxPerStageDescriptorStorageBuffers);
    kv_u("maxStorageBufferRange", p.limits.maxStorageBufferRange);
    kv_u("maxComputeSharedMemorySize", p.limits.maxComputeSharedMemorySize);
    kv_u("maxComputeWorkGroupInvocations", p.limits.maxComputeWorkGroupInvocations);
    kv_u("maxMemoryAllocationCount", p.limits.maxMemoryAllocationCount);
    END_OBJ();

    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    ARR("memory_heaps");
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++) {
        sep(NULL); putchar('{'); L++; comma[L] = 0;
        kv_u("size_bytes", mp.memoryHeaps[i].size);
        kv_b("device_local", (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0);
        END_OBJ();
    }
    END_ARR();

    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, NULL);
    VkQueueFamilyProperties *qp = calloc(nq ? nq : 1, sizeof *qp);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qp);
    ARR("queue_families");
    int qf = -1;
    for (uint32_t i = 0; i < nq; i++) {
        sep(NULL); putchar('{'); L++; comma[L] = 0;
        kv_u("flags", qp[i].queueFlags);
        kv_u("count", qp[i].queueCount);
        END_OBJ();
        if (qf < 0 && (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) qf = (int)i;
    }
    END_ARR();

    /* ---- pruebas: cada intento se registra por separado ---- */
    OBJ("device_tests");
    int submitted = 0;
    if (qf < 0) {
        kv_s("note", "sin familia de cola con gráficos+cómputo");
    } else {
        VkDevice dev = VK_NULL_HANDLE;

        /* A: dispositivo mínimo, sin extensiones ni features */
        VkResult ra = make_device(pd, (uint32_t)qf, NULL, 0, NULL, &dev);
        kv_result("create_minimal", ra);
        if (ra == VK_SUCCESS) {
            VkResult rs_ = submit_test(dev, (uint32_t)qf);
            kv_result("submit_minimal", rs_);
            submitted = (rs_ == VK_SUCCESS);
            vkDestroyDevice(dev, NULL);
        }

        /* B: todas las features soportadas + extensiones de interés presentes */
        const char *want[] = {"VK_KHR_swapchain", "VK_EXT_mesh_shader",
                              "VK_KHR_fragment_shader_barycentric", "VK_KHR_maintenance8"};
        const char *on[4];
        uint32_t non = 0;
        for (int i = 0; i < 4; i++)
            if (has_ext(ext, next, want[i])) on[non++] = want[i];
        ARR("create_full_enabled_extensions");
        for (uint32_t i = 0; i < non; i++) item_s(on[i]);
        END_ARR();
        dev = VK_NULL_HANDLE;
        VkResult rb = make_device(pd, (uint32_t)qf, on, non, has2 ? (const void *)&f2 : NULL, &dev);
        kv_result("create_full", rb);
        if (rb == VK_SUCCESS) {
            VkResult rs2 = submit_test(dev, (uint32_t)qf);
            kv_result("submit_full", rs2);
            if (rs2 == VK_SUCCESS) submitted = 1;
            vkDestroyDevice(dev, NULL);
        }
    }
    kv_s("presentation", "not_tested: requiere superficie (ANativeWindow o Wine)");
    END_OBJ();

    END_OBJ(); /* dispositivo */
    free(ext);
    free(qp);
    return submitted;
}

int main(void) {
    fputs("{", stdout);
    comma[0] = 0;
    kv_s("schema", "leonida-mobile/vkprobe/1");

    uint32_t inst_ver = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion eiv =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    if (eiv) eiv(&inst_ver);
    kv_ver("instance_version", inst_ver);

    uint32_t ni = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &ni, NULL);
    VkExtensionProperties *ie = calloc(ni ? ni : 1, sizeof *ie);
    vkEnumerateInstanceExtensionProperties(NULL, &ni, ie);
    ARR("instance_extensions");
    for (uint32_t i = 0; i < ni; i++) item_s(ie[i].extensionName);
    END_ARR();
    free(ie);

    uint32_t app_ver = inst_ver < VK_API_VERSION_1_3 ? inst_ver : VK_API_VERSION_1_3;
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "vkprobe";
    ai.apiVersion = app_ver;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    VkInstance inst;
    VkResult r = vkCreateInstance(&ici, NULL, &inst);
    kv_result("create_instance", r);
    if (r != VK_SUCCESS) {
        putchar('\n'); putchar('}'); putchar('\n');
        return 1;
    }

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, NULL);
    VkPhysicalDevice *pds = calloc(nd ? nd : 1, sizeof *pds);
    vkEnumeratePhysicalDevices(inst, &nd, pds);
    int any_ok = 0;
    ARR("devices");
    for (uint32_t i = 0; i < nd; i++)
        if (probe_device(pds[i], app_ver)) any_ok = 1;
    END_ARR();
    kv_b("any_device_submitted", any_ok);

    free(pds);
    vkDestroyInstance(inst, NULL);
    putchar('\n'); putchar('}'); putchar('\n');
    return any_ok ? 0 : 1;
}

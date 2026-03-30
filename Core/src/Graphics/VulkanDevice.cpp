#include "VulkanDevice.h"

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include <array>
#include <vector>
#include <algorithm>
#include <cstring>

// =========================================================================
// Constants
// =========================================================================

static constexpr std::array VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation",
};

// Extensions that every backend must support.
static constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

// Optional extensions — enabled when available; listed in DeviceDesc so
// NVRHI knows which features it can use at runtime.
static constexpr std::array OPTIONAL_DEVICE_EXTENSIONS = {
    VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
    VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
    VK_NV_MESH_SHADER_EXTENSION_NAME,
};

#ifdef NDEBUG
static constexpr bool ENABLE_VALIDATION = false;
#else
static constexpr bool ENABLE_VALIDATION = true;
#endif

// =========================================================================
// File-scope debug-messenger helpers
// =========================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    const char* tag =
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) ? "[perf] " :
        (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)  ? "[validation] " : "";

    if      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        spdlog::error("[Vulkan] {}{}", tag, data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        spdlog::warn("[Vulkan] {}{}", tag, data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        spdlog::debug("[Vulkan] {}{}", tag, data->pMessage);
    // VERBOSE level omitted — pure driver noise

    return VK_FALSE;
}

static void PopulateDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& info)
{
    info = {};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;
}

static VkResult CreateDebugMessenger(
    VkInstance                                instance,
    const VkDebugUtilsMessengerCreateInfoEXT* info,
    VkDebugUtilsMessengerEXT*                 out)
{
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, info, nullptr, out);
}

static void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn) fn(instance, messenger, nullptr);
}

// =========================================================================
// VulkanDevice — lifecycle
// =========================================================================

VulkanDevice::VulkanDevice(GLFWwindow* window) : m_Window(window) {}

VulkanDevice::~VulkanDevice()
{
    if (m_NvrhiDevice)
        DestroyDevice();
}

nvrhi::IDevice* VulkanDevice::CreateDevice()
{
    CreateInstance();

    if (ENABLE_VALIDATION)
        SetupDebugMessenger();

    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();

    // -----------------------------------------------------------------
    // Populate nvrhi::vulkan::DeviceDesc and create the NVRHI device.
    // We pass every Vulkan object the device needs plus the full
    // extension lists so NVRHI knows exactly what is available.
    // -----------------------------------------------------------------
    nvrhi::vulkan::DeviceDesc desc;
    desc.errorCB            = &m_MessageCallback;

    desc.instance           = m_Instance;
    desc.physicalDevice     = m_PhysicalDevice;
    desc.device             = m_Device;

    desc.graphicsQueue      = m_GraphicsQueue;
    desc.graphicsQueueIndex = static_cast<int>(m_Queues.graphics);

    desc.transferQueue      = m_TransferQueue;
    desc.transferQueueIndex = static_cast<int>(m_Queues.transfer);

    desc.computeQueue       = m_ComputeQueue;
    desc.computeQueueIndex  = static_cast<int>(m_Queues.compute);

    desc.instanceExtensions    = m_InstanceExtensions.data();
    desc.numInstanceExtensions = m_InstanceExtensions.size();
    desc.deviceExtensions      = m_DeviceExtensions.data();
    desc.numDeviceExtensions   = m_DeviceExtensions.size();

    // We enable bufferDeviceAddress in the Vulkan12 feature chain below
    desc.bufferDeviceAddressSupported = true;

    m_NvrhiDevice = nvrhi::vulkan::createDevice(desc);
    if (!m_NvrhiDevice)
    {
        spdlog::critical("[VulkanDevice] nvrhi::vulkan::createDevice() returned null");
        abort();
    }

    spdlog::info("[VulkanDevice] NVRHI Vulkan device created");
    return m_NvrhiDevice.Get();
}

void VulkanDevice::DestroyDevice()
{
    if (m_NvrhiDevice)
    {
        m_NvrhiDevice->waitForIdle();
        m_NvrhiDevice = nullptr;    // releases RefCountPtr → NVRHI internal cleanup
    }

    if (m_Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    if (ENABLE_VALIDATION && m_DebugMessenger != VK_NULL_HANDLE)
    {
        DestroyDebugMessenger(m_Instance, m_DebugMessenger);
        m_DebugMessenger = VK_NULL_HANDLE;
    }

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

void VulkanDevice::CreateSwapchain(uint32_t width, uint32_t height)
{
    // Query surface capabilities, formats, and present modes
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &format_count, formats.data());

    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &mode_count, modes.data());

	VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(formats);
	VkPresentModeKHR   present_mode = ChoosePresentMode(modes);
	VkExtent2D         extent = ChooseExtent(capabilities, width, height);
	m_SwapFormat = surface_format.format;
	m_SwapExtent = extent;
	spdlog::info("[VulkanDevice] Swapchain ready with {}x{}", extent.width, extent.height);
	spdlog::debug("[VulkanDevice]   Image format : {}", (int)surface_format.format);
	spdlog::debug("[VulkanDevice]   Color space  : {}", (int)surface_format.colorSpace);
	spdlog::debug("[VulkanDevice]   Present mode : {}",
		(present_mode == VK_PRESENT_MODE_MAILBOX_KHR) ? "mailbox (triple-buffer)" :
		(present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) ? "immediate (no vsync)" :
		"fifo (vsync)");

    // Request one more image than the minimum to avoid stalling on the driver
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0)
        image_count = std::min(image_count, capabilities.maxImageCount);

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = image_count;
	createInfo.imageFormat = surface_format.format;
	createInfo.imageColorSpace = surface_format.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// If graphics and present are on different queue families, images must
	// be shared (concurrent) rather than exclusively owned.
	uint32_t qf_indices[] = { m_Queues.graphics, m_Queues.present };
	if (m_Queues.graphics != m_Queues.present) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = qf_indices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = capabilities.currentTransform; // no extra rotation
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = present_mode;
	createInfo.clipped = VK_TRUE; // don't care about pixels behind other windows

	if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
	{
		spdlog::critical("[VulkanDevice] vkCreateSwapchainKHR failed");
		abort();
	}

	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &image_count, nullptr);
	m_SwapImages.resize(image_count);
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &image_count, m_SwapImages.data());

	CreateHandleForNativeTextures();
}

void VulkanDevice::BeginFrame()
{
	vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
		m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentFrameIdx);

    // Hand the acquire semaphore to nvrhi so it waits before executing commands
    m_NvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, m_ImageAvailableSemaphores[m_CurrentFrame], 0);
}

void VulkanDevice::Present()
{
	// Signal the render-finished semaphore when nvrhi is done with this frame's work
	m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, m_RenderFinishedSemaphores[m_CurrentFrame], 0);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &m_CurrentFrameIdx;
	VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		spdlog::info("[VulkanDevice] Swapchain out of date during Present; recreating");
		RecreateSwapchain(m_SwapExtent.width, m_SwapExtent.height);
	}
	else if (result != VK_SUCCESS)
	{
		spdlog::error("[VulkanDevice] vkQueuePresentKHR failed: {}", result);
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

}

// =========================================================================
// Init steps
// =========================================================================

void VulkanDevice::CreateInstance()
{
    if (ENABLE_VALIDATION)
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> available(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, available.data());

        for (const char* name : VALIDATION_LAYERS)
        {
            bool found = false;
            for (const auto& props : available)
                if (strcmp(name, props.layerName) == 0) { found = true; break; }

            if (!found)
            {
                spdlog::critical("[VulkanDevice] Validation layer '{}' not available. "
                                 "Install the Vulkan SDK or build in Release.", name);
                abort();
            }
        }
    }

    VkApplicationInfo appInfo  = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "CoreEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // GLFW specifies which instance extensions it needs for surface creation
    uint32_t     glfwExtCount = 0;
    const char** glfwExts     = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    m_InstanceExtensions.assign(glfwExts, glfwExts + glfwExtCount);

    if (ENABLE_VALIDATION)
        m_InstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo  = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo      = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_InstanceExtensions.data();

    // Chain a debug messenger into creation so validation errors during
    // vkCreateInstance / vkDestroyInstance itself are captured.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    if (ENABLE_VALIDATION)
    {
        PopulateDebugMessengerInfo(debugCreateInfo);
        createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        createInfo.pNext               = &debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
    {
        spdlog::critical("[VulkanDevice] vkCreateInstance failed");
        abort();
    }

    spdlog::info("[VulkanDevice] Vulkan instance created (API 1.3)");
}

void VulkanDevice::SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT info = {};
    PopulateDebugMessengerInfo(info);

    if (CreateDebugMessenger(m_Instance, &info, &m_DebugMessenger) != VK_SUCCESS)
        spdlog::warn("[VulkanDevice] Failed to create debug messenger");
}

void VulkanDevice::CreateSurface()
{
    if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
    {
        spdlog::critical("[VulkanDevice] glfwCreateWindowSurface failed");
        abort();
    }
}

void VulkanDevice::PickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
    if (count == 0)
    {
        spdlog::critical("[VulkanDevice] No Vulkan-capable GPUs found on this system");
        abort();
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice d : devices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);

        if (!IsDeviceSuitable(d))
        {
            spdlog::debug("[VulkanDevice]   Skipping (unsuitable): {}", props.deviceName);
            continue;
        }

        int score = ScoreDevice(d);
        spdlog::debug("[VulkanDevice]   Candidate: {} (score={})", props.deviceName, score);

        if (score > bestScore)
        {
            bestScore        = score;
            m_PhysicalDevice = d;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE)
    {
        spdlog::critical("[VulkanDevice] No suitable GPU found — "
                         "requires swapchain support and graphics + present queues");
        abort();
    }

    VkPhysicalDeviceProperties chosen = {};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &chosen);
    spdlog::info("[VulkanDevice] Using GPU: {}", chosen.deviceName);
    spdlog::debug("[VulkanDevice]   API    : {}.{}.{}",
        VK_VERSION_MAJOR(chosen.apiVersion),
        VK_VERSION_MINOR(chosen.apiVersion),
        VK_VERSION_PATCH(chosen.apiVersion));
    spdlog::debug("[VulkanDevice]   Driver : {:#010x}", chosen.driverVersion);
    spdlog::debug("[VulkanDevice]   Max 2D : {}px",     chosen.limits.maxImageDimension2D);
}

void VulkanDevice::CreateLogicalDevice()
{
    m_Queues = FindQueueFamilies(m_PhysicalDevice);

    // Build a deduplicated set of queue create infos. Some GPUs share a
    // single family for multiple roles; Vulkan requires one entry per family.
    std::vector<uint32_t> uniqueFamilies;
    auto addUnique = [&](uint32_t idx) {
        if (idx != UINT32_MAX &&
            std::find(uniqueFamilies.begin(), uniqueFamilies.end(), idx) == uniqueFamilies.end())
            uniqueFamilies.push_back(idx);
    };
    addUnique(m_Queues.graphics);
    addUnique(m_Queues.present);
    addUnique(m_Queues.transfer);
    addUnique(m_Queues.compute);

    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qi = {};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    // -----------------------------------------------------------------
    // Build the device extension list: required first, then any optional
    // extensions present on this physical device.
    // -----------------------------------------------------------------
    m_DeviceExtensions.assign(
        REQUIRED_DEVICE_EXTENSIONS.begin(),
        REQUIRED_DEVICE_EXTENSIONS.end());

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(extCount);
    vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extCount, availableExts.data());

    for (const char* opt : OPTIONAL_DEVICE_EXTENSIONS)
    {
        for (const auto& ext : availableExts)
        {
            if (strcmp(opt, ext.extensionName) == 0)
            {
                m_DeviceExtensions.push_back(opt);
                spdlog::debug("[VulkanDevice] Enabling optional extension: {}", opt);
                break;
            }
        }
    }

    // -----------------------------------------------------------------
    // Feature chain
    //
    // pNext: VkDeviceCreateInfo → VkPhysicalDeviceFeatures2
    //                           → VkPhysicalDeviceVulkan13Features
    //                           → VkPhysicalDeviceVulkan12Features
    //
    // Setting pEnabledFeatures = nullptr and driving everything through
    // the pNext chain is required when using VkPhysicalDeviceFeatures2.
    // -----------------------------------------------------------------

    // Vulkan 1.2 — timeline semaphores, BDA, and descriptor indexing are
    // all required by NVRHI's Vulkan backend.
    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType                                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress                     = VK_TRUE;
    features12.timelineSemaphore                       = VK_TRUE;
    features12.descriptorIndexing                      = VK_TRUE;
    features12.runtimeDescriptorArray                  = VK_TRUE;
    features12.descriptorBindingPartiallyBound         = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    // Vulkan 1.3 — dynamic rendering replaces legacy render-pass objects;
    // synchronization2 gives NVRHI a cleaner pipeline barrier API.
    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext            = &features12;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.maintenance4     = VK_TRUE;

    // Core 1.0 features
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType                      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext                      = &features13;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.fillModeNonSolid  = VK_TRUE;   // wireframe debug rendering

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext                   = &features2;  // drives entire feature chain
    createInfo.pEnabledFeatures        = nullptr;     // must be null when using Features2
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(m_DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

    // Older drivers require validation layers to also be set on the device
    if (ENABLE_VALIDATION)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
    {
        spdlog::critical("[VulkanDevice] vkCreateDevice failed");
        abort();
    }

    vkGetDeviceQueue(m_Device, m_Queues.graphics, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_Queues.present,  0, &m_PresentQueue);
    vkGetDeviceQueue(m_Device, m_Queues.transfer, 0, &m_TransferQueue);
    vkGetDeviceQueue(m_Device, m_Queues.compute,  0, &m_ComputeQueue);

    spdlog::debug("[VulkanDevice] Logical device created");
    spdlog::debug("[VulkanDevice]   Graphics : family {}", m_Queues.graphics);
    spdlog::debug("[VulkanDevice]   Present  : family {}", m_Queues.present);
    spdlog::debug("[VulkanDevice]   Transfer : family {}{}", m_Queues.transfer,
        m_Queues.transfer == m_Queues.graphics ? " (shared with graphics)" : "");
    spdlog::debug("[VulkanDevice]   Compute  : family {}{}", m_Queues.compute,
        m_Queues.compute == m_Queues.graphics ? " (shared with graphics)" : "");
}

// =========================================================================
// Physical-device helpers
// =========================================================================

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto& f = families[i];

        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
        if (presentSupport)
            indices.present = i;

        // Prefer families that are transfer-only (avoids blocking the graphics queue)
        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.transfer = i;

        // Prefer families that are compute-only (async compute)
        if ((f.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            indices.compute = i;
    }

    // Fall back to the graphics family when dedicated queues are unavailable
    if (indices.transfer == UINT32_MAX) indices.transfer = indices.graphics;
    if (indices.compute  == UINT32_MAX) indices.compute  = indices.graphics;

    return indices;
}

VkSurfaceFormatKHR VulkanDevice::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    // Prefer sRGB + B8G8R8A8 - this is the most common and gives correct
    // gamma-corrected display output without manual gamma correction.
	for (const auto& f : formats)
	{
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
			f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return f;
	}

    // Fall back on first available format
    return formats[0];
}

VkPresentModeKHR VulkanDevice::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    for (const auto& m : modes)
		if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m; // triple buffering, low latency, no tearing

    // Fall back on first available present mode
    return modes[0];
}

VkExtent2D VulkanDevice::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t width, uint32_t height)
{
    // If currentExtent is UINT32_MAX the surface lets us choose freely
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    VkExtent2D extent = { width, height };
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

void VulkanDevice::CreateSyncObjects()
{
    VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateSemaphore(m_Device, &info, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS)
		{
			spdlog::critical("[VulkanDevice] Failed to create synchronization objects");
			abort();
		}

        if (vkCreateSemaphore(m_Device, &info, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS)
        {
			spdlog::critical("[VulkanDevice] Failed to create synchronization objects");
			abort();
        }
	}
}

nvrhi::Format VulkanDevice::ToNvrhiFormat(VkFormat format) const
{
    switch (format)
    {
    case VK_FORMAT_B8G8R8A8_UNORM: return nvrhi::Format::SBGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:  return nvrhi::Format::SBGRA8_UNORM;
    case VK_FORMAT_R8G8B8_UNORM:   return nvrhi::Format::RGBA8_UNORM;
    case VK_FORMAT_R8G8B8_SRGB:    return nvrhi::Format::SRGBA8_UNORM;
    default:
        spdlog::warn("[VulkanDevice] Unrecognized swapchain format {}, defaulting to RGBA8_UNORM", static_cast<int>(format));
        return nvrhi::Format::RGBA8_UNORM;
    }
}

void VulkanDevice::CreateHandleForNativeTextures()
{
    nvrhi::TextureDesc desc;

	desc.width = m_SwapExtent.width;
	desc.height = m_SwapExtent.height;
	desc.format = ToNvrhiFormat(m_SwapFormat);
	desc.dimension = nvrhi::TextureDimension::Texture2D;
    desc.isRenderTarget = true;
	desc.initialState = nvrhi::ResourceStates::Present;
    desc.keepInitialState = true;

    m_BackBuffers.resize(m_SwapImages.size());
	for (size_t i = 0; i < m_SwapImages.size(); ++i)
	{
        nvrhi::Object nativeImage(m_SwapImages[i]);
		nativeImage.integer = reinterpret_cast<uint64_t>(m_SwapImages[i]);

		m_BackBuffers[i] = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nativeImage, desc);


	}
}

bool VulkanDevice::CheckExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* required : REQUIRED_DEVICE_EXTENSIONS)
    {
        bool found = false;
        for (const auto& ext : available)
            if (strcmp(required, ext.extensionName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device) const
{
    if (!FindQueueFamilies(device).IsComplete()) return false;
    if (!CheckExtensionSupport(device))          return false;

    // Verify the swapchain can present to our surface
    uint32_t formatCount = 0, modeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &modeCount, nullptr);
    return formatCount > 0 && modeCount > 0;
}

int VulkanDevice::ScoreDevice(VkPhysicalDevice device) const
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 10000;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;
    score += static_cast<int>(props.limits.maxImageDimension2D / 1024);
    return score;
}

// =========================================================================
// NVRHI message callback
// =========================================================================

void VulkanDevice::MessageCallback::message(nvrhi::MessageSeverity severity, const char* text)
{
    switch (severity)
    {
    case nvrhi::MessageSeverity::Fatal:
        spdlog::critical("[NVRHI] {}", text);
        abort();
        break;
    case nvrhi::MessageSeverity::Error:
        spdlog::error("[NVRHI] {}", text);
        break;
    case nvrhi::MessageSeverity::Warning:
        spdlog::warn("[NVRHI] {}", text);
        break;
    case nvrhi::MessageSeverity::Info:
        spdlog::info("[NVRHI] {}", text);
        break;
    }
}

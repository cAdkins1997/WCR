
#include "device.h"

Device::Device(std::string_view appName, const u32 _width, const u32 _height)
{
    init_window({_width, _height});
    init_instance();
    init_surface();
    select_gpu();
    init_device();
    init_swapchain();
    init_allocator();
    init_commands();
    init_sync_objects();
    init_draw_images();
    init_depth_images();
}

Device::~Device()
{
    handle.waitIdle();
    for (auto& frame : commandBufferInfos) {
        handle.destroyCommandPool(frame.commandPool, nullptr);
        handle.destroyFence(frame.renderFence, nullptr);
        handle.destroySemaphore(frame.acquiredSemaphore, nullptr);
    }

    destroy_swapchain();

    vmaDestroyImage(allocator, m_DrawImage.handle, m_DrawImage.allocation);
    vkDestroyImageView(handle, m_DrawImage.view, nullptr);

    vmaDestroyImage(allocator, m_DepthImage.handle, m_DepthImage.allocation);
    vkDestroyImageView(handle, m_DepthImage.view, nullptr);

    handle.destroyCommandPool(immediateInfo.immediateCommandPool, nullptr);
    handle.destroyFence(immediateInfo.immediateFence, nullptr);

    instance.destroySurfaceKHR(m_Surface, nullptr);

    glfwDestroyWindow(m_Window);
}

vk::Extent2D Device::get_display_extent()
{
    u32 width {}, height {};
    glfwGetFramebufferSize(m_Window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));
    m_DrawImage.extent = {width, height};
    m_DepthImage.extent = {width, height};
    return {width, height};
}

bool Device::recreate_swapchain() {

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);

    if (width <= 0 || height <= 0)
    {
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_Window, &width, &height);
            glfwWaitEvents();
        }
    }

    handle.waitIdle();

    destroy_swapchain();
    init_swapchain();
    recreate_draw_images();

    for (auto& cmd : commandBufferInfos)
        cmd.resizeRequested = false;

    return true;
}

void Device::recreate_draw_images()
{
    destroy_draw_images();
    destroy_depth_images();
    init_draw_images();
    init_depth_images();
}

void Device::init_window(const vk::Extent2D extent)
{
    if (glfwInit() != GLFW_TRUE)
        throw std::runtime_error("Failed to initialize GLFW");
    if (glfwVulkanSupported() != GLFW_TRUE)
        throw std::runtime_error("Vulkan is not supported by your system");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_Window = glfwCreateWindow(extent.width, extent.height, applicationName.data(), nullptr, nullptr);
}

void Device::init_instance()
{
    vk::ApplicationInfo appinfo;
    appinfo.pApplicationName = applicationName.data();
    appinfo.pEngineName = "Engine";
    appinfo.engineVersion = vk::ApiVersion14;
    appinfo.apiVersion = vk::ApiVersion14;

    vk::InstanceCreateInfo instanceCI;
    instanceCI.pApplicationInfo = &appinfo;

    const auto extensions = get_required_extensions();
    instanceCI.enabledExtensionCount = static_cast<u32>(extensions.size());
    instanceCI.ppEnabledExtensionNames = extensions.data();

    vk::DebugUtilsMessengerCreateInfoEXT debugCI;

    if (enableValidationLayers) {
        debugCI.messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
        debugCI.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
        debugCI.pfnUserCallback = debugMessageFunc;
    }

    vk_check(
createInstance(&instanceCI, nullptr, &instance),
"Failed to create instance"
    );
}

void Device::init_surface()
{
    glfwCreateWindowSurface(instance, m_Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&m_Surface));
}

void Device::select_gpu()
{
    u32 gpuCount = 0;
    vk_check(
        instance.enumeratePhysicalDevices(&gpuCount, nullptr),
        "Failed to enumerate physical devices"
        );

    if (gpuCount == 0) throw std::runtime_error("Failed to find suitable GPU");

    std::vector<vk::PhysicalDevice> gpus(gpuCount);

    vk_check(
        instance.enumeratePhysicalDevices(&gpuCount, gpus.data()),
        "Failed to enumerate physical devices"
        );

    const auto result = std::ranges::find_if(gpus, [&](auto candidate)
    {
        return gpu_is_suitable(candidate);
    });
    if (result != gpus.end())
    {
        m_Gpu = *result;
    }
    else
    {
        throw std::runtime_error("failed to find a suitable GPU");
    }
}

void Device::init_device()
{
    QueueFamilyIndices indices = find_queue_families(m_Gpu);

    std::vector<vk::DeviceQueueCreateInfo> queueCIs;
    std::set uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    f32 queuePriority = 1.0f;
    for (u32 queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCI;
        queueCI.queueFamilyIndex = queueFamily;
        queueCI.queueCount = 1;
        queueCI.pQueuePriorities = &queuePriority;
        queueCIs.push_back(queueCI);
    }

    vk::PhysicalDeviceFeatures2 deviceFeatures;
    deviceFeatures = m_Gpu.getFeatures2();

    vk::PhysicalDeviceVulkan11Features deviceVulkan11Features;
    deviceVulkan11Features.shaderDrawParameters = true;
    deviceFeatures.pNext = &deviceVulkan11Features;

    vk::PhysicalDeviceVulkan12Features deviceVulkan12Features;
    deviceVulkan12Features.bufferDeviceAddress = true;
    deviceVulkan12Features.runtimeDescriptorArray = true;
    deviceVulkan12Features.descriptorBindingPartiallyBound = true;
    deviceVulkan12Features.runtimeDescriptorArray = true;
    deviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingStorageImageUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = true;
    deviceVulkan12Features.descriptorBindingVariableDescriptorCount = true;
    deviceVulkan12Features.scalarBlockLayout = true;
    deviceVulkan12Features.vulkanMemoryModel = true;
    deviceVulkan12Features.vulkanMemoryModelDeviceScope = true;
    deviceVulkan11Features.pNext = deviceVulkan12Features;

    vk::PhysicalDeviceVulkan13Features deviceVulkan13Features;
    deviceVulkan13Features.synchronization2 = true;
    deviceVulkan13Features.dynamicRendering = true;
    deviceVulkan12Features.pNext = &deviceVulkan13Features;

    vk::PhysicalDeviceVulkan14Features deviceVulkan14Features;
    deviceVulkan13Features.pNext = &deviceVulkan14Features;

    vk::PhysicalDeviceRobustness2FeaturesEXT robustnessFeaturesEXT;
    robustnessFeaturesEXT.robustBufferAccess2 = true;
    deviceVulkan14Features.pNext = &robustnessFeaturesEXT;

    vk::DeviceCreateInfo deviceCI;
    deviceCI.pNext = &deviceFeatures;
    deviceCI.queueCreateInfoCount = static_cast<u32>(queueCIs.size());
    deviceCI.pQueueCreateInfos = queueCIs.data();
    deviceCI.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = deviceExtensions.data();

    handle = m_Gpu.createDevice(deviceCI, nullptr);

    graphicsQueue = handle.getQueue(indices.graphicsFamily.value(), graphicsQueueIndex);
    computeQueue = handle.getQueue(indices.computeFamily.value(), computeQueueIndex);
    presentQueue = handle.getQueue(indices.presentFamily.value(), presentQueueIndex);
    transferQueue = handle.getQueue(indices.transferFamily.value(), transferQueueIndex);
}

void Device::init_swapchain()
{
    auto [capabilities, formats, presentModes] = query_swapchain_support(m_Gpu);

    vk::SurfaceFormatKHR surfaceFormat = choose_swap_surface_format(formats);
    vk::PresentModeKHR presentMode = choose_swap_present_mode(presentModes);
    vk::Extent2D extent = choose_swap_extent(capabilities, get_display_extent());

    u32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR swapchainCI;
    swapchainCI.surface = m_Surface;
    swapchainCI.minImageCount = imageCount;
    swapchainCI.imageFormat = surfaceFormat.format;
    swapchainCI.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCI.imageExtent = extent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = find_queue_families(m_Gpu);
    u32 queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value(), indices.computeFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        swapchainCI.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchainCI.queueFamilyIndexCount = 4;
        swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
    }

    swapchainCI.preTransform = capabilities.currentTransform;
    swapchainCI.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchainCI.presentMode = presentMode;
    swapchainCI.clipped = vk::True;

    vk_check(
        handle.createSwapchainKHR(&swapchainCI, nullptr, &m_Swapchain),
        "Failed to create swapchain"
    );

    vk_check(
        handle.getSwapchainImagesKHR(m_Swapchain, &imageCount, nullptr),
        "Failed to get swapchain images"
    );

    swapchainImageData.resize(imageCount);
    std::vector<vk::Image> swapchainImages;
    swapchainImages.resize(imageCount);

    vk_check(
    handle.getSwapchainImagesKHR(m_Swapchain, &imageCount, swapchainImages.data()),
"Failed to get swapchain images"
    );

    m_SwapchainFormat = surfaceFormat.format;
    m_SwapchainExtent = extent;

    std::vector<vk::Semaphore> acquiredSemaphores;
    acquiredSemaphores.resize(imageCount);

    for (u64 i = 0; i < swapchainImageData.size(); ++i)
    {
        constexpr vk::SemaphoreCreateInfo semaphoreCI;

        vk_check(
             handle.createSemaphore(&semaphoreCI, nullptr, &acquiredSemaphores[i]),
             "Failed to create semaphore"
             );

        swapchainImageData[i].swapchainImage = swapchainImages[i];
        swapchainImageData[i].renderEndSemaphore = acquiredSemaphores[i];
    }

    std::vector<vk::ImageView> imageViews;
    imageViews.resize(imageCount);

    for (u64 i = 0; i < swapchainImageData.size(); ++i)
    {
        vk::ImageViewCreateInfo imageViewCI;
        imageViewCI.image = swapchainImages[i];
        imageViewCI.viewType = vk::ImageViewType::e2D;
        imageViewCI.format = m_SwapchainFormat;
        imageViewCI.components = {
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        };

        imageViewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewCI.subresourceRange.baseMipLevel = 0;
        imageViewCI.subresourceRange.levelCount = 1;
        imageViewCI.subresourceRange.baseArrayLayer = 0;
        imageViewCI.subresourceRange.layerCount = 1;

        vk_check(
        handle.createImageView(&imageViewCI, nullptr, &imageViews[i]),
        "Failed to create image view"
        );

        swapchainImageData[i].swapchainImageView = imageViews[i];
    }
}

void Device::init_commands()
{
    vk::CommandPoolCreateInfo commandPoolCI;
    commandPoolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    commandPoolCI.queueFamilyIndex = graphicsQueueIndex;

    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk_check(
            handle.createCommandPool(&commandPoolCI, nullptr, &commandBufferInfos[i].commandPool),
            "Failed to create command pool"
        );

        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = commandBufferInfos[i].commandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;

        vk::CommandBuffer newCmd;
        vk_check(
            handle.allocateCommandBuffers(&allocInfo, &newCmd),
            "Failed to allocate command buffers"
        );

        commandBufferInfos[i].commandBuffer.set_handle(newCmd);
    }

    vk_check(
        handle.createCommandPool(&commandPoolCI, nullptr, &immediateInfo.immediateCommandPool),
        "Failed to create immediate command pool"
    );

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = immediateInfo.immediateCommandPool;
    allocInfo.commandBufferCount = 1;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;

    vk_check(
        handle.allocateCommandBuffers(&allocInfo, &immediateInfo.immediateCommandBuffer),
        "Failed to allocate immediate command buffers"
    );
}

void Device::init_sync_objects()
{
    constexpr vk::FenceCreateInfo fenceCI(vk::FenceCreateFlagBits::eSignaled);
    constexpr vk::SemaphoreCreateInfo semaphoreCI;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk_check(
            handle.createFence(&fenceCI, nullptr, &commandBufferInfos[i].renderFence),
            "Failed to create fence"
        );

        vk_check(
            handle.createSemaphore(&semaphoreCI, nullptr, &commandBufferInfos[i].acquiredSemaphore),
            "Failed to create semaphore"
            );
    }

    vk_check(
        handle.createFence(&fenceCI, nullptr, &immediateInfo.immediateFence),
        "Failed to create immediate fence"
    );
}

void Device::init_allocator()
{
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    allocatorInfo.physicalDevice = m_Gpu;
    allocatorInfo.device = handle;
    allocatorInfo.instance = instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorInfo, &allocator);

    for (auto& cmdInfo : commandBufferInfos)
        cmdInfo.commandBuffer.set_allocator(allocator);
}

void Device::init_draw_images()
{
    const VkExtent3D drawImageExtent = to_extent_3D(get_display_extent());
    m_DrawImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_DrawImage.extent = drawImageExtent;

    VkImageUsageFlags drawImageUsages =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VK_IMAGE_USAGE_STORAGE_BIT |
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = nullptr};
    imageCI.format = m_DrawImage.format;
    imageCI.usage = drawImageUsages;
    imageCI.extent = drawImageExtent;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;

    VmaAllocationCreateInfo imageAI{};
    imageAI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAI.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(allocator, &imageCI, &imageAI, &m_DrawImage.handle, &m_DrawImage.allocation, nullptr);

    VkImageSubresourceRange subresourceRange;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo imageViewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr};
    imageViewCI.image = m_DrawImage.handle;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = m_DrawImage.format;
    imageViewCI.subresourceRange = subresourceRange;

    vkCreateImageView(handle, &imageViewCI, nullptr, &m_DrawImage.view);

    drawAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    drawAttachment.imageView = m_DrawImage.view;
    drawAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    drawAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    drawAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
}

void Device::init_depth_images()
{
    const auto depthImageExtent = to_extent_3D(get_display_extent());
    m_DepthImage.format = VK_FORMAT_D32_SFLOAT;
    m_DepthImage.extent = depthImageExtent;

    constexpr VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = nullptr};
    imageCI.format = m_DepthImage.format;
    imageCI.usage = depthImageUsages;
    imageCI.extent = depthImageExtent;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocationCI{};
    allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateImage(allocator, &imageCI, &allocationCI, &m_DepthImage.handle, &m_DepthImage.allocation, nullptr);

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkImageViewCreateInfo imageViewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr};
    imageViewCI.image = m_DepthImage.handle;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = VK_FORMAT_D32_SFLOAT;
    imageViewCI.subresourceRange = subresourceRange;

    vkCreateImageView(handle, &imageViewCI, nullptr, &m_DepthImage.view);

    depthAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    depthAttachment.imageView = m_DepthImage.view;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
}

void Device::init_imgui() const {
        const vk::DescriptorPoolSize poolSizes[] = {
            { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
        };

        vk::DescriptorPoolCreateInfo poolCI;
        poolCI.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolCI.maxSets = 1000;
        poolCI.poolSizeCount = static_cast<u32>(std::size(poolSizes));
        poolCI.pPoolSizes = poolSizes;

        vk::DescriptorPool imguiPool;
        vk_check(
            handle.createDescriptorPool(&poolCI, nullptr, &imguiPool),
            "Failed to create descriptor pool"
        );

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(m_Window, true);
        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = m_Gpu;
        initInfo.Device = handle;
        initInfo.Queue = graphicsQueue;
        initInfo.DescriptorPool = imguiPool;
        initInfo.MinImageCount = 3;
        initInfo.ImageCount = 3;
        initInfo.UseDynamicRendering = true;

        initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        constexpr VkFormat colorAttachFormat = VK_FORMAT_B8G8R8A8_SRGB;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachFormat;
        initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_Init(&initInfo);
}

std::vector<const char*> Device::get_required_extensions()
{
    u32 glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);

    return extensions;
}

bool Device::gpu_is_suitable(const vk::PhysicalDevice gpu)
{
    const QueueFamilyIndices indices = find_queue_families(gpu);

    const bool extensionsSupported = check_device_extension_support(gpu);

    bool swapchainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapchainSupport = query_swapchain_support(gpu);
        swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    }

    return indices.is_complete() && extensionsSupported && swapchainAdequate;
}

QueueFamilyIndices Device::find_queue_families(const vk::PhysicalDevice gpu) const
{
    QueueFamilyIndices indices;
    u32 count = 0;

    gpu.getQueueFamilyProperties(&count, nullptr);
    std::vector<vk::QueueFamilyProperties> families(count);
    gpu.getQueueFamilyProperties(&count, families.data());

    i32 idx = 0;
    for (const auto& family : families) {
        if (family.queueFlags & vk::QueueFlagBits::eGraphics) indices.graphicsFamily = idx;
        if (family.queueFlags & vk::QueueFlagBits::eCompute) indices.computeFamily = idx;
        if (family.queueFlags & vk::QueueFlagBits::eTransfer) indices.transferFamily = idx;

        vk::Bool32 presentSupport = false;
        vk_check(
            gpu.getSurfaceSupportKHR(idx, m_Surface, &presentSupport),
            "Failed to get GPU present support"
            );

        if (presentSupport) indices.presentFamily = idx;
        if (indices.is_complete()) break;
        idx++;
    }

    return indices;
}

bool Device::check_device_extension_support(vk::PhysicalDevice gpu)
{
    u32 extensionCount;
    vk_check(
        gpu.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr),
        "Failed to enumerate device extension properties"
        );

    std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
    vk_check(
        gpu.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
        "Failed to enumer device extension properties"
        );

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    std::println("Required extensions");
    for (const auto& extension : requiredExtensions)
        std::println("{}", extension);

    for (const auto& extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    std::println("Required extensions:");
    for (const auto& extension : requiredExtensions)
        std::println("{}", extension);

    return requiredExtensions.empty();
}

SwapChainSupportDetails Device::query_swapchain_support(const vk::PhysicalDevice gpu)
{
    SwapChainSupportDetails details;

    vk_check(
        gpu.getSurfaceCapabilitiesKHR(m_Surface, &details.capabilities),
        "Failed to get surface capabillities"
        );

    u32 formatCount;
    vk_check(
    gpu.getSurfaceFormatsKHR(m_Surface, &formatCount, nullptr),
    "Failed to get GPU surface formats"
    );

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vk_check(
        gpu.getSurfaceFormatsKHR(m_Surface, &formatCount, details.formats.data()),
"Failed to get GPU surface formats"
        );
    }

    u32 presentModeCount;
    vk_check(
    gpu.getSurfacePresentModesKHR(m_Surface, &presentModeCount, nullptr),
    "Failed to get surface present modes"
    );

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vk_check(
            gpu.getSurfacePresentModesKHR(m_Surface, &presentModeCount, details.presentModes.data()),
"Failed to get surface present modes"
        );
    }

    return details;
}

vk::SurfaceFormatKHR Device::choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    const auto it = std::ranges::find_if(availableFormats, [](auto format)
    {
        return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return *it;
}

vk::PresentModeKHR Device::choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    if (const auto it = std::ranges::find(availablePresentModes, vk::PresentModeKHR::eMailbox); it != availablePresentModes.end())
        return *it;

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Device::choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, const vk::Extent2D extent) const {
    const auto u32Limit = std::numeric_limits<u32>::max();
    if (capabilities.currentExtent.width < u32Limit && capabilities.currentExtent.height < u32Limit)
        return capabilities.currentExtent;

    const auto width = std::clamp(
        extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
        );
    const auto height = std::clamp(
        extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
        );
    return { width, height };
}

void Device::destroy_swapchain() {
    handle.destroySwapchainKHR(m_Swapchain, nullptr);

    for (const auto&[swapchainImage, swapchainImageView, renderEndSemaphore] : swapchainImageData) {
        handle.destroyImageView(swapchainImageView, nullptr);
        handle.destroySemaphore(renderEndSemaphore, nullptr);
    }
}

void Device::destroy_draw_images() const
{
    vmaDestroyImage(allocator, m_DrawImage.handle, m_DrawImage.allocation);
    vkDestroyImageView(handle, m_DrawImage.view, nullptr);
}

void Device::destroy_depth_images() const
{
    vmaDestroyImage(allocator, m_DepthImage.handle, m_DepthImage.allocation);
    vkDestroyImageView(handle, m_DepthImage.view, nullptr);
}

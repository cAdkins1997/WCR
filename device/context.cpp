
#include "context.h"

#include "simdjson.h"

Context::Context(std::string_view appName, u32 width, u32 height) : m_device(std::make_unique<Device>(appName, width, height))
{
    auto& infos = get_command_buffer_infos();
    for (auto& info : infos) {
        info.SceneData = create_buffer(
        sizeof(SceneData),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
        info.commandBuffer.set_allocator(m_device->get_allocator());
    }
}

Context::~Context()
{

}

void Context::frame_submit(const std::function<void(FrameInFlight& cmd, SwapchainImageData& swapchainData)>&& func)
{
    auto deviceHandle = m_device->get_handle();

    auto fif = m_device->commandBufferInfos[frameNumber % MAX_FRAMES_IN_FLIGHT];

    vk_check(
        deviceHandle.waitForFences(1, &fif.renderFence, true, UINT64_MAX),
        "Failed to wait for fences!"
    );

    auto swapchain = m_device->get_swapchain();

    vk_check(
        deviceHandle.acquireNextImageKHR(swapchain, UINT32_MAX, fif.acquiredSemaphore, nullptr, &swapchainImageIndex),
        "Failed to acquire next swapchain image!"
    );

    auto swapchainData = m_device->swapchainImageData[swapchainImageIndex];

    vk_check(
        deviceHandle.resetFences(1, &fif.renderFence),
        "Failed to reset render fences!"
    );

    func(fif, swapchainData);

    const vk::PresentInfoKHR presentInfo(1, &swapchainData.renderEndSemaphore, 1, &swapchain, &swapchainImageIndex);
    if (const auto result = get_graphic_queue().presentKHR(presentInfo); result == vk::Result::eErrorOutOfDateKHR)
    {
        fif.resizeRequested = true;
        return;
    }

    frameNumber++;
}

Buffer Context::create_buffer(
    const u64 allocationSize,
    vk::BufferUsageFlags usage,
    const VmaMemoryUsage memoryUsage,
    const VmaAllocationCreateFlags flags)
{
    usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;

    vk::BufferCreateInfo bufferInfo;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocationSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo{};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = flags;

    Buffer newBuffer{};

    vmaCreateBuffer(
        m_device->get_allocator(),
        reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo),
        &vmaallocInfo,
        &newBuffer.handle,
        &newBuffer.allocation,
        &newBuffer.info
        );

    vk::BufferDeviceAddressInfo addressInfo{};
    addressInfo.buffer = newBuffer.handle;
    newBuffer.deviceAddress = m_device->get_handle().getBufferAddress(addressInfo);

    return newBuffer;
}

Buffer Context::create_staging_buffer(const u64 size) const {
    VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateFlags allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationCreateInfo allocationCI{};
    allocationCI.flags = allocationFlags;
    allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
    Buffer newBuffer{};

    vmaCreateBuffer(
        m_device->get_allocator(),
        &bufferInfo,
        &allocationCI,
        &newBuffer.handle,
        &newBuffer.allocation,
        &newBuffer.info);
    return newBuffer;
}

Image Context::create_image(
    vk::Extent3D size,
    const VkFormat format,
    const VkImageUsageFlags usage,
    const u32 mipLevels,
    const bool mipmapped) const
{
    Image newImage{};
    newImage.format = format;
    newImage.extent = size;

    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCI.format = format;
    imageCI.usage = usage;
    imageCI.extent = size;
    imageCI.arrayLayers = 1;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.mipLevels = mipLevels;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;

    if (mipmapped)
        imageCI.mipLevels = mipLevels;
    else
        imageCI.mipLevels = 1;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateImage(m_device->get_allocator(), &imageCI, &allocInfo, (VkImage*)&newImage.handle, &newImage.allocation, nullptr);

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkImageViewCreateInfo viewInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = newImage.handle;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlag;
    viewInfo.subresourceRange.levelCount = imageCI.mipLevels;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_device->get_handle(), &viewInfo, nullptr, &newImage.view);

    return newImage;
}

Sampler Context::create_sampler(const vk::Filter minFilter, const vk::Filter magFilter, const vk::SamplerMipmapMode mipmapMode) const
{
    vk::SamplerCreateInfo samplerCI;
    samplerCI.minFilter = minFilter;
    samplerCI.magFilter = magFilter;
    samplerCI.mipmapMode = mipmapMode;
    vk::Sampler newSampler;

    vk_check(
        m_device->get_handle().createSampler(&samplerCI, nullptr, &newSampler),
        "failed to create sampler"
        );

    return Sampler{magFilter, minFilter, newSampler};
}

Shader Context::create_shader(std::string_view filePath) const
{
    std::ifstream file(filePath.data(), std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to find file!\n");

    const u64 fileSize = file.tellg();

    std::vector<u32> buffer(fileSize / sizeof(u32));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    vk::ShaderModuleCreateInfo shaderCI;
    shaderCI.codeSize = buffer.size() * sizeof(uint32_t);
    shaderCI.pCode = buffer.data();

    vk::ShaderModule shaderModule;
    vk_check(
        m_device->get_handle().createShaderModule(&shaderCI, nullptr, &shaderModule),
        "Failed to create shader module"
        );

    Shader shader;
    shader.module = shaderModule;
    shader.path = filePath.data();

    return shader;
}

void Context::destroy_shader(const Shader &shader) const {
    m_device->get_handle().destroy(shader.module);
}

void Context::submit_work(const CommandBuffer &cmd,
    const vk::PipelineStageFlagBits2 wait,
    const vk::PipelineStageFlagBits2 signal,
    const vk::Semaphore acquiredSemaphore,
    const vk::Semaphore renderEndSemaphore,
    const vk::Fence renderFence) const
{
    const vk::CommandBuffer handle = cmd.get_handle();
    const vk::CommandBufferSubmitInfo commandBufferSI(handle);

    vk::SemaphoreSubmitInfo waitInfo(acquiredSemaphore);
    waitInfo.stageMask = wait;
    vk::SemaphoreSubmitInfo signalInfo(renderEndSemaphore);
    signalInfo.stageMask = signal;

    constexpr vk::SubmitFlagBits submitFlags{};
    const vk::SubmitInfo2 submitInfo(
        submitFlags,
        1, &waitInfo,
        1, &commandBufferSI,
        1, &signalInfo);

    const auto graphicsQueue = m_device->get_graphics_queue();
    vk_check(
        graphicsQueue.submit2(1, &submitInfo, renderFence),
        "Failed to submit graphics commands"
        );
}

void Context::submit_upload_work() {
    const auto deviceHandle = m_device->get_handle();
    const auto [immFence, immPool, immCmd] = m_device->get_immediate_info();
    CommandBuffer cmd;
    cmd.set_handle(immCmd);
    cmd.set_allocator(m_device->get_allocator());

    vk_check(deviceHandle.waitForFences(1, &immFence, true, UINT64_MAX), "Failed to wait for fences");
    vk_check(deviceHandle.resetFences(1, &immFence), "Failed to reset fences");

    vk::CommandBufferSubmitInfo commandBufferSI(cmd.get_handle());
    const vk::SubmitInfo2 submitInfo({}, nullptr, commandBufferSI);

    const auto graphicsQueue = m_device->get_graphics_queue();
    vk_check(
        graphicsQueue.submit2(1, &submitInfo, immFence),
        "Failed to submit immediate upload commands"
        );

    vk_check(
        m_device->get_handle().waitForFences(1, &immFence, true, UINT64_MAX),
        "Failed to wait for fences"
        );
}

void Context::submit_immediate_work(std::function<void(CommandBuffer)> &&function) const {
    const auto deviceHandle = m_device->get_handle();
    const auto [immFence, immPool, immCmd] = m_device->get_immediate_info();
    CommandBuffer cmd;
    cmd.set_handle(immCmd);
    cmd.set_allocator(m_device->get_allocator());

    vk_check(deviceHandle.resetFences(1, &immFence), "Failed to reset fences");

    cmd.begin();
    function(cmd);
    cmd.end();

    const vk::CommandBufferSubmitInfo commandBufferSI(cmd.get_handle());
    vk::SubmitInfo2 submitInfo({}, nullptr, nullptr);
    submitInfo.pCommandBufferInfos = &commandBufferSI;

    const auto graphicsQueue = m_device->get_graphics_queue();
    vk_check(
        graphicsQueue.submit2(1, &submitInfo, immFence),
        "Failed to submit immediate upload commands"
        );

    vk_check(
        deviceHandle.waitForFences(1, &immFence, true, UINT64_MAX),
        "Failed to wait for fences"
        );
}

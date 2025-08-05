
#include "context.h"

Context::Context(std::string_view appName, u32 _width, u32 _height)
{
    device = std::make_unique<Device>(appName, _width, _height);
}

Context::~Context()
{

}

void Context::frame_submit(const std::function<void(const CommandBufferInfo& cmd)>&& func)
{
    auto deviceHandle = device->get_handle();

    auto fif = device->commandBufferInfos[frameNumber % MAX_FRAMES_IN_FLIGHT];
    auto swapchainData = device->swapchainImageData[frameNumber % device->swapchainImageData.size()];

    vk_check(
        deviceHandle.waitForFences(1, &fif.renderFence, true, UINT64_MAX),
        "Failed to wait for fences!"
    );

    func(fif);

    auto swapchain = device->get_swapchain();
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
        device->get_allocator(),
        reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo),
        &vmaallocInfo,
        &newBuffer.handle,
        &newBuffer.allocation,
        &newBuffer.info
        );

    vk::BufferDeviceAddressInfo addressInfo{};
    addressInfo.buffer = newBuffer.handle;
    newBuffer.deviceAddress = device->get_handle().getBufferAddress(addressInfo);

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

    vmaCreateImage(device->get_allocator(), &imageCI, &allocInfo, (VkImage*)&newImage.handle, &newImage.allocation, nullptr);

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

    vkCreateImageView(device->get_handle(), &viewInfo, nullptr, &newImage.view);

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
        device->get_handle().createSampler(&samplerCI, nullptr, &newSampler),
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
        device->get_handle().createShaderModule(&shaderCI, nullptr, &shaderModule),
        "Failed to create shader module"
        );

    Shader shader;
    shader.module = shaderModule;
    shader.path = filePath.data();

    return shader;
}

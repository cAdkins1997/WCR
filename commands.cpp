
#include "commands.h"

void CommandBuffer::begin() const
{
    cmd.reset();
    const vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk_check(cmd.begin(&beginInfo), "Failed to begin command buffer");
}

void CommandBuffer::end() const
{
    cmd.end();
}

void CommandBuffer::image_barrier(const vk::Image image, const vk::ImageLayout currentLayout, const vk::ImageLayout newLayout) const
{
    vk::ImageMemoryBarrier2 imageBarrier(
        vk::PipelineStageFlagBits2::eAllCommands,vk::AccessFlagBits2::eMemoryWrite,
        vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead
        );

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    const auto aspectMask = newLayout == vk::ImageLayout::eDepthAttachmentOptimal ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
    imageBarrier.subresourceRange = vk::ImageSubresourceRange(
        aspectMask,
        0,
        vk::RemainingMipLevels,
        0,
        vk::RemainingArrayLayers
        );
    imageBarrier.image = image;

    vk::DependencyInfo dependencyInfo;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    cmd.pipelineBarrier2(&dependencyInfo);
}

void CommandBuffer::buffer_barrier(
    const Buffer& buffer,
    vk::DeviceSize offset,
    vk::PipelineStageFlags srcStageFlags,
    vk::AccessFlags srcAccessMask,
    vk::PipelineStageFlags dstStageFlags,
    vk::AccessFlags dstAccessMask) const
{
    vk::BufferMemoryBarrier barrier(srcAccessMask, dstAccessMask);
    barrier.buffer = buffer.handle;
    barrier.offset = offset;
    barrier.size = vk::WholeSize;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

    cmd.pipelineBarrier(
        srcStageFlags,
        dstStageFlags,
        {},
        0,
        nullptr,
        1,
        &barrier,
        0,
        nullptr
        );
}

void CommandBuffer::memory_barrier(
    const vk::PipelineStageFlags2 srcStageFlags, const vk::AccessFlags2 srcAccessMask,
    const vk::PipelineStageFlags2 dstStageFlags, const vk::AccessFlags2 dstAccessMask) const
{
    vk::MemoryBarrier2 memoryBarrier;
    memoryBarrier.srcStageMask = srcStageFlags;
    memoryBarrier.srcAccessMask = srcAccessMask;
    memoryBarrier.dstStageMask = dstStageFlags;
    memoryBarrier.dstAccessMask = dstAccessMask;

    vk::DependencyInfo dependencyInfo;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    cmd.pipelineBarrier2(&dependencyInfo);
}

void CommandBuffer::copy_image(const vk::Image src, const vk::Image dst, const vk::Extent3D srcSize, const vk::Extent3D dstSize) const
{
    vk::ImageBlit2 blitRegion;

    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    vk::BlitImageInfo2 blitInfo(
        src,
        vk::ImageLayout::eTransferSrcOptimal,
        dst,
        vk::ImageLayout::eTransferDstOptimal,
        1,
        &blitRegion,
        vk::Filter::eLinear
        );

    cmd.blitImage2(&blitInfo);
}

void CommandBuffer::copy_buffer(
    const Buffer& bufferSrc, const Buffer& bufferDst,
    const vk::DeviceSize srcOffset, const vk::DeviceSize dstOffset,
    const vk::DeviceSize dataSize) const
{
    vk::BufferCopy bufferCopy;
    bufferCopy.dstOffset = dstOffset;
    bufferCopy.srcOffset = srcOffset;
    bufferCopy.size = dataSize;
    cmd.copyBuffer(bufferSrc.handle, bufferDst.handle, 1, &bufferCopy);
}

void CommandBuffer::copy_buffer_to_image(
    const Buffer& buffer,
    const Image& image,
    const vk::ImageLayout layout,
    const std::vector<vk::BufferImageCopy>& regions) const
{
    cmd.copyBufferToImage(buffer.handle, image.handle, layout, regions.size(), regions.data());
}

void CommandBuffer::upload_image(void* data, const Image& image) const
{
    const auto extent = image.extent;
    const u64 size = extent.width * extent.height * extent.depth * 4;
    const Buffer stagingBuffer = make_staging_buffer(size);

    image_barrier(image.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy copyRegion;
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    cmd.copyBufferToImage(stagingBuffer.handle, image.handle, vk::ImageLayout::eTransferDstOptimal, copyRegion);

    image_barrier(image.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

}

void CommandBuffer::upload_uniform(const void* data, const u64 dataSize, const Buffer& uniform) const
{
    VkMemoryPropertyFlags propertyFlags;
    vmaGetAllocationMemoryProperties(allocator, uniform.allocation, &propertyFlags);
    if (propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        memcpy(uniform.p_get_mapped_data(), data, dataSize);
        vmaFlushAllocation(allocator, uniform.allocation, 0, VK_WHOLE_SIZE);

        buffer_barrier(
            uniform,
            0,
            vk::PipelineStageFlagBits::eHost,
            vk::AccessFlagBits::eHostWrite,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead
        );
    }
    else {
        const Buffer stagingBuffer = make_staging_buffer(dataSize);
        memcpy(stagingBuffer.p_get_mapped_data(), data, dataSize);
        vmaFlushAllocation(allocator, uniform.allocation, 0, VK_WHOLE_SIZE);

        buffer_barrier(
            stagingBuffer,
            0,
            vk::PipelineStageFlagBits::eHost,
            vk::AccessFlagBits::eHostWrite,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead
        );

        copy_buffer(stagingBuffer, uniform , 0, 0, dataSize);

        buffer_barrier(
            uniform,
            0,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eUniformRead
        );
    }
}

void CommandBuffer::update_uniform(const void* data, const u64 dataSize, const Buffer& uniform) const
{
    const auto pBufferData = uniform.p_get_mapped_data();
    memcpy(pBufferData, data, dataSize);
}

void CommandBuffer::dispatch(const u32 groupCountX, const u32 groupCountY, const u32 groupCountZ) const
{
    cmd.dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::set_up_render_pass(
    const vk::Extent2D extent,
    const VkRenderingAttachmentInfo* drawImage,
    const VkRenderingAttachmentInfo* depthImage) const
{
    vk::Rect2D renderArea;
    renderArea.extent = extent;
    VkRenderingInfo renderInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = nullptr};
    renderInfo.renderArea = renderArea;
    renderInfo.pColorAttachments = drawImage;
    renderInfo.pDepthAttachment = depthImage;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;

    cmd.beginRendering(renderInfo);
}

void CommandBuffer::end_render_pass() const
{
    cmd.endRendering();
}

void CommandBuffer::set_viewport(const f32 x, const f32 y, const f32 minDepth, const f32 maxDepth) const
{
    vk::Viewport viewport(0, 0, x, y);
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    cmd.setViewport(0, 1, &viewport);
}

void CommandBuffer::set_viewport(const vk::Extent2D extent, const f32 minDepth, const f32 maxDepth) const
{
    vk::Viewport viewport(0, 0, extent.width, extent.height);
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    cmd.setViewport(0, 1, &viewport);
}

void CommandBuffer::set_scissor(const u32 x, const u32 y) const
{
    vk::Rect2D scissor;
    const vk::Extent2D extent {x, y};
    scissor.extent = extent;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    cmd.setScissor(0, 1, &scissor);
}

void CommandBuffer::set_scissor(const vk::Extent2D extent) const
{
    vk::Rect2D scissor;
    scissor.extent = extent;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    cmd.setScissor(0, 1, &scissor);
}

void CommandBuffer::bind_index_buffer(const Buffer& indexBuffer) const
{
    cmd.bindIndexBuffer(indexBuffer.handle, 0, vk::IndexType::eUint32);
}

void CommandBuffer::bind_vertex_buffer(const Buffer& vertexBuffer) const
{
    const vk::DeviceSize offsets[] = {0};
    cmd.bindVertexBuffers(0, static_cast<vk::Buffer>(vertexBuffer.handle), offsets);
}

void CommandBuffer::set_push_constants(const void* pPushConstants, u64 size,
    const vk::ShaderStageFlags shaderStage) const
{
    cmd.pushConstants(pipeline.pipelineLayout, shaderStage, 0, size, pPushConstants);
}

void CommandBuffer::draw(const u32 count, const u32 startIndex) const
{
    cmd.drawIndexed(count, 1, startIndex, 0, 0);
}

Buffer CommandBuffer::make_staging_buffer(const u64 allocSize) const
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    VmaAllocationCreateFlags allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationCreateInfo allocationCI;
    allocationCI.flags = allocationFlags;
    allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
    Buffer newBuffer{};

    vmaCreateBuffer(allocator, reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocationCI, (VkBuffer*)&newBuffer.handle, &newBuffer.allocation, &newBuffer.info);

    return newBuffer;
}

Buffer CommandBuffer::create_device_buffer(const u64 size, const vk::BufferUsageFlags bufferUsage) const
{
    VmaAllocationCreateInfo allocationCI;
    allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    vk::BufferCreateInfo deviceBufferInfo;
    deviceBufferInfo.pNext = nullptr;
    deviceBufferInfo.size = size;
    deviceBufferInfo.usage = bufferUsage;

    Buffer deviceBuffer{};
    vmaCreateBuffer(
        allocator,
        reinterpret_cast<VkBufferCreateInfo*>(&deviceBufferInfo),
        &allocationCI,
        &deviceBuffer.handle,
        &deviceBuffer.allocation,
        &deviceBuffer.info);

    return deviceBuffer;
}

void CommandBuffer::destroy_buffer(const Buffer& buffer) const
{

}

void CommandBuffer::destroy_image(const Image& image) const
{

}

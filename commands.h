#pragma once
#include "common.h"
#include "device/resourcetypes.h"

class CommandBuffer {
public:
    void begin() const;
    void end() const;

    void image_barrier(vk::Image image, vk::ImageLayout currentLayout, vk::ImageLayout newLayout) const;
    void buffer_barrier(
        const Buffer &buffer,
        vk::DeviceSize offset,
        vk::PipelineStageFlags srcStageFlags,
        vk::AccessFlags srcAccessMask,
        vk::PipelineStageFlags dstStageFlags,
        vk::AccessFlags dstAccessMask
    ) const;

    void memory_barrier(
    vk::PipelineStageFlags2 srcStageFlags, vk::AccessFlags2 srcAccessMask,
    vk::PipelineStageFlags2 dstStageFlags, vk::AccessFlags2 dstAccessMask) const;

    void blit_image(vk::Image src, vk::Image dst, vk::Extent3D srcSize, vk::Extent3D dstSize) const;
    void copy_buffer(const Buffer &bufferSrc, const Buffer &bufferDst, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset, vk::DeviceSize dataSize) const;
    void copy_buffer_to_image(const Buffer& buffer, const Image& image, vk::ImageLayout layout, const std::span<vk::BufferImageCopy>& regions) const;

    void upload_image(void* data, const Image& image) const;
    void upload_uniform(const void* data, u64 dataSize, const Buffer& uniform) const;
    void update_uniform(const void* data, u64 dataSize, const Buffer& uniform) const;

    void dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) const;

    void set_up_render_pass(vk::Extent2D extent, const VkRenderingAttachmentInfo* drawImage, const VkRenderingAttachmentInfo* depthImage) const;
    void end_render_pass() const;
    void set_viewport(f32 x, f32 y, f32 minDepth, f32 maxDepth) const;
    void set_viewport(vk::Extent2D extent, f32 minDepth, f32 maxDepth) const;
    void set_scissor(u32 x, u32 y) const;
    void set_scissor(vk::Extent2D extent) const;
    void bind_index_buffer(const Buffer &indexBuffer) const;
    void bind_vertex_buffer(const Buffer &vertexBuffer) const;
    void set_push_constants(const void *pPushConstants, u64 size, const vk::ShaderStageFlags shaderStage) const;
    void draw(u32 count, u32 startIndex) const;

    void set_handle(const vk::CommandBuffer& _cmd) { cmd = _cmd; }
    void bind_pipeline(vk::PipelineBindPoint bindPoint, const Pipeline& _pipeline);
    void set_allocator(const VmaAllocator& _allocator) { allocator = _allocator; }

    [[nodiscard]] vk::CommandBuffer get_handle() const { return cmd; }

private:
    [[nodiscard]] Buffer make_staging_buffer(u64 allocSize) const;
    [[nodiscard]] Buffer create_device_buffer(u64 size, vk::BufferUsageFlags bufferUsage) const;
    void destroy_buffer(const Buffer& buffer) const;
    void destroy_image(const Image& image) const;

    Pipeline pipeline{};
    vk::CommandBuffer cmd{};
    VmaAllocator allocator{};
};

#pragma once
#include "device.h"

#include <fstream>

class Context {
public:
    Context(std::string_view appName, u32 _width, u32 _height);
    ~Context();

    void frame_submit(const std::function<void(const CommandBufferInfo& cmd)>&& func);
    CommandBufferInfo get_fif() const { return device->commandBufferInfos[frameNumber % MAX_FRAMES_IN_FLIGHT]; }

    [[nodiscard]] vk::Device get_device_handle() const { return device->get_handle(); }
    [[nodiscard]] vk::Queue get_graphic_queue() const { return device->get_graphics_queue(); }
    [[nodiscard]] vk::Queue get_transfer_queue() const { return device->get_transferQueue(); }
    [[nodiscard]] vk::Queue get_present_queue() const { return device->get_present_queue(); }
    [[nodiscard]] GLFWwindow* p_get_window() const { return device->get_window_p(); }
    [[nodiscard]] Image& get_draw_image() const { return device->get_draw_image(); }
    [[nodiscard]] Image& get_depth_image() const { return device->get_depth_image(); }
    [[nodiscard]] std::array<CommandBufferInfo, MAX_FRAMES_IN_FLIGHT>& get_command_buffer_infos() const { return device->commandBufferInfos; }

    [[nodiscard]] Buffer create_buffer(
     u64 allocationSize,
     vk::BufferUsageFlags usage,
     VmaMemoryUsage memoryUsage,
     VmaAllocationCreateFlags flags = VMA_ALLOCATION_CREATE_MAPPED_BIT);

    [[nodiscard]] Image create_image(vk::Extent3D size, VkFormat format, VkImageUsageFlags usage, u32 mipLevels, bool mipmapped) const;
    [[nodiscard]] Sampler create_sampler(vk::Filter minFilter, vk::Filter magFilter, vk::SamplerMipmapMode mipmapMode) const;
    [[nodiscard]] Shader create_shader(std::string_view filePath) const;

    [[nodiscard]] vk::Extent2D get_display_extent() const { return device->get_display_extent(); }

private:
    std::unique_ptr<Device> device;
    u32 frameNumber = 0;
    u32 swapchainImageIndex = 0;
};

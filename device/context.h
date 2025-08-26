#pragma once
#include "device.h"

#include "../glmdefines.h"
#include <glm/glm.hpp>

#include <fstream>

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cameraPosition;
};

class Context {
public:
    explicit Context(std::string_view appName, u32 width, u32 height);
    ~Context();

    void frame_submit(const std::function<void(FrameInFlight& cmd, SwapchainImageData& swapchainData)>&& func);
    [[nodiscard]] FrameInFlight& get_fif() const { return m_device->commandBufferInfos[frameNumber % MAX_FRAMES_IN_FLIGHT]; }

    [[nodiscard]] vk::Device get_device_handle() const { return m_device->get_handle(); }
    [[nodiscard]] Device& get_device() const { return *m_device; }
    [[nodiscard]] VmaAllocator get_allocator() const { return m_device->get_allocator(); }
    [[nodiscard]] vk::Queue get_graphic_queue() const { return m_device->get_graphics_queue(); }
    [[nodiscard]] vk::Queue get_transfer_queue() const { return m_device->get_transferQueue(); }
    [[nodiscard]] vk::Queue get_present_queue() const { return m_device->get_present_queue(); }
    [[nodiscard]] GLFWwindow* p_get_window() const { return m_device->get_window_p(); }
    [[nodiscard]] Image& get_draw_image() const { return m_device->get_draw_image(); }
    [[nodiscard]] Image& get_depth_image() const { return m_device->get_depth_image(); }
    [[nodiscard]] std::array<FrameInFlight, MAX_FRAMES_IN_FLIGHT>& get_command_buffer_infos() const { return m_device->commandBufferInfos; }
    [[nodiscard]] ImmediateCommandInfo get_immediate_info() const { return m_device->get_immediate_info(); }

    [[nodiscard]] Buffer create_buffer(
     u64 allocationSize,
     vk::BufferUsageFlags usage,
     VmaMemoryUsage memoryUsage,
     VmaAllocationCreateFlags flags = VMA_ALLOCATION_CREATE_MAPPED_BIT) const;

    [[nodiscard]] Buffer create_staging_buffer(const u64 size) const;

    [[nodiscard]] Image create_image(vk::Extent3D size, VkFormat format, VkImageUsageFlags usage, u32 mipLevels, bool mipmapped) const;
    [[nodiscard]] Sampler create_sampler(vk::Filter minFilter, vk::Filter magFilter, vk::SamplerMipmapMode mipmapMode) const;
    [[nodiscard]] Shader create_shader(std::string_view filePath) const;

    void destroy_shader(const Shader& shader) const;

    [[nodiscard]] vk::Extent2D get_display_extent() const { return m_device->get_display_extent(); }

    void init_imgui() const;

    void submit_work(
        const CommandBuffer& cmd,
        vk::PipelineStageFlagBits2 wait,
        vk::PipelineStageFlagBits2 signal,
        vk::Semaphore acquiredSemaphore, vk::Semaphore renderEndSemaphore,
        vk::Fence renderFence) const;
    void submit_upload_work();
    void submit_immediate_work(std::function<void(CommandBuffer cmd)>&& function) const;

private:

    std::unique_ptr<Device> m_device;
    u32 frameNumber = 0;
    u32 swapchainImageIndex = 0;
};

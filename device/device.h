#pragma once
#include "../common.h"
#include "../commands.h"
#include "debug.h"
#include "resourcetypes.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <limits>

#include <functional>

#include <set>

static constexpr u8 MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

static const std::vector deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME
    //VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
};

struct QueueFamilyIndices {
    std::optional<u32> graphicsFamily;
    std::optional<u32> presentFamily;
    std::optional<u32> computeFamily;
    std::optional<u32> transferFamily;

    [[nodiscard]] bool is_complete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

struct SwapchainImageData
{
    vk::Image swapchainImage;
    vk::ImageView swapchainImageView;
    vk::Semaphore renderEndSemaphore;
};

struct FrameInFlight
{
    Buffer SceneData{};
    vk::CommandPool commandPool;
    CommandBuffer commandBuffer{};
    vk::Semaphore acquiredSemaphore;
    vk::Fence renderFence;
    bool resizeRequested = false;
};

struct ImmediateCommandInfo {
    vk::Fence immediateFence;
    vk::CommandPool immediateCommandPool;
    vk::CommandBuffer immediateCommandBuffer;
};

class Device {
public:
    Device(std::string_view appName, u32 _width, u32 _height);
    ~Device();
public:
    std::vector<SwapchainImageData> swapchainImageData;
    std::array<FrameInFlight, MAX_FRAMES_IN_FLIGHT> commandBufferInfos;

    [[nodiscard]] vk::Device get_handle() const { return handle; }
    [[nodiscard]] vk::Queue get_graphics_queue() const { return graphicsQueue; }
    [[nodiscard]] vk::Queue get_present_queue() const { return presentQueue; }
    [[nodiscard]] vk::Queue get_transferQueue() const { return transferQueue; }
    [[nodiscard]] VmaAllocator get_allocator() const { return allocator; }
    [[nodiscard]] Image& get_draw_image() { return m_DrawImage; }
    [[nodiscard]] Image& get_depth_image() { return m_DepthImage; }
    [[nodiscard]] vk::Extent2D get_display_extent();
    [[nodiscard]] vk::SwapchainKHR get_swapchain() const { return m_Swapchain; }
    [[nodiscard]] GLFWwindow* get_window_p() const { return m_Window; }
    [[nodiscard]] ImmediateCommandInfo get_immediate_info() const { return immediateInfo; }
    [[nodiscard]] VkRenderingAttachmentInfo get_draw_attachment() const { return drawAttachment; }
    [[nodiscard]] VkRenderingAttachmentInfo get_depth_attachment() const { return depthAttachment; }

    bool recreate_swapchain();
    void recreate_draw_images();
    void init_imgui() const;

private:
    void init_window(vk::Extent2D extent);
    void init_instance();
    void init_surface();
    void select_gpu();
    void init_device();
    void init_swapchain();
    void init_commands();
    void init_sync_objects();
    void init_allocator();
    void init_draw_images();
    void init_depth_images();

private:
    std::vector<const char*> get_required_extensions();
    bool gpu_is_suitable(vk::PhysicalDevice gpu);
    [[nodiscard]] QueueFamilyIndices find_queue_families(vk::PhysicalDevice gpu) const;
    bool check_device_extension_support(vk::PhysicalDevice gpu);
    SwapChainSupportDetails query_swapchain_support(vk::PhysicalDevice gpu);
    vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    [[nodiscard]] vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D extent) const;

    void destroy_swapchain();
    void destroy_draw_images() const;
    void destroy_depth_images() const;

private:
    std::string applicationName;
    std::vector<const char*> validationLayers {"VK_LAYER_KHRONOS_validation"};
    vk::Instance instance;
    vk::Device handle;
    vk::PhysicalDevice m_Gpu;

    GLFWwindow* m_Window = nullptr;
    vk::SurfaceKHR m_Surface;
    vk::SwapchainKHR m_Swapchain;
    vk::Format m_SwapchainFormat;
    vk::Extent2D m_SwapchainExtent;

    Image m_DrawImage;
    Image m_DepthImage;
    VkRenderingAttachmentInfo drawAttachment;
    VkRenderingAttachmentInfo depthAttachment;

    u32 graphicsQueueIndex{}, computeQueueIndex{}, presentQueueIndex{}, transferQueueIndex{};
    vk::Queue graphicsQueue, computeQueue, presentQueue, transferQueue;

    ImmediateCommandInfo immediateInfo;

    VmaAllocator allocator{};
};

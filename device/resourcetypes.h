#pragma once
#include "../common.h"

enum QueueType { Graphics, Compute, Transfer };

struct Buffer {
    VkBuffer handle;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    vk::MemoryPropertyFlags properties;
    vk::DeviceAddress deviceAddress;

    [[nodiscard]] void* p_get_mapped_data() const { return info.pMappedData; }
};

struct Pipeline {
    vk::Pipeline pipeline;
    vk::PipelineLayout pipelineLayout;
    vk::DescriptorSetLayout setLayout;
    vk::DescriptorSet set;
};

struct Shader {
    vk::ShaderModule module;
    std::string path;
    bool isCompute = false;
};

class Image {
public:
    VkImage handle{};
    VkImageView view{};
    VmaAllocation allocation{};
    VkExtent3D extent{};
    VkFormat format{};
    u32 mipLevels{};
};

struct Sampler {
    vk::Filter magFilter;
    vk::Filter minFilter;
    vk::Sampler sampler;
};

#pragma once
#include "../common.h"
#include "../device/context.h"
#include "../pipelines/descriptors.h"
#include "../commands.h"
#include "../glmdefines.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <ktx.h>

typedef ktx_uint64_t ku64;
typedef ktx_uint32_t ku32;
typedef ktx_uint16_t ku16;
typedef ktx_uint8_t ku8;

typedef ktx_int64_t ki64;
typedef ktx_int32_t ki32;
typedef ktx_int16_t ki16;

typedef glm::vec4 Plane;
typedef std::array<Plane, 5> Frustum;

struct AABB {
    glm::vec3 min{};
    glm::vec3 max{};
};

struct BoundingSphere {
    glm::vec4 center{};
    f32 radius{};
};

struct Surface {
    AABB boundingVolume;
    u32 initialIndex{};
    u32 indexCount{};
    MaterialHandle material{};
};

struct GPUSurface {
    glm::mat4 renderMatrix{};
    AABB boundingVolume{};
    u32 initialIndex{};
    u32 indexCount{};
    u32 materialIndex{};
};

struct Mesh {
    std::vector<Surface> surfaces;
};

struct Vertex {
    glm::vec3 position{};
    float uvX{};
    glm::vec3 normal{};
    float uvY{};
    glm::vec4 colour{};
};

enum class MaterialPass : u8 {
    Opaque, Transparent
};

enum class MaterialType : u8 {
    transparent, opaque, invalid
};

struct Material {
    glm::vec4 baseColorFactor{};
    f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

    TextureHandle baseColorTexture{};
    TextureHandle mrTexture{};
    TextureHandle normalTexture{};
    TextureHandle occlusionTexture{};
    TextureHandle emissiveTexture{};
    u32 bufferOffset{};
};

struct GPUMaterial {
    glm::vec4 baseColorFactor{};
    f32 metalnessFactor{}, roughnessFactor{}, emissiveStrength{};

    u32 baseColorTexture{};
    u32 mrTexture{};
    u32 normalTexture{};
    u32 occlusionTexture{};
    u32 emissiveTexture{};
};

enum class LightType : u8 {
    Directional, Point, Spot
};

struct Light {
    glm::vec3 position{};
    glm::vec3 colour{};
    f32 intensity{};
    f32 range{};
    f32 innerAngle{};
    f32 outerAngle{};
};

class SceneManager;

struct Node {
    std::vector<NodeHandle> children;
    glm::mat4 localMatrix{};
    glm::mat4 worldMatrix{};

    NodeHandle parent{};
    MeshHandle mesh{};
    NodeHandle light{};

    auto refresh(const glm::mat4 &parentMatrix, SceneManager &sceneManager) -> void;
};

struct Renderable {
    Surface surface;
    glm::mat4 worldMatrix{};
};

struct PushConstants {
    glm::mat4 renderMatrix;
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress materialBuffer;
    vk::DeviceAddress lightBuffer;
    u32 materialIndex;
    u32 numLights;
};


struct Scene {
    std::vector<NodeHandle> nodes;
    std::vector<NodeHandle> renderableNodes;
    std::vector<NodeHandle> opaqueNodes;
    std::vector<NodeHandle> transparentNodes;
    std::vector<NodeHandle> lightNodes;
    std::vector<MeshHandle> meshes;
    std::vector<MaterialHandle> materials;
    std::vector<SamplerHandle> samplers;
    std::vector<TextureHandle> textures;
    std::vector<LightHandle> lights;
};

struct ktxTextureData {
    std::vector<ktxTexture*> texturePs;
    std::vector<std::vector<vk::BufferImageCopy>> copyRegions;
    u64 stagingBufferSize{};
};

struct ResourceData {
    std::vector<Scene> scenes;
    std::vector<u16> sceneMetadata;
    std::vector<Node> nodes;
    std::vector<u16> nodeMetadata;
    std::vector<Mesh> meshes;
    std::vector<u16> meshMetadata;
    std::vector<Material> materials;
    std::vector<u16> materialMetadata;
    std::vector<Image> textures;
    std::vector<u16> texturesMetadata;
    std::vector<Sampler> samplers;
    std::vector<u16> samplerMetadata;
    std::vector<Light> lights;
    std::vector<u16> lightMetadata;

    std::string lightNames;
    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer materialBuffer;
    Buffer lightBuffer;
};

class SceneManager {

public:
    explicit SceneManager(const std::shared_ptr<ResourceData>& resourceData)
    : m_resourceData(resourceData) {
        for (const auto& [surfaces] : m_resourceData->meshes)
            for (const auto& surface : surfaces)
                numSurfaces++;
    };

    void draw_scene(const CommandBuffer& cmd, SceneHandle handle, const glm::mat4& viewProjectionMatrix);
    void cpu_frustum_culling(const Scene& scene, const glm::mat4& viewProjectionMatrix);

    [[nodiscard]] Scene& get_scene(SceneHandle handle) const;
    [[nodiscard]] Node& get_node(NodeHandle handle) const;
    [[nodiscard]] Light& get_light(LightHandle handle) const;
    [[nodiscard]] Material& get_material(MaterialHandle handle) const;
    [[nodiscard]] Mesh& get_mesh(MeshHandle handle) const;
    [[nodiscard]] Image& get_texture(TextureHandle handle) const;
    [[nodiscard]] Sampler& get_sampler(SamplerHandle handle) const;
    [[nodiscard]] Light* get_all_lights_p() const { return m_resourceData->lights.data(); }
    [[nodiscard]] std::string& get_light_names() const { return m_resourceData->lightNames; }
    [[nodiscard]] u64 get_num_lights() const { return m_resourceData->lights.size(); }

    void update_light_buffer(const CommandBuffer& cmd) const;
    void update_nodes(const glm::mat4& rootMatrix, SceneHandle handle);
    void release_gpu_resources(const Context& context) const;

private:
    std::shared_ptr<ResourceData> m_resourceData;
    std::vector<Renderable> m_renderables;
    PushConstants pc{};
    u64 numSurfaces = 0;

    void assert_handle(SceneHandle handle) const;
    void assert_handle(NodeHandle handle) const;
    void assert_handle(LightHandle handle) const;
    void assert_handle(MaterialHandle handle) const;
    void assert_handle(MeshHandle handle) const;
    void assert_handle(TextureHandle handle) const;
    void assert_handle(SamplerHandle handle) const;
};

struct GeometricData {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
};

struct GeoBuffers {
    Buffer vertexBuffer{};
    Buffer indexBuffer{};
    u64 vertexBufferSize = 0;
    u64 indexBufferSize = 0;
};

class SceneBuilder {
public:
    explicit SceneBuilder(Context& context, const std::shared_ptr<ResourceData>& resourceData);

    [[nodiscard]] std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& path) const;
    std::optional<SceneHandle> build_scene(fastgltf::Asset& asset);

    void write_textures(DescriptorBuilder& builder) const;

private:
    Context& m_context;
    std::shared_ptr<ResourceData> m_resourceData;

    void create_nodes(const fastgltf::Asset& asset, Scene& scene) const;
    GeometricData create_meshes(const fastgltf::Asset& asset, Scene& scene) const;
    void create_materials(const fastgltf::Asset& asset, Scene& scene) const;
    void create_samplers(const fastgltf::Asset& asset, Scene& scene) const;
    void create_lights(const fastgltf::Asset& asset, Scene& scene) const;
    void create_images(const std::vector<ktxTexture*>& ktxTexturePs, Scene& scene) const;

    ktxTextureData ktx_texture_data_from_gltf(fastgltf::Asset& asset);

    void upload_scene_data(const GeometricData& geoData, ktxTextureData& ktxTextureData) const;

    [[nodiscard]] GeoBuffers prep_geo_buffers(const GeometricData& geoData) const;
    [[nodiscard]] Buffer prep_vertex_index_staging(const GeometricData& geoData) const;
    [[nodiscard]] Buffer prepare_image_staging(const ktxTextureData& textureData) const;
    [[nodiscard]] Buffer prepare_material_buffer() const;
    [[nodiscard]] Buffer prepare_light_buffer() const;

    [[nodiscard]] u16 get_metadata_at_index(u32 index) const;

};

template<typename T>
u32 get_handle_index(const T handle) {
    constexpr u32 indexMask = (1 << 16) - 1;
    const auto handleAsU32 = static_cast<u32>(handle);
    return handleAsU32 & indexMask;
}

template<typename T>
u32 get_handle_metadata(const T handle) {
    constexpr u32 indexMask = (1 << 16) - 1;
    constexpr u32 metaDataMask = ~indexMask;
    const auto handleAsU32 = static_cast<u32>(handle);
    return (handleAsU32 & metaDataMask) >> 16;
}

Frustum compute_frustum(const glm::mat4& viewProjection);
AABB recompute_aabb(const AABB& oldAABB, const glm::mat4& transform);

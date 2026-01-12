
#include "scenemanager.h"

Scene& SceneManager::get_scene(const SceneHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->scenes[index];
}

Node& SceneManager::get_node(const NodeHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->nodes[index];
}

SpotLight& SceneManager::get_spot_light(const SpotLightHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->spotLights[index];
}

PointLight& SceneManager::get_point_light(PointLightHandle handle) const
{
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->pointLights[index];
}

Material& SceneManager::get_material(const MaterialHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->materials[index];
}

Mesh& SceneManager::get_mesh(const MeshHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->meshes[index];
}

Image& SceneManager::get_texture(const TextureHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->textures[index];
}

Sampler& SceneManager::get_sampler(const SamplerHandle handle) const {
    assert_handle(handle);
    const u32 index = get_handle_index(handle);
    return m_resourceData->samplers[index];
}

void SceneManager::assert_handle(const SceneHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index <= m_resourceData->scenes.size() && "Handle out of bounds");
    assert(m_resourceData->sceneMetadata[index] == metaData && "Handle metadata does not match an existing scene");
}

void SceneManager::assert_handle(const NodeHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index <= m_resourceData->nodes.size() && "Handle out of bounds");
    assert(m_resourceData->nodeMetadata[index] == metaData && "Handle metadata does not match an existing node");
}

void SceneManager::assert_handle(const SpotLightHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index <= m_resourceData->spotLights.size() && "Handle out of bounds");
    assert(m_resourceData->spotLightMetadata[index] == metaData && "Handle metadata does not match an existing light");
}

void SceneManager::assert_handle(const PointLightHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index <= m_resourceData->pointLights.size() && "Handle out of bounds");
    assert(m_resourceData->pointLightMetaData[index] == metaData && "Handle metadata does not match an existing light");
}

void SceneManager::assert_handle(const MaterialHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index < m_resourceData->materials.size());
    assert(m_resourceData->materialMetadata[index] == metaData);
}

void SceneManager::assert_handle(const MeshHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index < m_resourceData->meshes.size() && "Mesh index out of range");
    assert(m_resourceData->meshMetadata[index] == metaData && "Mesh metadata doesn't match");
}

void SceneManager::assert_handle(const TextureHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index < m_resourceData->textures.size());
    assert(m_resourceData->texturesMetadata[index] == metaData);
}

void SceneManager::assert_handle(const SamplerHandle handle) const {
    const u32 metaData = get_handle_metadata(handle);
    const u32 index = get_handle_index(handle);

    assert(index < m_resourceData->samplers.size());
    assert(m_resourceData->samplerMetadata[index] == metaData);
}

void SceneManager::draw_scene_opaque(const CommandBuffer &cmd, const SceneHandle handle, const glm::mat4 &viewProjectionMatrix, const glm::vec3& position) {
    pc.vertexBuffer = m_resourceData->vertexBuffer.deviceAddress;
    pc.materialBuffer = m_resourceData->materialBuffer.deviceAddress;
    pc.pointLightBuffer = m_resourceData->pointLightBuffer.deviceAddress;
    pc.spotLightBuffer = m_resourceData->spotLightBuffer.deviceAddress;
    pc.numPointLights = static_cast<u32>(get_num_point_lights());
    pc.numSpotLights = static_cast<u32>(get_num_spot_lights());

    const auto scene = get_scene(handle);
    cmd.bind_index_buffer(m_resourceData->indexBuffer);
    cpu_frustum_culling(scene, viewProjectionMatrix);

    for (const auto&[surface, worldMatrix] : m_OpaqueRenderables) {
        pc.renderMatrix = worldMatrix;
        pc.materialIndex = get_handle_index(surface.material);
        cmd.set_push_constants(&pc, sizeof(pc), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        cmd.draw(surface.indexCount, surface.initialIndex);
    }

    m_OpaqueRenderables.clear();
}

void SceneManager::draw_scene_transparent(const CommandBuffer &cmd, const SceneHandle handle, const glm::mat4 &viewProjectionMatrix, const glm::vec3 &position) {
    for (const auto&[surface, worldMatrix] : m_blendedRenderables) {
        pc.renderMatrix = worldMatrix;
        pc.materialIndex = get_handle_index(surface.material);
        cmd.set_push_constants(&pc, sizeof(pc), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        cmd.draw(surface.indexCount, surface.initialIndex);
    }

    sort_transparencies(position);
    m_blendedRenderables.clear();

}

void SceneManager::cpu_frustum_culling(const Scene& scene, const glm::mat4 &viewProjectionMatrix) {
    const Frustum viewFrustum = compute_frustum(viewProjectionMatrix);

    for (const auto&[surface, worldMatrix] : scene.opaqueRenderables) {
        AABB transformedAABB = recompute_aabb(surface.boundingVolume, worldMatrix);
        const auto [min, max] = transformedAABB;
        bool visible = true;
        for (const auto& plane : viewFrustum) {
            int out = 0;
            out += dot(plane, glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            if (out == 8) visible = false;
        }

        if (visible) m_OpaqueRenderables.push_back({surface, worldMatrix});
    }

    for (const auto&[surface, worldMatrix] : scene.blendedRenderables) {
        AABB transformedAABB = recompute_aabb(surface.boundingVolume, worldMatrix);
        const auto [min, max] = transformedAABB;
        bool visible = true;
        for (const auto& plane : viewFrustum) {
            int out = 0;
            out += dot(plane, glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            out += dot(plane, glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0f ? 1.0f : 0.0f;
            if (out == 8) visible = false;
        }

        if (visible) m_blendedRenderables.push_back({ surface, worldMatrix });
    }
}

void SceneManager::sort_transparencies(const glm::vec3& position) {
    glm::vec4 pos(position, 0.0f);
    std::ranges::sort(m_blendedRenderables, [&pos](const Renderable& prev, const Renderable& curr) {
        constexpr glm::vec4 meshPos(1.0f);
        const f32 currentPos = glm::length((meshPos * curr.worldMatrix) - pos);
        const f32 prevPos = glm::length((meshPos * prev.worldMatrix) - pos);
        return currentPos > prevPos;
    });
}

void SceneManager::update_light_buffer(const CommandBuffer& cmd) const {
    const auto& pointLights = m_resourceData->pointLights;
    const auto& spotLights = m_resourceData->spotLights;
    const auto pointLightBuffer = m_resourceData->pointLightBuffer;
    const auto spotLightBuffer = m_resourceData->spotLightBuffer;

    if (!pointLights.empty())
    {
        auto* pointLightData = static_cast<PointLight*>(pointLightBuffer.p_get_mapped_data());
        for (u64 i = 0; i < pointLights.size(); i++)
            pointLightData[i] = pointLights[i];

        constexpr u64 pointLightBufferSize = MAX_POINT_LIGHTS * sizeof(PointLight);
        cmd.update_uniform(pointLightData, pointLightBufferSize, pointLightBuffer);
    }
    else
    {
        constexpr PointLight pointLight{};
        cmd.update_uniform(&pointLight, sizeof(PointLight), pointLightBuffer);
    }
    if (!spotLights.empty())
    {
        auto* spotLightData = static_cast<SpotLight*>(spotLightBuffer.p_get_mapped_data());
        for (u64 i = 0; i < spotLights.size(); i++)
            spotLightData[i] = spotLights[i];

        constexpr u64 spotLightBufferSize = MAX_SPOT_LIGHTS * sizeof(SpotLight);
        cmd.update_uniform(spotLightData, spotLightBufferSize, spotLightBuffer);
    }
    else
    {
        constexpr SpotLight spotLight{};
        cmd.update_uniform(&spotLight, sizeof(SpotLight), spotLightBuffer);
    }
}

PointLightHandle SceneManager::add_point_light(const PointLight& light) const
{
    auto& pointLights = m_resourceData->pointLights;
    pointLights.push_back(light);
    return static_cast<PointLightHandle>(pointLights.size() << 16 | pointLights.size());
}

SpotLightHandle SceneManager::add_spot_light(const SpotLight& light) const
{
    auto& spotLights = m_resourceData->spotLights;
    spotLights.push_back(light);
    return static_cast<SpotLightHandle>(spotLights.size() << 16 | spotLights.size());
}

void SceneManager::remove_point_light(const PointLightHandle handle) const
{
    auto& pointLights = m_resourceData->pointLights;
    auto pointLight = get_point_light(handle);
    std::swap(pointLight, *pointLights.end());
    pointLights.pop_back();
}

void SceneManager::remove_point_light(const u32 index) const
{
    assert(index < static_cast<u32>(m_resourceData->pointLights.size()) && "Tried to remove spot light out of bounds");
    auto& pointLights = m_resourceData->pointLights;
    auto& pointLight = pointLights[index];
    std::swap(pointLight, *pointLights.end());
    pointLights.pop_back();
}

void SceneManager::remove_spot_light(const SpotLightHandle handle) const
{
    auto& spotLights = m_resourceData->spotLights;
    auto spotLight = get_spot_light(handle);
    std::swap(spotLight, *spotLights.end());
    spotLights.pop_back();
}

void SceneManager::remove_spot_light(const u32 index) const
{
    assert(index < static_cast<u32>(m_resourceData->spotLights.size()) && "Tried to remove spot light out of bounds");
    auto& spotLights = m_resourceData->pointLights;
    auto& spotLight = spotLights[index];
    std::swap(spotLight, *spotLights.end());
    spotLights.pop_back();
}

void SceneManager::update_nodes(const glm::mat4 &rootMatrix, const SceneHandle handle) {
    for (const auto& scene = get_scene(handle); const auto nodeHandle : scene.nodes) {
        auto& node = get_node(nodeHandle);
        node.refresh(rootMatrix, *this);
    }
}

void SceneManager::release_gpu_resources(const Context& context) const {
    auto allocator = context.get_allocator();
    auto deviceHandle = context.get_device_handle();

    vmaDestroyBuffer(allocator, m_resourceData->pointLightBuffer.handle, m_resourceData->pointLightBuffer.allocation);
    vmaDestroyBuffer(allocator, m_resourceData->spotLightBuffer.handle, m_resourceData->spotLightBuffer.allocation);

    vmaDestroyBuffer(allocator, m_resourceData->materialBuffer.handle, m_resourceData->materialBuffer.allocation);

    vmaDestroyBuffer(allocator, m_resourceData->vertexBuffer.handle, m_resourceData->vertexBuffer.allocation);
    vmaDestroyBuffer(allocator, m_resourceData->indexBuffer.handle, m_resourceData->indexBuffer.allocation);

    for (auto& texture : m_resourceData->textures) {
        vmaDestroyImage(allocator, texture.handle, texture.allocation);
        vkDestroyImageView(deviceHandle, texture.view, nullptr);
    }

    for (auto& sampler : m_resourceData->samplers) {
        vkDestroySampler(deviceHandle, sampler.sampler, nullptr);
    }
}

Frustum compute_frustum(const glm::mat4 &viewProjection) {
    const glm::mat4 transpose = glm::transpose(viewProjection);

    const Plane leftPlane = transpose[3] + transpose[0];
    const Plane rightPlane = transpose[3] - transpose[0];
    const Plane bottomPlane = transpose[3] + transpose[1];
    const Plane topPlane = transpose[3] - transpose[1];
    const Plane nearPlane = transpose[3] + transpose[2];

    return {leftPlane, rightPlane, bottomPlane, topPlane, nearPlane};
}

AABB recompute_aabb(const AABB &oldAABB, const glm::mat4 &transform) {
    const glm::vec3& min = oldAABB.min;
    const glm::vec3& max = oldAABB.max;

    const glm::vec3 corners[8] = {
        glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f)),
        glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f))
    };


    AABB result {corners[0], corners[0]};

    for (const auto& corner : corners) {
        result.min = glm::min(result.min, corner);
        result.max = glm::max(result.max, corner);
    }

    return result;
}

void Node::refresh(const glm::mat4& parentMatrix, SceneManager& sceneManager) {
    worldMatrix = parentMatrix * localMatrix;
    for (const auto childHandle : children)
        sceneManager.get_node(childHandle).refresh(worldMatrix, sceneManager);
}

SceneBuilder::SceneBuilder(Context &context, const std::shared_ptr<ResourceData>& resourceData)  : m_context(context) {
    m_resourceData = resourceData;
}

std::optional<fastgltf::Asset> SceneBuilder::parse_gltf(const std::filesystem::path &path) {
    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
    constexpr auto options =
        fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble  |
                fastgltf::Options::LoadExternalBuffers;


        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None)
            throw std::runtime_error("Failed to read glTF file");

        fastgltf::Asset gltf;

        auto type = determineGltfFileType(data.get());
        if (type == fastgltf::GltfType::glTF) {
            auto load = parser.loadGltf(data.get(), path.parent_path(), options);
            if (load) {
                gltf = std::move(load.get());
            }
            else {
                std::cerr << "Failed to load glTF file" << to_underlying(load.error()) << std::endl;
                return{};
            }
        }
        else if (type == fastgltf::GltfType::GLB) {
            auto load = parser.loadGltfBinary(data.get(), path.parent_path(), options);
            if (load) {
                gltf = std::move(load.get());
            }
            else {
                std::cerr << "Failed to load glTF file" << to_underlying(load.error()) << '\n';
                return{};
            }
        }
        else {
            std::cerr << "Failed to determine glTF container" << '\n';
            return {};
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path());
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            throw std::runtime_error("failed to read GLTF buffer");
        }

        std::println();

        std::println("Creating scene...");
        std::println("Gltf Extensions Used: ");
        for (const auto& extension : gltf.extensionsRequired)
            std::println("{}", extension);
        for (const auto& extension : gltf.extensionsUsed)
            std::println("{}", extension);
        std::println();

        return gltf;
}

std::optional<SceneHandle> SceneBuilder::build_scene(fastgltf::Asset &asset) {
    Scene newScene;
    auto textureData = ktx_texture_data_from_gltf(asset);
    const auto geoData = create_meshes(asset, newScene);
    create_materials(asset, newScene);
    create_lights(asset, newScene);
    create_samplers(asset, newScene);
    create_nodes(asset, newScene);
    create_images(textureData.texturePs, newScene);

    upload_scene_data(geoData, textureData);

    auto& scenes = m_resourceData->scenes;
    auto& sceneMetadata = m_resourceData->sceneMetadata;
    const u16 metadata = scenes.size();
    auto handle = static_cast<SceneHandle>(metadata << 16 | scenes.size());

    scenes.push_back(std::move(newScene));
    sceneMetadata.push_back(metadata);

    return handle;
}

void SceneBuilder::write_textures(DescriptorBuilder &builder) const {
    u32 i = 0;
    for (const auto& texture : m_resourceData->textures) {
        const auto sampler = m_resourceData->samplers[0];
        builder.write_image(i, texture.view, sampler.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
        i++;
    }
}

void SceneBuilder::create_nodes(const fastgltf::Asset& asset, Scene& scene) const {
    auto& nodes = m_resourceData->nodes;
    auto& nodeMetadata = m_resourceData->nodeMetadata;
    const auto numGltfNodes = nodes.size();
    nodes.reserve(numGltfNodes + asset.nodes.size());

    for (const auto& gltfNode : asset.nodes) {
        Node node;
        u16 metadata = nodes.size();

        std::visit(fastgltf::visitor { [&](fastgltf::math::fmat4x4 matrix) {
            memcpy(&node.localMatrix, matrix.data(), sizeof(matrix));
        },
            [&](fastgltf::TRS transform) {
                const glm::vec3 translation(transform.translation.x(), transform.translation.y(), transform.translation.z());
                const glm::quat rotation(transform.rotation.w(), transform.rotation.x(), transform.rotation.y(), transform.rotation.z());
                const glm::vec3 scale(transform.scale.x(), transform.scale.y(), transform.scale.z());

                const glm::mat4 translationMatrix = translate(glm::mat4(1.0f), translation);
                const glm::mat4 rotationMatrix = toMat4(rotation);
                const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

                node.localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
            }
        }, gltfNode.transform);


        const auto handle = static_cast<NodeHandle>(metadata << 16 | nodes.size());
        scene.nodes.push_back(handle);

        [[likely]] if (gltfNode.meshIndex.has_value()) {

            scene.renderableNodes.push_back(handle);

            const u16 meshMetadata = get_metadata_at_index(gltfNode.meshIndex.value());
            node.mesh = static_cast<MeshHandle>(meshMetadata << 16 | gltfNode.meshIndex.value());

            auto mesh = m_resourceData->meshes[get_handle_index(node.mesh)];
            for (const auto& surface : mesh.surfaces) {

                switch (const auto material = m_resourceData->materials[get_handle_index(surface.material)]; material.type) {
                    case MaterialType::opaque:
                        scene.opaqueRenderables.push_back({surface, node.localMatrix} ); break;
                    case MaterialType::transparentBlend:
                        scene.blendedRenderables.push_back({ surface, node.localMatrix } ); break;
                    case MaterialType::transparentMasked:
                        scene.maskedRenderables.push_back({ surface, node.localMatrix } ); break;
                }
            }
        }

        nodes.push_back(node);
        nodeMetadata.push_back(metadata);
    }
}

GeometricData SceneBuilder::create_meshes(const fastgltf::Asset &asset, Scene &scene) const {
    auto& meshes = m_resourceData->meshes;
    auto& meshMetadata = m_resourceData->meshMetadata;
    const auto numGltfMeshes = asset.meshes.size();
    meshes.reserve(numGltfMeshes + asset.meshes.size());
    meshMetadata.reserve(numGltfMeshes + asset.meshes.size());

    std::vector<u32> indices;
    std::vector<Vertex> vertices;

    for (const auto& gltfMesh : asset.meshes) {
        Mesh mesh;
        for (auto&& primitive : gltfMesh.primitives) {
            Surface newSurface;
            newSurface.initialIndex = indices.size();
            newSurface.indexCount = asset.accessors[primitive.indicesAccessor.value()].count;

            if (primitive.materialIndex.has_value())
                newSurface.material = static_cast<MaterialHandle>(primitive.materialIndex.value());


            u64 initialVertex = vertices.size();
            {
                auto& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<u32>(asset, indexAccessor,
                    [&](u32 idx) {
                    indices.push_back(idx + initialVertex);
                });
            }

            {
                auto& positionAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newVertex{};
                        newVertex.position = v;
                        newVertex.normal = { 1, 0, 0 };
                        newVertex.colour = glm::vec4 { 1.f };
                        newVertex.uvX = 0;
                        newVertex.uvY = 0;
                        vertices[initialVertex + index] = newVertex;
                });
            }

            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normals->accessorIndex],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVertex + index].normal = v;
                    });
            }

            auto uv = primitive.findAttribute("TEXCOORD_0");
            if (uv != primitive.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->accessorIndex],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initialVertex + index].uvX = v.x;
                        vertices[initialVertex + index].uvY = -v.y;
                    });
            }

            if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[colors->accessorIndex],
                [&](glm::vec4 v, u64 index) {
                    vertices[initialVertex + index].colour = v;
                });
            }

            glm::vec3 min = vertices[initialVertex].position;
            glm::vec3 max = vertices[initialVertex].position;
            for (u64 i = initialVertex; i < vertices.size(); i++) {
                min = glm::min(min, vertices[i].position);
                max = glm::max(max, vertices[i].position);
            }

            newSurface.boundingVolume = {min, max};
            mesh.surfaces.push_back(newSurface);
        }

        u16 metadata = meshes.size();
        const auto handle = static_cast<MeshHandle>(metadata << 16 | meshes.size());
        scene.meshes.push_back(handle);
        meshes.push_back(mesh);
        meshMetadata.push_back(metadata);
    }

    return {vertices, indices };
}

void SceneBuilder::create_materials(const fastgltf::Asset &asset, Scene &scene) const {
    auto& materials = m_resourceData->materials;
    auto& materialMetadata = m_resourceData->materialMetadata;
    const auto numGltfMaterials = materials.size();
    materials.reserve(numGltfMaterials + materials.size());

    for (const auto& gltfMaterial : asset.materials) {
        Material material;
        u16 metadata = materials.size();

        material.baseColorFactor = glm::vec4(
        gltfMaterial.pbrData.baseColorFactor.x(),
        gltfMaterial.pbrData.baseColorFactor.y(),
        gltfMaterial.pbrData.baseColorFactor.z(),
        gltfMaterial.pbrData.baseColorFactor.w()
        );

        if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
            auto textureIndex = gltfMaterial.pbrData.baseColorTexture.value().textureIndex;
            material.baseColorTexture = static_cast<TextureHandle>(textureIndex);
        }
        else {
            material.baseColorFactor = glm::vec4(1.0f);
        }

        if (gltfMaterial.normalTexture.has_value()) {
            auto textureIndex = gltfMaterial.normalTexture.value().textureIndex;
            material.normalTexture = static_cast<TextureHandle>(textureIndex);
        };

        if (gltfMaterial.occlusionTexture.has_value()) {
            auto textureIndex = gltfMaterial.occlusionTexture.value().textureIndex;
            material.occlusionTexture = static_cast<TextureHandle>(textureIndex);
        };

        if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
            auto textureIndex = gltfMaterial.pbrData.metallicRoughnessTexture.value().textureIndex;
            material.mrTexture = static_cast<TextureHandle>(textureIndex);
        }

        material.metalnessFactor = gltfMaterial.pbrData.metallicFactor;
        material.roughnessFactor = gltfMaterial.pbrData.roughnessFactor;


        if (gltfMaterial.emissiveTexture.has_value()) {
            auto textureIndex = gltfMaterial.emissiveTexture.value().textureIndex;
            material.emissiveTexture = static_cast<TextureHandle>(textureIndex);
        }

        switch (gltfMaterial.alphaMode) {
            case fastgltf::AlphaMode::Opaque:
                material.type = MaterialType::opaque;
                break;
            case fastgltf::AlphaMode::Blend:
                material.type = MaterialType::transparentBlend;
                break;
            case fastgltf::AlphaMode::Mask:
                material.type = MaterialType::transparentMasked;
                break;
            default:
                break;
        }

        material.emissiveStrength = gltfMaterial.emissiveStrength;
        materials.push_back(material);
        materialMetadata.push_back(metadata);

        const auto handle = static_cast<MaterialHandle>(metadata << 16 | materials.size());

        scene.materials.push_back(handle);
    }
}

inline vk::Filter convert_filter(const fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return vk::Filter::eNearest;

        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::Filter::eLinear;
    }
}

inline vk::SamplerMipmapMode convert_mipmap_mode(const fastgltf::Filter filter) {
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return vk::SamplerMipmapMode::eNearest;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

void SceneBuilder::create_samplers(const fastgltf::Asset& asset, Scene& scene) const {
    auto& samplers = m_resourceData->samplers;
    auto& samplerMetadata = m_resourceData->samplerMetadata;
    const auto numGltfSamplers = samplers.size();
    samplers.reserve(numGltfSamplers + samplers.size());

    samplers.reserve(asset.samplers.size());
    for (const auto& gltfSampler : asset.samplers) {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.magFilter = convert_filter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = convert_filter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.maxLod = vk::LodClampNone;
        samplerInfo.minLod = 0;
        samplerInfo.mipmapMode = convert_mipmap_mode(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));

        auto magFilter = convert_filter(gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest));
        const auto minFilter = convert_filter(gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest));
        const auto mipMapMode = convert_mipmap_mode(gltfSampler.minFilter.value_or(magFilter));

        Sampler sampler = m_context.create_sampler(magFilter, minFilter, mipMapMode);
        u16 metaData = samplers.size();

        samplers.push_back(sampler);
        samplerMetadata.push_back(metaData);

        const auto handle = static_cast<SamplerHandle>(metaData << 16 | samplers.size());
        scene.samplers.push_back(handle);
    }
}

void SceneBuilder::create_lights(const fastgltf::Asset &asset, Scene &scene) const {
    auto& pointLights = m_resourceData->pointLights;
    auto& pointLightMetadata = m_resourceData->pointLightMetaData;

    auto& spotLights = m_resourceData->spotLights;
    auto& spotLightMetadata = m_resourceData->spotLightMetadata;

    spotLights.reserve(MAX_SPOT_LIGHTS);
    pointLights.reserve(MAX_POINT_LIGHTS);

    for (const auto& gltfLight : asset.lights) {

        if (gltfLight.type == fastgltf::LightType::Point)
        {
            PointLight pointLight{};
            pointLight.colour = {gltfLight.color.x(), gltfLight.color.y(), gltfLight.color.z()};
            pointLight.intensity = gltfLight.intensity;
            if (gltfLight.range.has_value())
                pointLight.range = gltfLight.range.value();

            u16 metaData = pointLightMetadata.size();
            pointLightMetadata.push_back(metaData);
            pointLights.push_back(pointLight);

            const auto handle = static_cast<PointLightHandle>(pointLights.size() << 16 | pointLights.size());
            scene.pointLights.push_back(handle);
        }
        if (gltfLight.type == fastgltf::LightType::Spot)
        {
            SpotLight spotLight{};
            spotLight.colour = {gltfLight.color.x(), gltfLight.color.y(), gltfLight.color.z()};
            spotLight.intensity = gltfLight.intensity;
            if (gltfLight.outerConeAngle.has_value())
                spotLight.umbraAngle = gltfLight.outerConeAngle.value();
            if (gltfLight.innerConeAngle.has_value())
                spotLight.penumbraAngle = gltfLight.innerConeAngle.value();

            u16 metaData = pointLightMetadata.size();
            spotLightMetadata.push_back(metaData);
            spotLights.push_back(spotLight);

            const auto handle = static_cast<SpotLightHandle>(spotLights.size() << 16 | spotLights.size());
            scene.spotLights.push_back(handle);
        }
    }
}

void SceneBuilder::create_images(const std::span<ktxTexture*> ktxTexturePs, Scene &scene) const {

    auto& textures = m_resourceData->textures;
    auto& textureMetadata = m_resourceData->texturesMetadata;
    textures.reserve(ktxTexturePs.size());

    for (const auto ktxTextureP : ktxTexturePs) {
        Image texture = m_context.create_image(
            { ktxTextureP->baseWidth, ktxTextureP->baseHeight, 1 },
            VK_FORMAT_BC7_SRGB_BLOCK,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            ktxTextureP->numLevels,
            true
        );
        u16 metadata = textures.size();

        const auto handle = static_cast<TextureHandle>(metadata << 16 | textures.size());
        scene.textures.push_back(handle);

        textures.push_back(texture);
        textureMetadata.push_back(metadata);
    }
}

ktxTextureData SceneBuilder::ktx_texture_data_from_gltf(fastgltf::Asset &asset) {
    std::vector<ktxTexture*> texturePs;
    texturePs.reserve(asset.images.size());

    std::vector<u16> mipLevelsArray;
    mipLevelsArray.reserve(asset.images.size());

    std::vector<vk::BufferImageCopy> copyRegions;
    copyRegions.reserve(asset.images.size());

    u64 stagingBufferSize = 0;

    for (auto& [data, name] : asset.images) {
        std::visit(fastgltf::visitor {
            [&](auto& arg) {},
                [&](fastgltf::sources::URI& path) {
                    std::string prepend = "../assets/scenes/sponza/";
                    prepend.append(path.uri.c_str());

                    ktxTexture* texture;

                    const ktxResult result = ktxTexture_CreateFromNamedFile(
                        prepend.c_str(),
                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                        &texture
                        );

                    if (result != KTX_SUCCESS)
                        throw std::runtime_error("Failed to load KTX texture " + prepend);

                    texturePs.push_back(texture);
                    const ku32 mipLevels = texture->numLevels;
                    mipLevelsArray.push_back(mipLevels);

                    for (u32 i = 0; i < mipLevels; i++) {
                        ku64 imageOffset = stagingBufferSize;
                        ku64 mipOffset;
                        if (ktxTexture_GetImageOffset(texture, i, 0, 0, &mipOffset) == KTX_SUCCESS) {
                            vk::BufferImageCopy copyRegion;
                            ku64 offset = imageOffset + mipOffset;
                            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                            copyRegion.imageSubresource.mipLevel = i;
                            copyRegion.imageSubresource.baseArrayLayer = 0;
                            copyRegion.imageSubresource.layerCount = 1;
                            copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                            copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                            copyRegion.imageExtent.depth = 1;
                            copyRegion.bufferOffset = offset;

                            copyRegions.push_back(copyRegion);
                        }
                    }

                    stagingBufferSize += texture->dataSize;
            },

            [&](fastgltf::sources::Array& array) {

                ku8* ktxTextureData{};
                ku64 textureSize{};
                ktxTexture* texture;

                const ktxResult result = ktxTexture_CreateFromMemory(ktxTextureData, textureSize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

                if (result != KTX_SUCCESS)
                    throw std::runtime_error("Failed to load KTX texture");

                const ku32 mipLevels = texture->numLevels;
                mipLevelsArray.push_back(mipLevels);

                stagingBufferSize += textureSize;

                for (u32 i = 0; i < mipLevels; i++) {
                    ku64 offset;
                    if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                        vk::BufferImageCopy copyRegion;
                        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                        copyRegion.imageSubresource.mipLevel = i;
                        copyRegion.imageSubresource.baseArrayLayer = 0;
                        copyRegion.imageSubresource.layerCount = 1;
                        copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                        copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                        copyRegion.imageExtent.depth = 1;
                        copyRegion.bufferOffset = offset;

                        copyRegions.push_back(copyRegion);
                    }
                }
            },

            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor {
                        [&](auto& arg) {},
                        [&](fastgltf::sources::Array& array) {
                            const auto ktxTextureBytes = reinterpret_cast<const ku8*>(array.bytes.data() + bufferView.byteOffset);
                            const auto byteLength = static_cast<i32>(bufferView.byteLength);

                            ktxTexture* texture;
                            const ktxResult result = ktxTexture_CreateFromMemory(
                                ktxTextureBytes,
                                byteLength,
                                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                &texture
                                );

                            if (result != KTX_SUCCESS)
                                throw std::runtime_error("Failed to load KTX texture");

                            const ku32 mipLevels = texture->numLevels;
                            mipLevelsArray.push_back(mipLevels);

                            const ku32 textureSize = texture->dataSize;
                            stagingBufferSize += textureSize;

                            for (u32 i = 0; i < mipLevels; i++) {
                                ku64 offset;
                                if (ktxTexture_GetImageOffset(texture, i, 0, 0, &offset) == KTX_SUCCESS) {
                                    vk::BufferImageCopy copyRegion;
                                    copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                                    copyRegion.imageSubresource.mipLevel = i;
                                    copyRegion.imageSubresource.baseArrayLayer = 0;
                                    copyRegion.imageSubresource.layerCount = 1;
                                    copyRegion.imageExtent.width = std::max(1u, texture->baseWidth >> i);
                                    copyRegion.imageExtent.height = std::max(1u, texture->baseHeight >> i);
                                    copyRegion.imageExtent.depth = 1;
                                    copyRegion.bufferOffset = offset;

                                    copyRegions.push_back(copyRegion);
                                }
                            }
                        }
                    }, buffer.data);
            }
        }, data);
    }

    return {texturePs, copyRegions, stagingBufferSize};
}

void SceneBuilder::upload_scene_data(const GeometricData& geoData, ktxTextureData& ktxTextureData) const {
    const auto geoStaging = prep_vertex_index_staging(geoData);
    const auto imageStaging = prepare_image_staging(ktxTextureData);
    const auto materialBuffer = prepare_material_buffer();
    const auto pointLightBuffer = prepare_point_light_buffer();
    const auto spotLightBuffer = prepare_spot_light_buffer();

    const auto& [vertexBuffer, indexBuffer, vertexBufferSize, indexBufferSize] = prep_geo_buffers(geoData);

    auto& pointLights = m_resourceData->pointLights;
    auto& spotLights = m_resourceData->spotLights;
    auto& materials = m_resourceData->materials;
    auto& textures = m_resourceData->textures;
    auto& regions = ktxTextureData.copyRegions;

    CommandBuffer cmd;
    cmd.set_handle(m_context.get_immediate_info().immediateCommandBuffer);
    cmd.set_allocator(m_context.get_allocator());
    cmd.begin();
    cmd.upload_uniform(pointLights.data(), pointLights.size(), pointLightBuffer);
    cmd.upload_uniform(spotLights.data(), spotLights.size(), spotLightBuffer);
    cmd.upload_uniform(materials.data(), materials.size(), materialBuffer);
    cmd.copy_buffer(geoStaging, vertexBuffer, 0, 0, vertexBufferSize);
    cmd.copy_buffer(geoStaging, indexBuffer, vertexBufferSize, 0, indexBufferSize);

    u64 index = 0;
    for (auto& texture : textures) {
        auto begin = regions.begin() + index;
        auto end = begin + texture.mipLevels;
        cmd.image_barrier(texture.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        cmd.copy_buffer_to_image(
            imageStaging,
            texture,
            vk::ImageLayout::eTransferDstOptimal,
            std::span(begin, end)
            );
        cmd.image_barrier(texture.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        if (texture.mipLevels == 0) { index++; }
        else { index += texture.mipLevels; }
    }
    cmd.end();

    m_context.submit_upload_work();

    m_resourceData->indexBuffer = indexBuffer;
    m_resourceData->vertexBuffer = vertexBuffer;
    m_resourceData->materialBuffer = materialBuffer;
    m_resourceData->pointLightBuffer = pointLightBuffer;
    m_resourceData->spotLightBuffer = spotLightBuffer;
}

GeoBuffers SceneBuilder::prep_geo_buffers(const GeometricData &geoData) const {
    GeoBuffers geoBuffers;
    const auto& [vertices, indices] = geoData;
    constexpr vk::BufferUsageFlags vertexBufferFlags =
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eVertexBuffer;

    constexpr vk::BufferUsageFlags indexBufferFlags =
        vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eIndexBuffer;

    const u64 vertexBufferSize = vertices.size() * sizeof(Vertex);
    geoBuffers.vertexBufferSize = vertexBufferSize;
    const u64 indexBufferSize = indices.size() * sizeof(u32);
    geoBuffers.indexBufferSize = indexBufferSize;

    geoBuffers.vertexBuffer = m_context.create_buffer(vertexBufferSize, vertexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);
    geoBuffers.indexBuffer = m_context.create_buffer(indexBufferSize, indexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);

    return geoBuffers;
}

Buffer SceneBuilder::prep_vertex_index_staging(const GeometricData &geoData) const {
    auto& [vertices, indices] = geoData;
    const auto allocator = m_context.get_allocator();
    const u64 vertexBufferSize = geoData.vertices.size() * sizeof(Vertex);
    const u64 indexBufferSize = geoData.indices.size() * sizeof(u32);
    const u64 stagingSize = vertexBufferSize + indexBufferSize;
    const auto staging = m_context.create_staging_buffer(stagingSize);
    auto data = staging.p_get_mapped_data();

    vmaMapMemory(allocator, staging.allocation, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
    vmaUnmapMemory(allocator, staging.allocation);

    return staging;
}

Buffer SceneBuilder::prepare_image_staging(const ktxTextureData &textureData) const {
    auto& [texturePs, copyRegions, stagingSize] = textureData;
    const auto allocator = m_context.get_allocator();
    auto staging = m_context.create_staging_buffer(stagingSize);
    auto data = staging.p_get_mapped_data();

    std::vector<ku8> bytes;
    bytes.resize(stagingSize);

    auto ptr = bytes.data();
    for (const auto& texture : texturePs) {
        const ku64 size = texture->dataSize;
        const ku8* currentImageBytes = texture->pData;
        memcpy(ptr, currentImageBytes, size);
        ptr += size / sizeof(ku8);

        ktxTexture_Destroy(texture);
    }

    vmaMapMemory(allocator, staging.allocation, &data);
    memcpy(data, bytes.data(), stagingSize);
    vmaUnmapMemory(allocator, staging.allocation);

    return staging;
}

Buffer SceneBuilder::prepare_material_buffer() const {
    const auto& materials = m_resourceData->materials;
    const auto numMaterials = materials.size();
    const u64 materialBufferSize = sizeof(Material) * numMaterials;
    const auto materialBuffer = m_context.create_buffer(
            materialBufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    const auto data = static_cast<GPUMaterial*>(materialBuffer.p_get_mapped_data());

    for (u64 i = 0; i < numMaterials; i++) {
        GPUMaterial gpuMaterial;
        gpuMaterial.baseColorFactor = materials[i].baseColorFactor;
        gpuMaterial.roughnessFactor = materials[i].roughnessFactor;
        gpuMaterial.metalnessFactor = materials[i].metalnessFactor;
        gpuMaterial.baseColorTexture = get_handle_index(materials[i].baseColorTexture);
        gpuMaterial.mrTexture = get_handle_index(materials[i].mrTexture);
        gpuMaterial.occlusionTexture = get_handle_index(materials[i].occlusionTexture);
        gpuMaterial.normalTexture = get_handle_index(materials[i].normalTexture);
        gpuMaterial.emissiveTexture = get_handle_index(materials[i].emissiveTexture);
        data[i] = gpuMaterial;
    }

    return materialBuffer;
}

Buffer SceneBuilder::prepare_point_light_buffer() const
{
    constexpr u64 lightBufferSize = sizeof(PointLight) * MAX_POINT_LIGHTS;

    return m_context.create_buffer(
        lightBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

Buffer SceneBuilder::prepare_spot_light_buffer() const
{
    constexpr u64 lightBufferSize = sizeof(SpotLight) * MAX_SPOT_LIGHTS;

    return m_context.create_buffer(
        lightBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

u16 SceneBuilder::get_metadata_at_index(const u32 index) const {
    return m_resourceData->meshMetadata[index];
}


#include "gui.h"

#include <ranges>

ImGUIManager::ImGUIManager(Context &context, SceneManager& sceneManager) : m_Context(context), m_SceneManager(sceneManager) {
    init_gui_data();
}

void ImGUIManager::draw_imgui(const CommandBuffer& cmd, const vk::ImageView imageView, glm::mat4& view, glm::mat4& projection, const vk::Extent2D extent) {

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);

    ImGui::Begin("Scene Settings");
    ImGui::BeginChild("Light Settings");

    auto lightMetadata = select_light(lightState);
    process_light_data(cmd, lightMetadata, lightState);

    ImGui::EndChild();
    ImGui::BeginChild("Guizmo Settings");

    update_gizmo_data(view, projection, lightMetadata);

    ImGui::End();
    ImGui::Render();

    VkRenderingAttachmentInfo ImGUIDrawImage {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    ImGUIDrawImage.imageView= imageView;
    ImGUIDrawImage.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ImGUIDrawImage.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ImGUIDrawImage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = nullptr};
    vk::Rect2D renderArea{};
    renderArea.extent = extent;
    renderInfo.renderArea = renderArea;
    renderInfo.pColorAttachments = &ImGUIDrawImage;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;

    const auto cmdHandle = cmd.get_handle();
    vkCmdBeginRendering(cmdHandle, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdHandle);
    vkCmdEndRendering(cmdHandle);
}

LightMetaData ImGUIManager::select_light(LightGUIState& state) {

    auto& lightIndex = state.selectedLightIndex;
    const i64 combinedNumLights = state.numPointLights + state.numSpotLights;

    if (ImGui::BeginCombo("Selected Light", std::to_string(lightIndex).c_str(), ImGuiComboFlags_HeightLargest))
    {
        for (u64 i = 0; i < combinedNumLights; ++i)
        {
            if (ImGui::Selectable(std::to_string(i).c_str()))
            {
                lightIndex = i;
                lightsDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    switch (auto [selectedLightType, index] = state.lightTypes[lightIndex]; selectedLightType) {
        case LightType::Point:
            return {&state.pointLights[index], index, selectedLightType};
        case LightType::Spot:
            return {&state.spotLights[index], index, selectedLightType};
        default:
            return {};
    }
}

void ImGUIManager::process_light_data(const CommandBuffer& cmd, LightMetaData& metadata, LightGUIState &state) {
    light_creation_dialogue(cmd, state);

    auto [light, index, type] = metadata;

    if (ImGui::Button("Destroy Light")) {
        switch (type) {
            case LightType::Point:
                if (state.numPointLights > 0) {
                    m_SceneManager.remove_point_light(metadata.index);
                    state.lightTypes.erase(state.lightTypes.begin() + state.selectedLightIndex);
                    state.numPointLights--;
                    update_light_types(state.lightTypes);

                    lightsDirty = true;

                    if (state.selectedLightIndex > 0) {
                        state.selectedLightIndex--;
                    }
                }
                break;
            case LightType::Spot:
                if (state.numSpotLights > 0) {
                    m_SceneManager.remove_spot_light(metadata.index);
                    state.lightTypes.erase(state.lightTypes.begin() + state.selectedLightIndex);
                    state.numSpotLights--;
                    update_light_types(state.lightTypes);

                    lightsDirty = true;

                    if (state.selectedLightIndex > 0) {
                        state.selectedLightIndex--;
                    }
                }
                break;
            default:
                break;
        }
    }

    if (metadata.light.has_value()) {
        switch (type) {
            case LightType::Point:
                imgui_point_lights(std::get<PointLight*>(light.value()));
                break;
            case LightType::Spot:
                imgui_spot_lights(std::get<SpotLight*>(light.value()));
                break;
            default:
                break;
        }
    }

    if (lightsDirty) {
        m_SceneManager.update_light_buffer(cmd);
        state.numPointLights = m_SceneManager.get_num_point_lights();
        state.numSpotLights = m_SceneManager.get_num_spot_lights();

        lightsDirty = false;
    }
}

void ImGUIManager::imgui_point_lights(PointLight *light) {
    ImGui::BeginChild("Point Light");
    ImGui::Text("Point Light");

    if (ImGui::InputFloat3("Position", reinterpret_cast<f32*>(&light->position)))
        lightsDirty = true;

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<f32*>(&light->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float))
        lightsDirty = true;

    if (ImGui::DragFloat("Intensity", &light->intensity, 0.001f, 0.0f, 1.0f))
        lightsDirty = true;

    if (ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 100.0f))
        lightsDirty = true;

    ImGui::EndChild();
}

void ImGUIManager::imgui_spot_lights(SpotLight *light) {

    ImGui::BeginChild("Spot Light");
     ImGui::Text("Spot Light");

    if (ImGui::InputFloat3("Position", reinterpret_cast<f32*>(&light->position)))
        lightsDirty = true;

    if (ImGui::DragFloat3("Direction", reinterpret_cast<f32*>(&light->direction), 0.1f, -360.0f, 360.0f))
        lightsDirty = true;

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<f32*>(&light->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float))
        lightsDirty = true;

    if (ImGui::DragFloat("Intensity", &light->intensity, 0.001f, 0.0f, 1.0f))
        lightsDirty = true;

    if (ImGui::DragFloat("Range", &light->range, 0.1f, 0.0f, 100.0f))
        lightsDirty = true;

    if (ImGui::InputFloat("Penumbra Angle", &light->penumbraAngle))
        lightsDirty = true;

    if (ImGui::InputFloat("Umbra Angle", &light->umbraAngle))
        lightsDirty = true;

    ImGui::EndChild();
}


void ImGUIManager::update_gizmo_data(glm::mat4& view, glm::mat4& projection, const LightMetaData& metaData) {
    view[1][1] *= -1.0f;
    projection[1][1] *= -1.0f;

    static ImGuizmo::OPERATION op(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mode(ImGuizmo::WORLD);

    if (metaData.lightType == LightType::Point) {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) { op = ImGuizmo::TRANSLATE; }

        process_point_light_gizmo(std::get<PointLight*>(metaData.light.value()), op, mode, view, projection);
    }

    if (metaData.lightType == LightType::Spot) {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) { op = ImGuizmo::TRANSLATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { op = ImGuizmo::ROTATE; }

        process_spot_light_gizmo(std::get<SpotLight*>(metaData.light.value()), op, mode, view, projection);
    }
}

void ImGUIManager::process_point_light_gizmo(PointLight* pointLight, ImGuizmo::OPERATION op, const ImGuizmo::MODE mode, glm::mat4 &view, glm::mat4 &projection) {

    auto translation = &pointLight->position;
    auto rotation = glm::vec3(1.0f);
    auto scale = glm::vec3(1.0f);

    const auto pGizmo = reinterpret_cast<f32*>(&gizmoState.gizmoMatrix);
    const auto pTranslation = reinterpret_cast<f32*>(translation);
    const auto pRotation = reinterpret_cast<f32*>(&rotation);
    const auto pScale = reinterpret_cast<f32*>(&scale);
    const auto pView = reinterpret_cast<f32*>(&view);
    const auto pProjection = reinterpret_cast<f32*>(&projection);

    ImGuizmo::DecomposeMatrixToComponents(pGizmo, pTranslation, pRotation, pScale);
    if (ImGui::InputFloat3("Tr", pTranslation))
        lightsDirty = true;
    if (ImGui::InputFloat3("Rt", pRotation))
        lightsDirty = true;
    ImGuizmo::RecomposeMatrixFromComponents(pTranslation, pRotation, pScale, pGizmo);

    static bool useSnap = false;
    if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyPressed(ImGuiKey_LeftCtrl))
        useSnap = !useSnap;
    ImGui::Checkbox("##useSnap", &useSnap);
    ImGui::SameLine();

    const auto snapP = &gizmoState.snap[0];
    ImGui::InputFloat3("Snap", snapP);

    const auto io = m_Context.get_imgui_io();
    ImGuizmo::SetRect(0, 0, io->DisplaySize.x, io->DisplaySize.y);
    if (ImGuizmo::Manipulate(pView, pProjection, op, mode, pGizmo, nullptr, useSnap ? snapP : nullptr)) {
        lightsDirty = true;
    }

    if (ImGuizmo::IsUsing()) {
        ImGui::Text("Using gizmo");
    }
    else {
        ImGui::Text(ImGuizmo::IsOver()?"Over gizmo":"");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Over translate gizmo" : "");
    }
    ImGui::Separator();
    ImGui::EndChild();
}

void ImGUIManager::process_spot_light_gizmo(SpotLight* spotLight, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, glm::mat4 &view, glm::mat4 &projection) {

    auto translation = &spotLight->position;
    auto rotation = &spotLight->direction;
    auto scale = glm::vec3(1.0f);

    const auto pGizmo = reinterpret_cast<f32*>(&gizmoState.gizmoMatrix);
    const auto pTranslation = reinterpret_cast<f32*>(translation);
    const auto pRotation = reinterpret_cast<f32*>(&rotation);
    const auto pScale = reinterpret_cast<f32*>(&scale);
    const auto pView = reinterpret_cast<f32*>(&view);
    const auto pProjection = reinterpret_cast<f32*>(&projection);

    ImGuizmo::DecomposeMatrixToComponents(pGizmo, pTranslation, pRotation, pScale);
    if (ImGui::InputFloat3("Tr", pTranslation))
        lightsDirty = true;
    if (ImGui::InputFloat3("Rt", pRotation))
        lightsDirty = true;
    ImGuizmo::RecomposeMatrixFromComponents(pTranslation, pRotation, pScale, pGizmo);

    static bool useSnap = false;
    if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyPressed(ImGuiKey_LeftCtrl))
        useSnap = !useSnap;
    ImGui::Checkbox("##useSnap", &useSnap);
    ImGui::SameLine();

    const auto snapP = &gizmoState.snap[0];
    ImGui::InputFloat3("Snap", snapP);


    const auto io = m_Context.get_imgui_io();
    ImGuizmo::SetRect(0, 0, io->DisplaySize.x, io->DisplaySize.y);
    if (ImGuizmo::Manipulate(pView, pProjection, op, mode, pGizmo, nullptr, useSnap ? snapP : nullptr)) {
        lightsDirty = true;
    }

    if (ImGuizmo::IsUsing()) {
        ImGui::Text("Using gizmo");
    }
    else {
        ImGui::Text(ImGuizmo::IsOver()?"Over gizmo":"");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Over translate gizmo" : "");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::ROTATE) ? "Over rotate gizmo" : "");
    }
    ImGui::Separator();
    ImGui::EndChild();
}

void ImGUIManager::light_creation_dialogue(const CommandBuffer& cmd, LightGUIState& state) {
    ImGui::Text("Light Creation");
    ImGui::Text("Point Light");
    ImGui::SameLine();
    ImGui::Text("Spot Light");
    if (ImGui::Button("Create Point Light")) {
        constexpr PointLight newPointLight {{0.0f, 0.0f, 0.0f},{0.3f, 5.0f, 2.0f}, 0.5f, 3.0f};
        m_SceneManager.add_point_light(newPointLight);
        state.pointLights = m_SceneManager.get_all_point_lights_p();
        state.lightTypes.emplace_back(LightType::Point, state.lightTypes.size());
        update_light_types(state.lightTypes);

        lightsDirty = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Create Spot Light")) {
        constexpr SpotLight newSpotLight {{ 0.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f }, {0.3f, 5.0f, 2.0f}, 0.5f, 1.0f, 0.1f};
        m_SceneManager.add_spot_light(newSpotLight);
        state.spotLights = m_SceneManager.get_all_spot_lights_p();
        state.lightTypes.emplace_back(LightType::Spot, state.lightTypes.size());
        update_light_types(state.lightTypes);

        lightsDirty = true;
    }
}

void ImGUIManager::update_light_types(std::vector<std::pair<LightType, i64>>& lightTypes) {
    i64 pointIndex = 0;
    i64 spotIndex = 0;
    for (auto& [type, index] : lightTypes) {
        switch (type) {
            case LightType::Point:
                index = pointIndex;
                pointIndex++;
                break;
            case LightType::Spot:
                index = spotIndex;
                spotIndex++;
                break;
            default:
                break;
        }
    }
}

void ImGUIManager::init_gui_data() {
    lightState.pointLights = m_SceneManager.get_all_point_lights_p();
    lightState.spotLights = m_SceneManager.get_all_spot_lights_p();
    lightState.numPointLights = static_cast<i32>(m_SceneManager.get_num_point_lights());
    lightState.numSpotLights = static_cast<i32>(m_SceneManager.get_num_spot_lights());

    lightState.lightTypes.reserve(lightState.numPointLights + lightState.numSpotLights);

    for (u64 i = 0; i < lightState.numPointLights; i++)
        lightState.lightTypes.emplace_back(LightType::Point, i);

    for (u64 i = 0; i < lightState.numSpotLights; i++)
        lightState.lightTypes.emplace_back(LightType::Spot, i);

    gizmoState.gizmoMatrix = glm::mat4(1.0f);
}


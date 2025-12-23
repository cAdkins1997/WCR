
#include "gui.h"

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

    imgui_point_lights(cmd);
    imgui_spot_lights(cmd);

    ImGui::EndChild();

    ImGui::BeginChild("Guizmo Settings");

    constexpr glm::mat4 mat(1.0f);
    update_gui_data(view, projection);

    if (ImGuizmo::IsUsing())
    {
        ImGui::Text("Using gizmo");
    }
    else
    {
        ImGui::Text(ImGuizmo::IsOver()?"Over gizmo":"");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Over translate gizmo" : "");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::ROTATE) ? "Over rotate gizmo" : "");
        ImGui::SameLine();
        ImGui::Text(ImGuizmo::IsOver(ImGuizmo::SCALE) ? "Over scale gizmo" : "");
    }
    ImGui::Separator();
    ImGui::EndChild();

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

void ImGUIManager::update_gui_data(glm::mat4& view, glm::mat4& projection) {
    view[1][1] *= -1.0f;
    projection[1][1] *= -1.0f;

    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
    if (ImGui::IsKeyPressed(ImGuiKey_T))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;

    auto& pointLight = imguiVariables.pointLights[imguiVariables.selectedPointLight];
    const auto pGizmo = reinterpret_cast<f32*>(&imguiVariables.gizmoConfig.gizmoMatrix);

    auto* translation = &pointLight.position;
    auto rotation = glm::vec3(1.0f);
    auto scale = glm::vec3(1.0f);

    const auto pTranslation = (f32*)translation;
    const auto pRotation = reinterpret_cast<f32*>(&rotation);
    const auto pScale = reinterpret_cast<f32*>(&scale);
    const auto pView = reinterpret_cast<f32*>(&view);
    const auto pProjection = reinterpret_cast<f32*>(&projection);

    ImGuizmo::DecomposeMatrixToComponents(pGizmo, pTranslation, pRotation, pScale);
    ImGui::InputFloat3("Tr", pTranslation);
    ImGui::InputFloat3("Rt", pRotation);
    ImGui::InputFloat3("Sc", pScale);
    ImGuizmo::RecomposeMatrixFromComponents(pTranslation, pRotation, pScale, pGizmo);

    if (mCurrentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
    }

    static bool useSnap(false);
    if (ImGui::IsKeyPressed(ImGuiKey_S))
        useSnap = !useSnap;
    ImGui::Checkbox("##useSnap", &useSnap);
    ImGui::SameLine();

    const auto pConfig = &imguiVariables.gizmoConfig;
    const auto snapP = &pConfig->snap[0];
    switch (mCurrentGizmoOperation)
    {
        case ImGuizmo::TRANSLATE:
            ImGui::InputFloat3("Snap", snapP);
            break;
        case ImGuizmo::ROTATE:
            ImGui::InputFloat("Angle Snap", snapP);
            break;
        case ImGuizmo::SCALE:
            ImGui::InputFloat("Scale Snap", snapP);
            break;
        default:
            break;
    }

    const auto io = m_Context.get_imgui_io();
    ImGuizmo::SetRect(0, 0, io->DisplaySize.x, io->DisplaySize.y);
    if (ImGuizmo::Manipulate(pView, pProjection, mCurrentGizmoOperation, mCurrentGizmoMode, pGizmo, nullptr, useSnap ? snapP : nullptr)) {
        auto newTranslation = xyz(glm::vec4(translation->x, translation->y, translation->z, 0.0f) * imguiVariables.gizmoConfig.gizmoMatrix);
        translation = &newTranslation;
        imguiVariables.lightsDirty = true;
    }
}

void ImGUIManager::imgui_point_lights(const CommandBuffer &cmd) {
    ImGui::BeginChild("Point Lights");
    ImGui::Text("Point Lights");
    if (ImGui::Button("Create Point Light"))
    {
        constexpr PointLight newPointLight {{0.0f, 2.0f, 0.0f},{0.3f, 5.0f, 2.0f}, 0.5f, 3.0f};
        m_SceneManager.add_point_light(newPointLight);
        imguiVariables.numPointLights = m_SceneManager.get_num_point_lights();
        imguiVariables.lightsDirty = true;
        imguiVariables.selectedPointLight = imguiVariables.numPointLights - 1;
    }
    if (ImGui::Button("Destroy Point Light"))
    {
        if (imguiVariables.numPointLights > 0)
        {
            m_SceneManager.remove_point_light(imguiVariables.selectedPointLight);
            imguiVariables.numPointLights = m_SceneManager.get_num_point_lights();
            imguiVariables.lightsDirty = true;
            if (imguiVariables.selectedPointLight > 0)
            {
                imguiVariables.selectedPointLight--;
            }
        }
    }

    if (ImGui::BeginCombo("Selected Point Light", std::to_string(imguiVariables.selectedPointLight).c_str(), ImGuiComboFlags_HeightLargest))
    {
        for (u64 i = 0; i < imguiVariables.numPointLights; ++i)
        {
            if (ImGui::Selectable(std::to_string(i).c_str()))
            {
                imguiVariables.selectedPointLight = i;
                imguiVariables.lightsDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    PointLight* currentPointLight = &imguiVariables.pointLights[imguiVariables.selectedPointLight];

    if (ImGui::InputFloat3("Position", reinterpret_cast<f32*>(&currentPointLight->position)))
        imguiVariables.lightsDirty = true;

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<f32*>(&currentPointLight->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat("Intensity", &currentPointLight->intensity, 0.001f, 0.0f, 1.0f))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat("Range", &currentPointLight->range, 0.1f, 0.0f, 100.0f))
        imguiVariables.lightsDirty = true;

    if (imguiVariables.lightsDirty) {
        m_SceneManager.update_light_buffer(cmd);
        imguiVariables.lightsDirty = false;
    }

    ImGui::EndChild();
}

void ImGUIManager::imgui_spot_lights(const CommandBuffer &cmd) {
        ImGui::BeginChild("Spot Lights");
    ImGui::Text("Spot Lights");

    if (ImGui::Button("Create Point Light"))
    {
        constexpr SpotLight newPointLight {{ 0.0f, 3.0f, 0.0f },{ 0.0f, -1.0f, 0.0f }, {0.3f, 5.0f, 2.0f}, 0.5f, 1.0f, 0.1f};
        m_SceneManager.add_spot_light(newPointLight);
        imguiVariables.numPointLights = m_SceneManager.get_num_point_lights();
        imguiVariables.lightsDirty = true;
        imguiVariables.selectedPointLight = imguiVariables.numPointLights - 1;
    }
    if (ImGui::Button("Destroy Point Light"))
    {
        if (imguiVariables.numPointLights > 0)
        {
            m_SceneManager.remove_point_light(imguiVariables.selectedPointLight);
            imguiVariables.numPointLights = m_SceneManager.get_num_point_lights();
            imguiVariables.lightsDirty = true;
            if (imguiVariables.selectedPointLight > 0)
            {
                imguiVariables.selectedPointLight--;
            }
        }
    }

    if (ImGui::BeginCombo("Selected Spot Light", "Choose a light", ImGuiComboFlags_HeightLargest))
    {
        for (u64 i = 0; i < imguiVariables.numSpotLights; ++i)
        {
            if (ImGui::Selectable(std::to_string(i).c_str()))
            {
                imguiVariables.selectedSpotLight = i;
                imguiVariables.lightsDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    SpotLight* currentSpotLight = &imguiVariables.spotLights[imguiVariables.selectedSpotLight];

    if (ImGui::InputFloat3("Position", reinterpret_cast<f32*>(&currentSpotLight->position)))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat3("Direction", reinterpret_cast<f32*>(&currentSpotLight->direction), 0.1f, -360.0f, 360.0f))
        imguiVariables.lightsDirty = true;

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<f32*>(&currentSpotLight->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat("Intensity", &currentSpotLight->intensity, 0.001f, 0.0f, 1.0f))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat("Range", &currentSpotLight->range, 0.1f, 0.0f, 10.0f))
        imguiVariables.lightsDirty = true;

    if (ImGui::InputFloat("Penumbra Angle", &currentSpotLight->penumbraAngle))
        imguiVariables.lightsDirty = true;

    if (ImGui::InputFloat("Umbra Angle", &currentSpotLight->umbraAngle))
        imguiVariables.lightsDirty = true;

    if (imguiVariables.lightsDirty) {
        m_SceneManager.update_light_buffer(cmd);
        imguiVariables.lightsDirty = false;
    }

    ImGui::EndChild();
}

void ImGUIManager::init_gui_data() {
    imguiVariables.pointLights = m_SceneManager.get_all_point_lights_p();
    imguiVariables.spotLights = m_SceneManager.get_all_spot_lights_p();
    imguiVariables.numPointLights = static_cast<i32>(m_SceneManager.get_num_point_lights());
    imguiVariables.numSpotLights = static_cast<i32>(m_SceneManager.get_num_spot_lights());
}

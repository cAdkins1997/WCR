#pragma once
#include "glmdefines.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>
#include "scenes/scenemanager.h"

struct LightGUIState {
    std::vector<std::pair<LightType, i64>> lightTypes;
    i64 selectedLightIndex = 0;
    i64 numPointLights = 0;
    i64 numSpotLights = 0;
    PointLight* pointLights = nullptr;
    SpotLight* spotLights = nullptr;
};

struct LightMetaData {
    std::optional<std::variant<PointLight*, SpotLight*>> light;
    i64 index = 0;
    LightType lightType;
};

struct GizmoState {
    glm::mat4 gizmoMatrix;
    glm::vec3 snap;
};

class ImGUIManager {
public:
    explicit ImGUIManager(Context& context, SceneManager& sceneManager);
    void draw_imgui(const CommandBuffer& cmd, vk::ImageView imageView, glm::mat4& view, glm::mat4& projection, vk::Extent2D extent);
    LightMetaData select_light(LightGUIState& state);
    void update_gizmo_data(glm::mat4& view, glm::mat4& projection, const LightMetaData& metaData);
    void process_point_light_gizmo(PointLight* pointLight, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, glm::mat4& view, glm::mat4& projection);
    void process_spot_light_gizmo(SpotLight* spotLight, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, glm::mat4& view, glm::mat4& projection);
    void light_creation_dialogue(const CommandBuffer& cmd, LightGUIState& state);
    void update_light_types(std::vector<std::pair<LightType, i64>>& lightTypes);

private:
    void process_light_data(const CommandBuffer& cmd, LightMetaData& metadata, LightGUIState& state);
    void imgui_point_lights(PointLight *light);
    void imgui_spot_lights(SpotLight *light);
    void init_gui_data();

    GizmoState gizmoState;
    LightGUIState lightState;

    Context& m_Context;
    SceneManager& m_SceneManager;
    bool lightsDirty = false;
};
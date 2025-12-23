#pragma once
#include "glmdefines.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <ImGuizmo.h>
#include "scenes/scenemanager.h"

struct GizmoConfig {
    glm::mat4 gizmoMatrix;
    glm::vec3 snap;
};

struct ImGUIVariables {
    GizmoConfig gizmoConfig{};
    i64 selectedPointLight = 0;
    i64 selectedSpotLight = 0;
    i64 numPointLights = 0;
    i64 numSpotLights = 0;
    PointLight* pointLights = nullptr;
    SpotLight* spotLights = nullptr;
    bool lightsDirty = false;
};

class ImGUIManager {
public:
    explicit ImGUIManager(Context& context, SceneManager& sceneManager);
    void draw_imgui(const CommandBuffer& cmd, vk::ImageView imageView, glm::mat4& view, glm::mat4& projection, vk::Extent2D extent);
    void update_gui_data(glm::mat4& view, glm::mat4& projection);

private:
    void imgui_point_lights(const CommandBuffer& cmd);
    void imgui_spot_lights(const CommandBuffer& cmd);
    void init_gui_data();

    Context& m_Context;
    SceneManager& m_SceneManager;
    ImGUIVariables imguiVariables;
};
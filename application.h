#pragma once

#include "device/context.h"
#include "pipelines/pipelines.h"
#include "pipelines/descriptors.h"
#include "scenes/scenemanager.h"
#include "camera.h"


void mouse_callback(GLFWwindow* window, f64 xPosIn, f64 yPosIn);
void process_scroll(GLFWwindow* window, f64 xOffset, f64 yOffset);
void process_input(GLFWwindow* window, f32 deltaTime, bool& mouseLook);

inline auto camera = Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), -90.0f, 0.0f);

struct ImGUIVariables {
    i32 selectedLight = 0;
    i32 numLights = 0;
    Light* lights;
    char* lightNames = nullptr;
    bool lightsDirty = false;
};

class Application {
public:
    Application(std::string_view appName, u32 width, u32 height);
    ~Application();

    void draw();
    void draw_imgui(const CommandBuffer& cmd, vk::ImageView view, vk::Extent2D extent);
    void run();
    void update();
    void init();
    void init_opaque_pipeline();
    void init_descriptors();
    void init_scene_data();
    void init_gui_data();

private:
    std::unique_ptr<Context> context;
    std::unique_ptr<DescriptorBuilder> descriptorBuilder;
    std::shared_ptr<ResourceData> resourceData;
    std::unique_ptr<SceneBuilder> sceneBuilder;
    std::unique_ptr<SceneManager> sceneManager;
    SceneHandle testScene{};
    Pipeline opaquePipeline;
    VkRenderingAttachmentInfo drawAttachment;
    VkRenderingAttachmentInfo depthAttachment;
    ImGUIVariables imguiVariables;

};

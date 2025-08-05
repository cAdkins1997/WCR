#pragma once

#include "device/context.h"
#include "glmdefines.h"

#include <glm/glm.hpp>

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cameraPosition;
};

class Application {
public:
    Application(std::string_view appName, uint32_t width, uint32_t height);

    void draw();
    void run();
    void update();
    void init();

private:
    SceneData sceneData;
    std::unique_ptr<Context> context;
};

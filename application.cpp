
#include "application.h"

void mouse_callback(GLFWwindow *window, f64 xPosIn, f64 yPosIn) {
    const auto xPos = static_cast<float>(xPosIn);
    const auto yPos = static_cast<float>(yPosIn);

    if (firstMouse)
    {
        lastX = xPos;
        lastY = yPos;
        firstMouse = false;
    }

    float xOffset = xPos - lastX;
    float yOffset = lastY - yPos;

    lastX = xPos;
    lastY = yPos;

    camera.process_mouse_movement(xOffset, yOffset, false);
}

void process_scroll(GLFWwindow *window, f64 xOffset, f64 yOffset) {
    camera.process_mouse_scroll(static_cast<float>(yOffset));
}

void process_input(GLFWwindow *window, const f32 deltaTime, u32 &inputDelay, bool &mouseLook) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.process_keyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.process_keyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.process_keyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.process_keyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        camera.process_keyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        camera.process_keyboard(DOWN, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && inputDelay == 0) {
        if (mouseLook) {
            mouseLook = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
            inputDelay += 240;
        }
        else {
            mouseLook = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            inputDelay += 240;
        }
    }

    if (inputDelay > 0) inputDelay--;
}

Application::Application(std::string_view appName, u32 width, u32 height)
{
    context = std::make_unique<Context>(appName, width, height);
    resourceData = std::make_shared<ResourceData>();
    descriptorBuilder = std::make_unique<DescriptorBuilder>(context->get_device());
    sceneBuilder = std::make_unique<SceneBuilder>(*context, resourceData);
    sceneManager = std::make_unique<SceneManager>(resourceData);

    const auto windowP = context->p_get_window();
    glfwSetCursorPosCallback(windowP, mouse_callback);
    glfwSetScrollCallback(windowP,  process_scroll);
    glfwSetInputMode(windowP, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(windowP, &context->get_device());

    context->init_imgui();

    init();
    run();
}

Application::~Application() {
    const auto deviceHandle = context->get_device_handle();
    auto allocator = context->get_allocator();
    vkDeviceWaitIdle(context->get_device_handle());
    sceneManager->release_gpu_resources(*context);
    descriptorBuilder->release_descriptor_resources();
    deviceHandle.destroyPipeline(opaquePipeline.pipeline);
    deviceHandle.destroyPipelineLayout(opaquePipeline.pipelineLayout);
    deviceHandle.destroyDescriptorSetLayout(opaquePipeline.setLayout);
}

void Application::draw()
{
    const auto currentFrameTime = static_cast<f32>(glfwGetTime());
    deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    process_input(context->p_get_window(), deltaTime, camera.inputDelay, camera.enableMouseLook);
    SceneData sceneData{};
    sceneData.view = camera.get_view_matrix();
    sceneData.projection = glm::perspective(
        glm::radians(camera.zoom),
        static_cast<float>(context->get_display_extent().width) / static_cast<float>(context->get_display_extent().height),
        10000.f,
        0.1f
        );
    sceneData.cameraPosition = camera.Position;


    context->frame_submit([&](FrameInFlight& cmd, const SwapchainImageData& swapchainData) {
        auto& commandBuffer = cmd.commandBuffer;
        const auto& drawImage = context->get_draw_image();
        const auto& depthImage = context->get_depth_image();
        auto& currentSwapchainImage = swapchainData.swapchainImage;
        const auto displayExtent = context->get_display_extent();
        const auto drawAttachment = context->get_draw_attachment();
        const auto depthAttachment = context->get_depth_attachment();

        descriptorBuilder->write_buffer(cmd.SceneData.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
        descriptorBuilder->update_set(opaquePipeline.set);

        commandBuffer.begin();
        commandBuffer.update_uniform(&sceneData, sizeof(SceneData), cmd.SceneData);

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        commandBuffer.image_barrier(depthImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

        commandBuffer.set_up_render_pass(displayExtent, &drawAttachment, &depthAttachment);
        commandBuffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline);
        commandBuffer.set_viewport(displayExtent, 0.0f, 1.0f);
        commandBuffer.set_scissor(displayExtent);

        sceneManager->draw_scene(commandBuffer, testScene, sceneData.projection * sceneData.view);

        commandBuffer.end_render_pass();

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        commandBuffer.blit_image(drawImage.handle, currentSwapchainImage, to_extent_3D(displayExtent), to_extent_3D(displayExtent));
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
        draw_imgui(cmd.commandBuffer, swapchainData.swapchainImageView, displayExtent);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
        commandBuffer.end();

        context->submit_work(
            commandBuffer,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eAllGraphics,
            cmd.acquiredSemaphore,
            swapchainData.renderEndSemaphore,
            cmd.renderFence);
    });
}

void Application::draw_imgui(const CommandBuffer &cmd, const vk::ImageView view, const vk::Extent2D extent) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Scene Settings");

    ImGui::BeginChild("Light Settings");

    imgui_point_lights(cmd);
    imgui_spot_lights(cmd);

    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();

    VkRenderingAttachmentInfo ImGUIDrawImage {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    ImGUIDrawImage.imageView= view;
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

void Application::draw_gizmos()
{
    static u32 leftPress = 0, rightPress = 0;
    f64 x, y;
    const auto window = context->p_get_window();
    glfwGetCursorPos(window, &x, &y);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != leftPress)
    {
        leftPress = leftPress == GLFW_PRESS ? GLFW_RELEASE : GLFW_PRESS;
        track.mouse(vg::evLeftButton, get_vgizmo_key_mod(), leftPress, x, y);
    }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != rightPress) { // same thing for rightButton
        rightPress = rightPress == GLFW_PRESS ? GLFW_RELEASE : GLFW_PRESS;
        track.mouse(vg::evRightButton,get_vgizmo_key_mod(), rightPress, x, y);
    }

    track.motion(x, y);
    track.idle();
}

void Application::imgui_point_lights(const CommandBuffer& cmd)
{
    ImGui::BeginChild("Point Lights");
    ImGui::Text("Point Lights");
    if (ImGui::Button("Create Point Light"))
    {
        constexpr PointLight newPointLight {{0.0f, 2.0f, 0.0f},{0.3f, 5.0f, 2.0f}, 0.5f, 3.0f};
        sceneManager->add_point_light(newPointLight);
        imguiVariables.numPointLights = sceneManager->get_num_point_lights();
        imguiVariables.lightsDirty = true;
        imguiVariables.selectedPointLight = imguiVariables.numPointLights - 1;
    }
    if (ImGui::Button("Destroy Point Light"))
    {
        if (imguiVariables.numPointLights > 0)
        {
            sceneManager->remove_point_light(imguiVariables.selectedPointLight);
            imguiVariables.numPointLights = sceneManager->get_num_point_lights();
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
        sceneManager->update_light_buffer(cmd);
        imguiVariables.lightsDirty = false;
    }

    ImGui::EndChild();
}

void Application::imgui_spot_lights(const CommandBuffer& cmd)
{
    ImGui::BeginChild("Spot Lights");
    ImGui::Text("Spot Lights");

    if (ImGui::Button("Create Point Light"))
    {
        constexpr SpotLight newPointLight {{ 0.0f, 3.0f, 0.0f },{ 0.0f, -1.0f, 0.0f }, {0.3f, 5.0f, 2.0f}, 0.5f, 1.0f, 0.1f};
        sceneManager->add_spot_light(newPointLight);
        imguiVariables.numPointLights = sceneManager->get_num_point_lights();
        imguiVariables.lightsDirty = true;
        imguiVariables.selectedPointLight = imguiVariables.numPointLights - 1;
    }
    if (ImGui::Button("Destroy Point Light"))
    {
        if (imguiVariables.numPointLights > 0)
        {
            sceneManager->remove_point_light(imguiVariables.selectedPointLight);
            imguiVariables.numPointLights = sceneManager->get_num_point_lights();
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
        sceneManager->update_light_buffer(cmd);
        imguiVariables.lightsDirty = false;
    }

    ImGui::EndChild();
}

void Application::run()
{
    while (!glfwWindowShouldClose(context->p_get_window()))
    {
        glfwPollEvents();
        update();
        draw();
    }
}

void Application::update()
{
    sceneManager->update_nodes(glm::mat4(1.0f), testScene);
}

void Application::init()
{
    init_scene_data();
    init_descriptors();
    init_opaque_pipeline();
    init_gui_data();
}

void Application::init_opaque_pipeline() {
    const Shader vertShader = context->create_shader("../shaders/bin/slang/vertex.slang.spv");
    const Shader fragShader = context->create_shader("../shaders/bin/slang/pbr.slang.spv");

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout = opaquePipeline.pipelineLayout;
    pipelineBuilder.set_shader(vertShader.module, fragShader.module);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);

    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(vk::True, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(context->get_draw_image().format);
    pipelineBuilder.set_depth_format(context->get_depth_image().format);
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(context->get_device());

    context->destroy_shader(vertShader);
    context->destroy_shader(fragShader);
}

void Application::init_descriptors() {
    descriptorBuilder = std::make_unique<DescriptorBuilder>(context->get_device());
    auto globalSet = descriptorBuilder->build(opaquePipeline.setLayout);
    opaquePipeline.set = globalSet;

    sceneBuilder->write_textures(*descriptorBuilder);
    descriptorBuilder->update_set(opaquePipeline.set);

    vk::PushConstantRange pcRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants));
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &opaquePipeline.setLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pcRange;

    const auto globalPipelineLayout = context->get_device_handle().createPipelineLayout(pipelineLayoutInfo, nullptr);
    opaquePipeline.pipelineLayout = globalPipelineLayout;
}

void Application::init_scene_data() {
    auto gltf = sceneBuilder->parse_gltf("../assets/scenes/sponza/NewSponza_Main_glTF_003.gltf");
    if (gltf.has_value())
        if (const auto scene = sceneBuilder->build_scene(gltf.value()); scene.has_value())
            testScene = scene.value();
}

void Application::init_gui_data() {
    imguiVariables.pointLights = sceneManager->get_all_point_lights_p();
    imguiVariables.spotLights = sceneManager->get_all_spot_lights_p();
    imguiVariables.numPointLights = static_cast<i32>(sceneManager->get_num_point_lights());
    imguiVariables.numSpotLights = static_cast<i32>(sceneManager->get_num_spot_lights());
}

void Application::init_vgizmo_3d()
{
    track.setGizmoRotControl(vg::evButton1, 0);
    track.setGizmoRotXControl(vg::evButton1,vg::evShiftModifier);
    track.setGizmoRotYControl(vg::evButton1,vg::evControlModifier);
    track.setGizmoRotZControl(vg::evButton1,vg::evAltModifier | vg::evSuperModifier);
    track.setGizmoSecondaryRotControl(vg::evButton2, 0);
    track.setDollyControl(vg::evButton2, vg::evControlModifier);
    track.setPanControl(vg::evButton2, vg::evShiftModifier);

    const auto extent = context->get_display_extent();
    track.viewportSize(extent.width, extent.height);
}

u32 Application::get_vgizmo_key_mod()
{
    const auto window = context->p_get_window();
    if((glfwGetKey(window,GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || (glfwGetKey(window,GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        return vg::evControlModifier;
    if((glfwGetKey(window,GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(window,GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS))
        return vg::evShiftModifier;
    if((glfwGetKey(window,GLFW_KEY_LEFT_ALT) == GLFW_PRESS) || (glfwGetKey(window,GLFW_KEY_RIGHT_ALT) == GLFW_PRESS))
        return vg::evAltModifier;
    if((glfwGetKey(window,GLFW_KEY_LEFT_SUPER) == GLFW_PRESS) || (glfwGetKey(window,GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS))
        return vg::evSuperModifier;
    return vg::evNoModifier;
}

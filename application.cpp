
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

    /*if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&  inputDelay == 0) {
        if (mouseLook) {
            mouseLook = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
            inputDelay += 360;
        }
        else {
            mouseLook = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            inputDelay += 360;
        }
    }*/

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
    /*glfwMakeContextCurrent(windowP);
    glfwSetCursorPosCallback(windowP, mouse_callback);
    glfwSetScrollCallback(windowP,  process_scroll);
    glfwSetInputMode(windowP, GLFW_CURSOR, GLFW_CURSOR_NORMAL);*/

    init();
    run();
}

Application::~Application() {
    auto deviceHandle = context->get_device_handle();
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
        static_cast<float>(context->get_draw_image().extent.width) / static_cast<float>(context->get_draw_image().extent.height),
        10000.f,
        0.1f
        );
    sceneData.cameraPosition = glm::vec3(camera.Position.x, camera.Position.y, camera.Position.z);


    context->frame_submit([&](CommandBufferInfo& cmd, const SwapchainImageData& swapchainData) {
        auto& commandBuffer = cmd.commandBuffer;
        const auto& drawImage = context->get_draw_image();
        const auto& depthImage = context->get_depth_image();
        auto& currentSwapchainImage = swapchainData.swapchainImage;
        auto extent = drawImage.extent;
        auto displayExtent = context->get_display_extent();

        descriptorBuilder->write_buffer(cmd.SceneData.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
        descriptorBuilder->update_set(opaquePipeline.set);

        commandBuffer.begin();
        commandBuffer.update_uniform(&sceneData, sizeof(SceneData), cmd.SceneData);

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        commandBuffer.image_barrier(depthImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

        commandBuffer.set_up_render_pass({extent.width, extent.height}, &drawAttachment, &depthAttachment);
        commandBuffer.bind_pipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline);
        commandBuffer.set_viewport({extent.width, extent.height}, 0.0f, 1.0f);
        commandBuffer.set_scissor(extent.width, extent.height);

        sceneManager->draw_scene(commandBuffer, testScene, sceneData.projection * sceneData.view);

        commandBuffer.end_render_pass();

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        commandBuffer.copy_image(drawImage.handle, currentSwapchainImage, extent, {displayExtent.width, displayExtent.height});
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
        draw_imgui(cmd.commandBuffer, swapchainData.swapchainImageView, {drawImage.extent.width, drawImage.extent.height});
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
    ImGui::Text("Light Settings");

    auto label = "Lights";
    ImGui::Combo(label, &imguiVariables.selectedLight, imguiVariables.lightNames, imguiVariables.numLights);
    Light* currentLight = &imguiVariables.lights[imguiVariables.selectedLight];

    if (ImGui::InputFloat3("Position", reinterpret_cast<float*>(&currentLight->position)))
        imguiVariables.lightsDirty = true;

    if (ImGui::ColorPicker3("Colour", reinterpret_cast<float*>(&currentLight->colour), ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float))
        imguiVariables.lightsDirty = true;

    if (ImGui::DragFloat("Intensity", &currentLight->intensity, 0.001f, 0.0f, 1.0f))
        imguiVariables.lightsDirty = true;

    if (imguiVariables.lightsDirty) {
        sceneManager->update_light_buffer(cmd);
        imguiVariables.lightsDirty = false;
    }

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
    renderArea.extent.height = extent.height;
    renderArea.extent.width = extent.width;
    renderInfo.renderArea = renderArea;
    renderInfo.pColorAttachments = &ImGUIDrawImage;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;

    auto cmdHandle = cmd.get_handle();
    vkCmdBeginRendering(cmdHandle, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdHandle);
    vkCmdEndRendering(cmdHandle);
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
    const Shader vertShader = context->create_shader("../shaders/vertex.vert.spv");
    const Shader fragShader = context->create_shader("../shaders/pbr.frag.spv");

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

    drawAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    drawAttachment.imageView = context->get_draw_image().view;
    drawAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    drawAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    drawAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    depthAttachment = VkRenderingAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr};
    depthAttachment.imageView = context->get_depth_image().view;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil.depth = 0.f;
}

void Application::init_descriptors() {
    descriptorBuilder = std::make_unique<DescriptorBuilder>(context->get_device());
    auto globalSet = descriptorBuilder->build(opaquePipeline.setLayout);
    opaquePipeline.set = globalSet;
    auto infos = context->get_command_buffer_infos();
    /*for (const auto& info : infos)
        descriptorBuilder->write_buffer(info.SceneData.handle, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);*/

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
    imguiVariables.lights = sceneManager->get_all_lights_p();
    imguiVariables.lightNames = sceneManager->get_light_names().data();
    i32 numLights = sceneManager->get_num_lights();
}


#include "application.h"

Application::Application(std::string_view appName, uint32_t width, uint32_t height)
{
    context = std::make_unique<Context>(appName, width, height);
    init();
    run();
}

void Application::draw()
{
    context->frame_submit([&](const CommandBufferInfo& cmd)
    {
        auto& commandBuffer = cmd.commandBuffer;
        const auto& drawImage = context->get_draw_image();
        const auto& depthImage = context->get_depth_image();
        auto& extent = drawImage.extent;

        commandBuffer.begin();
        commandBuffer.update_uniform(&sceneData, sizeof(sceneData), cmd.SceneData);

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        commandBuffer.image_barrier(depthImage.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

        commandBuffer.set_up_render_pass(extent, &drawAttachment, &depthAttachment);
        commandBuffer.bind_pipeline(opaquePipeline);
        commandBuffer.set_viewport(extent.width, extent.height, 0.0f, 1.0f);
        commandBuffer.set_scissor(extent.width, extent.height);

        //TODO draw calls go here

        commandBuffer.end_render_pass();

        commandBuffer.image_barrier(drawImage.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        commandBuffer.copy_image(drawImage.handle, currentSwapchainImage, extent, extent);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
        //TODO IMGUI draw_imgui(graphicsContext, currentSwapchainImageView, extent);
        commandBuffer.image_barrier(currentSwapchainImage, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
        commandBuffer.end();

        //TODO cmd buffer submission context->submit_graphics_work(currentFrame, graphicsContext, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eAllGraphics);
    });
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
}

void Application::init()
{
    auto& infos = context->get_command_buffer_infos();
    for (auto& info : infos)
    {
        info.SceneData = context->create_buffer(
            sizeof(SceneData),
        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT
        );
    }
}

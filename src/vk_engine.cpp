// ReSharper disable CppMemberFunctionMayBeConst

//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"
#include "glm/common.hpp"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

#include <chrono>
#include <thread>
#include <vulkan/vulkan_core.h>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    initDescriptors();
    initPipelines();

    // everything went fine
    isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (isInitialized) {
        vkQueueWaitIdle(graphicsQueue);
        vkQueueWaitIdle(presentQueue);

        // Destory immediate mode pool and command buffers
        vkWaitForFences(device, 1, &immFence, VK_TRUE, UINT64_MAX);
        vkDestroyCommandPool(device, immCommandPool, nullptr);
        vkDestroyFence(device, immFence, nullptr);

        for (uint8_t i = 0; i < FRAME_OVERLAP; i++) {
            FrameData& frame = frames[i];
            vkDestroyCommandPool(device, frame.commandPool, nullptr);

            vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(device, frame.swapchainSemaphore, nullptr);
            vkDestroyFence(device, frame.renderFence, nullptr);
        }

        vkDestroyPipeline(device, gradientPipeline, nullptr);
        vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);

        descriptorAllocator.destroyPool(device);
        vkDestroyDescriptorSetLayout(device, renderImageDescriptorSetLayout, nullptr);

        destroySwapchain();

        vkDestroyImageView(device, renderImage.imageView, nullptr);
        vmaDestroyImage(allocator, renderImage.image, renderImage.imageAllocation);

        vmaDestroyAllocator(allocator);

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);

        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);

        SDL_DestroyWindow(window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    FrameData& currentFrame = getCurrentFrame();

    // Wait for GPU to finish rendering the current frame
    VK_CHECK(vkWaitForFences(device, 1, &currentFrame.renderFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &currentFrame.renderFence));

    // Request image from swapchain to render to
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, currentFrame.swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

    VkCommandBuffer& commandBuffer = currentFrame.commandBuffer;

    // Reset the command buffer so we can record commands to it again
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // Set the extent in which we will draw
    renderExtent.width = swapchainExtent.width;
    renderExtent.height = swapchainExtent.height;

    // Begin recording to the command buffer, and specify it as a one-time command buffer
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    {
        // TODO: Replace with more specific image layouts
        vkutil::transitionImage(commandBuffer, renderImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        // Bind the compute pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline);

        // Bind the descriptor sets to be used by the pipeline
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &renderImageDescriptorSet, 0, nullptr);

        // Execute the compute pipeline dispatch
        vkCmdDispatch(commandBuffer, glm::ceil(renderExtent.width / 16.0), glm::ceil(renderExtent.height / 16.0), 1);

        // Transition render image to be used as a source for transfer
        vkutil::transitionImage(commandBuffer, renderImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Transition the swapchain image to be a destination for a transfer
        vkutil::transitionImage(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // // Copy the rendered image to the swpachain
        vkutil::copyImageToImage(commandBuffer, renderImage.image, swapchainImages[swapchainImageIndex], renderExtent, swapchainExtent);

        // Transition the cleared image into a presentable layout
        vkutil::transitionImage(commandBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(commandBuffer);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, currentFrame.renderSemaphore);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, currentFrame.swapchainSemaphore);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

    // Submit this command buffer to the graphics queue and begin actual rendering.
    // Once the rendering is finished, the renderFence will be signalled.
    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, currentFrame.renderFence));

    // Prepare for presenting
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &currentFrame.renderSemaphore;
    presentInfo.pImageIndices = &swapchainImageIndex;
    VK_CHECK(vkQueuePresentKHR(presentQueue, &presentInfo));

    // Increase frame number
    frameNumber++;
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stopRendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void VulkanEngine::initVulkan() {
    // Use VkBootstrap to build the Vulkan instance
    vkb::InstanceBuilder builder;

    vkb::Result<vkb::Instance> result = builder.set_app_name("VkGuide program")
        .request_validation_layers()
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    const vkb::Instance vkbInstance = result.value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, &surface);

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE
    };

    // Select physical device that supports all features we want
    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice vkbPhysicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select()
        .value();

    // Create logical device from physical device
    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    chosenGPU = vkbPhysicalDevice.physical_device;

    // Initialize VMA allocator
    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.physicalDevice = chosenGPU;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

    // Find appropriate queue family and create a queue
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
    presentQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::present).value();
}

void VulkanEngine::initSwapchain() {
    createSwapchain(_windowExtent.width, _windowExtent.height);

    VkExtent3D renderImageExtent{};
    renderImageExtent.width = swapchainExtent.width;
    renderImageExtent.height = swapchainExtent.height;
    renderImageExtent.depth = 1;

    renderImage.imageExtent = renderImageExtent;
    renderImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags imageUsageFlags{};
    imageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // NOTE(stefan): This usage flag indicates compute shader writing and reading
    imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

    // Allocate image
    VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(renderImage.imageFormat, imageUsageFlags, renderImageExtent);
    VmaAllocationCreateInfo imageAllocationCreateInfo{};
    imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAllocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(allocator, &imageCreateInfo, &imageAllocationCreateInfo, &renderImage.image, &renderImage.imageAllocation, nullptr));

    VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageview_create_info(renderImage.imageFormat, renderImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &renderImage.imageView));
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder builder{chosenGPU, device, surface};
    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = builder
        .set_desired_format(VkSurfaceFormatKHR{.format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        .set_desired_extent(_windowExtent.width, _windowExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) // So we can transfer to from framebuffer
        .build()
        .value();

    swapchainExtent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroySwapchain() {
    // NOTE: destroying the swapchain destroys the images it holds
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::initCommands() {
    // Create a command pool for buffers submitted to the graphics queue 
    // NOTE: We allow for resetting individual command buffers with the flag.
    VkCommandPoolCreateInfo poolCreateInfo = vkinit::command_pool_create_info(graphicsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (uint8_t i = 0; i < FRAME_OVERLAP; i++) {
        FrameData& frame = frames[i];
        VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &frame.commandPool));
        
        VkCommandBufferAllocateInfo allocateInfo = vkinit::command_buffer_allocate_info(frame.commandPool);
        VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &frame.commandBuffer));
    }

    // Create command pool for immediate mode commands
    VkCommandPoolCreateInfo immPoolCreateInfo = vkinit::command_pool_create_info(graphicsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(device, &immPoolCreateInfo, nullptr, &immCommandPool));

    // Allocate command buffer for immediate mode commands
    VkCommandBufferAllocateInfo immAllocateInfo = vkinit::command_buffer_allocate_info(immCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &immAllocateInfo, &immCommandBuffer));
}

void VulkanEngine::initSyncStructures() {
    // Fence to indicate when the GPU has finished rendering the frame
    VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    // Two semaphores to sync rendering with the swapchain
    VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();

    for (uint8_t i = 0; i < FRAME_OVERLAP; i++) {
        FrameData& frame = frames[i];

        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frame.renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.renderSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.swapchainSemaphore));
    }

    // Create immediate mode fence
    VkFenceCreateInfo immFenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(device, &immFenceInfo, nullptr, &immFence));
}

void VulkanEngine::initDescriptors()
{
    // We only need a single descriptor of type storage image
    std::vector<DescriptorAllocator::CountDescriptorsPerType> counts
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    // Question(stefan): Why 10 here?
    // Initialize descriptor set pool
    descriptorAllocator.initPool(device, 10, counts);

    // Create a descriptor set layout
    DescriptorSetLayout layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    renderImageDescriptorSetLayout = layoutBuilder.build(device, 0);

    // Allocate a descriptor set from the pool
    renderImageDescriptorSet = descriptorAllocator.allocateSet(device, renderImageDescriptorSetLayout);

    // Describe the image descriptor
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = renderImage.imageView;

    // Describe the write operation to do in the descriptor set
    VkWriteDescriptorSet descriptorSetWrite{};
    descriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorSetWrite.dstSet = renderImageDescriptorSet;
    descriptorSetWrite.dstBinding = 0;
    descriptorSetWrite.dstArrayElement = 0;
    descriptorSetWrite.descriptorCount = 1;
    descriptorSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorSetWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, &descriptorSetWrite, 0, nullptr);
}

void VulkanEngine::initPipelines()
{
    initBackgroundPipelines();
}

void VulkanEngine::initBackgroundPipelines()
{
    VkShaderModule gradientShaderModule;
    if (!vkutil::loadShaderModule("../shaders/gradient.comp.spv", device, &gradientShaderModule))
    {
        return;
    }

    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pSetLayouts = &renderImageDescriptorSetLayout;
    layoutCreateInfo.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &gradientPipelineLayout));

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = gradientShaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = gradientPipelineLayout;
    pipelineCreateInfo.stage = stageInfo;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &gradientPipeline));

    vkDestroyShaderModule(device, gradientShaderModule, nullptr);
}

void VulkanEngine::beginImmediateCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(immCommandBuffer, &beginInfo);
}

void VulkanEngine::endImmediateCommandBuffer()
{
    vkEndCommandBuffer(immCommandBuffer);
    vkResetCommandBuffer(immCommandBuffer, 0);
}

void VulkanEngine::initDearImGui()
{
}

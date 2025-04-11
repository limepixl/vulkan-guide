// ReSharper disable CppMemberFunctionMayBeConst

//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

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

    _window = SDL_CreateWindow(
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

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        for (uint8_t i = 0; i < FRAME_OVERLAP; i++) {
            FrameData& frame = frames[i];
            vkDestroyCommandPool(device, frame.commandPool, nullptr);
        }

        destroySwapchain();
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);

        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // nothing yet
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
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
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

    SDL_Vulkan_CreateSurface(_window, instance, &surface);

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

    // Find appropriate queue family and create a queue
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initSwapchain() {
    createSwapchain(_windowExtent.width, _windowExtent.height);
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

}

void VulkanEngine::initSyncStructures() {
}

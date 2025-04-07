// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };

	VkExtent2D _windowExtent{ 1700 , 900 };
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice chosenGPU;
	VkDevice device;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	void initVulkan();

	void initSwapchain();
	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();

	void initCommands();

	void initSyncStructures();
};

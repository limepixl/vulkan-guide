// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <cstdint>
#include <vk_types.h>
#include <vk_descriptors.h>

struct FrameData
{
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore swapchainSemaphore;
	VkSemaphore renderSemaphore;
	VkFence renderFence;
};

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	const char* name = nullptr;
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	bool hasPushConstants = false;
	ComputePushConstants constants;
};

constexpr uint8_t FRAME_OVERLAP = 2;
constexpr bool USE_IMGUI = true;

class VulkanEngine {
public:

	bool isInitialized{ false };
	int frameNumber {0};
	bool stopRendering{ false };

	VmaAllocator allocator;
	
	VkExtent2D _windowExtent{ 1700 , 900 };
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice chosenGPU;
	VkDevice device;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamilyIndex;
	VkQueue presentQueue;
	uint32_t presentQueueFamilyIndex;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkExtent2D renderExtent;
	AllocatedImage renderImage;

	DescriptorAllocator descriptorAllocator;
	VkDescriptorSet renderImageDescriptorSet;
	VkDescriptorSetLayout renderImageDescriptorSetLayout;

	std::vector<ComputeEffect> computeEffects;
	uint8_t currentComputeEffectIndex = 0;

	VkCommandBuffer immCommandBuffer;
	VkCommandPool immCommandPool;
	VkFence immFence;

	// ImGui
	VkDescriptorPool imguiPool;

	struct SDL_Window* window{ nullptr };

	static VulkanEngine& Get();

	FrameData frames[FRAME_OVERLAP];

	FrameData& getCurrentFrame() { return frames[frameNumber % FRAME_OVERLAP]; }

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	void drawDearImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);

	//run main loop
	void run();

private:
	void initVulkan();

	void initSwapchain();
	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();

	void initCommands();

	void initSyncStructures();

	void initDescriptors();

	void initPipelines();
	void initBackgroundPipelines();

	void beginImmediateCommandBuffer();
	void endImmediateCommandBuffer();

	void initDearImGui();
};

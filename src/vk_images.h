
#pragma once 

#include <vulkan/vulkan_core.h>

namespace vkutil {

    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

};
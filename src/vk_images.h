
#pragma once 

#include <vulkan/vulkan_core.h>

namespace vkutil {

    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyImageToImage(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent);
};
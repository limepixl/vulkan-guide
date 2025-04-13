#include "vk_initializers.h"
#include <vk_images.h>
#include <vulkan/vulkan_core.h>

void vkutil::transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    // NOTE(stefan): Using pipeline barriers from synchronization 2 feature

    // TODO: Replace this image barrier with a more specific barrier
    VkImageMemoryBarrier2 imageBarrier{};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    // NOTE(stefan): "all commands" is a bit excessive here, and might be 
    // inefficient. But for a few number of transitions it should be fine.
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask;
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void vkutil::copyImageToImage(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent) {
    VkImageBlit2 imageBlit2{};
    imageBlit2.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    imageBlit2.srcOffsets[0] = VkOffset3D{0, 0, 0};
    imageBlit2.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(srcExtent.width), static_cast<int32_t>(srcExtent.height), 1};
    imageBlit2.dstOffsets[0] = VkOffset3D{0, 0, 0};
    imageBlit2.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(dstExtent.width), static_cast<int32_t>(dstExtent.height), 1};
    imageBlit2.srcSubresource.mipLevel = 0;
    imageBlit2.srcSubresource.baseArrayLayer = 0;
    imageBlit2.srcSubresource.layerCount = 1;
    imageBlit2.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlit2.dstSubresource.mipLevel = 0;
    imageBlit2.dstSubresource.baseArrayLayer = 0;
    imageBlit2.dstSubresource.layerCount = 1;
    imageBlit2.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkBlitImageInfo2 blitInfo2{};
    blitInfo2.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitInfo2.srcImage = src;
    blitInfo2.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo2.dstImage = dst;
    blitInfo2.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo2.filter = VK_FILTER_LINEAR;
    blitInfo2.regionCount = 1;
    blitInfo2.pRegions = &imageBlit2;

    vkCmdBlitImage2(commandBuffer, &blitInfo2);
}
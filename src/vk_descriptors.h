#pragma once

#include <cstdint>
#include <vector>
#include <vk_types.h>

struct DescriptorSetLayout {
    // The bindings this set layout has
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t bindingSlot, VkDescriptorType type, VkShaderStageFlags stageFlags);
    
    VkDescriptorSetLayout build(VkDevice device, VkDescriptorSetLayoutCreateFlags flags);
};

struct DescriptorAllocator {
    struct CountDescriptorsPerType {
        VkDescriptorType type;
        uint32_t count;
    };

    VkDescriptorPool pool;

    void initPool(VkDevice device, uint32_t maxDescriptorSets, std::vector<CountDescriptorsPerType> const& counts);
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocateSet(VkDevice device, VkDescriptorSetLayout layout);
};
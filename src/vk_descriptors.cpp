#include <vk_descriptors.h>

void DescriptorSetLayout::addBinding(uint32_t bindingSlot, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = bindingSlot;
    binding.descriptorCount = 1;
    binding.descriptorType = type;
    binding.stageFlags = stageFlags;
    binding.pImmutableSamplers = nullptr;

    bindings.push_back(binding);
}

VkDescriptorSetLayout DescriptorSetLayout::build(VkDevice device, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayout layout{};

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    createInfo.flags = flags;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout))    ;
    return layout;
}

void DescriptorAllocator::initPool(VkDevice device, uint32_t maxDescriptorSets, std::vector<CountDescriptorsPerType> const& counts)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(counts.size());
    for (uint32_t i = 0; i < counts.size(); i++) {
        poolSizes.push_back(
            VkDescriptorPoolSize{
                .type = counts[i].type,
                .descriptorCount = counts[i].count
            }
        );
    }

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.maxSets = maxDescriptorSets;
    createInfo.poolSizeCount = poolSizes.size();
    createInfo.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(device, &createInfo, nullptr, &pool));
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
    VK_CHECK(vkResetDescriptorPool(device, pool, 0));
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocateSet(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;
    allocateInfo.descriptorPool = pool;

    VkDescriptorSet set{};
    VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &set));
    return set;
}

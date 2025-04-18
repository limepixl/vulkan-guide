#include <vk_pipelines.h>
#include <fstream>

bool vkutil::loadShaderModule(const char * filePath, VkDevice device, VkShaderModule * outShaderModule)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("Failed to open shader file from path: %s\n", filePath);
        return false;
    }

    uint32_t fileSize = file.tellg();
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));

    file.seekg(0);

    file.read((char*)code.data(), fileSize);

    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = code.data();
    createInfo.codeSize = code.size() * sizeof(uint32_t);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}

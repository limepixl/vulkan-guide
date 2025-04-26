#pragma once 
#include <vk_types.h>

namespace vkutil {
    struct PipelineState {
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        // NOTE: No VkPipelineVertexInputStateCreateInfo due to vertex pulling
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
        VkPipelineRasterizationStateCreateInfo rasterizationState{};
        VkPipelineMultisampleStateCreateInfo multisampleState{};
        VkPipelineDepthStencilStateCreateInfo depthStencilState{};
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        
        // Dynamic rendering
        VkPipelineRenderingCreateInfo renderingCreateInfo{}; 

        VkFormat colorAttachmentFormat{};
        VkPipelineLayout pipelineLayout{};
    };

    VkPipeline buildPipeline(PipelineState const& pipelineState, VkDevice device);
    void setPipelineShaders(PipelineState& state, VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void setInputTopology(PipelineState& state, VkPrimitiveTopology topology);
    void setPolygonMode(PipelineState& state, VkPolygonMode mode);
    void setCullMode(PipelineState& state, VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setMultisamplingNone(PipelineState& state);
    void disableBlending(PipelineState& state);
    void setColorAttachmentFormat(PipelineState& state, VkFormat format);
    void setDepthFormat(PipelineState& state, VkFormat format);
    void disableDepthDesting(PipelineState& state);
    void clearPipelineState(PipelineState& state);

    bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};
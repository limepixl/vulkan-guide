#include "vk_initializers.h"
#include <cstdio>
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

VkPipeline vkutil::buildPipeline(const PipelineState &pipelineState, VkDevice device)
{
    VkPipeline pipeline;

    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.flags = 0;

    // Vertex input state (clear because of vertex pulling)
    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &vertexInputState;

    // Set up empty viewport state (as we use dynamic viewport state)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    createInfo.pViewportState = &viewportState;

    // Set up opaque color blending state
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &pipelineState.colorBlendAttachmentState;
    createInfo.pColorBlendState = &colorBlending;

    // Dynamic rendering
    createInfo.pNext = &pipelineState.renderingCreateInfo;
    
    // Other state configured by the program
    createInfo.stageCount = pipelineState.stages.size();
    createInfo.pStages = pipelineState.stages.data();
    createInfo.pInputAssemblyState = &pipelineState.inputAssemblyState;
    createInfo.pRasterizationState = &pipelineState.rasterizationState;
    createInfo.pMultisampleState = &pipelineState.multisampleState;
    createInfo.pDepthStencilState = &pipelineState.depthStencilState;
    createInfo.layout = pipelineState.pipelineLayout;
    
    // Dynamic pipeline state
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamicStates[]
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    dynamicStateInfo.pDynamicStates = dynamicStates;
    dynamicStateInfo.dynamicStateCount = 2;
    createInfo.pDynamicState = &dynamicStateInfo;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        printf("Failed to create graphics pipeline!\n");
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

void vkutil::setPipelineShaders(PipelineState &state, VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    state.stages.resize(2);
    
    // Vertex shader stage
    VkPipelineShaderStageCreateInfo& vertexStage = state.stages[0];
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexShader;
    vertexStage.pName = "main";

    // Fragment shader stage
    VkPipelineShaderStageCreateInfo& fragmentStage = state.stages[1];
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentShader;
    fragmentStage.pName = "main";
}

void vkutil::setInputTopology(PipelineState &state, VkPrimitiveTopology topology)
{
    state.inputAssemblyState.topology = topology;

    // Tutorial doesn't use primitive restart
    state.inputAssemblyState.primitiveRestartEnable = VK_FALSE;
}

void vkutil::setPolygonMode(PipelineState &state, VkPolygonMode mode)
{
    state.rasterizationState.polygonMode = mode;
    state.rasterizationState.lineWidth = 1.0f;
}

void vkutil::setMultisamplingNone(PipelineState& state)
{
    state.multisampleState.sampleShadingEnable = VK_FALSE;
    state.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    state.multisampleState.minSampleShading = 1.0f;
    state.multisampleState.alphaToCoverageEnable = VK_FALSE;
    state.multisampleState.alphaToOneEnable = VK_FALSE;
}

void vkutil::setCullMode(PipelineState &state, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    state.rasterizationState.cullMode = cullMode;
    state.rasterizationState.frontFace = frontFace;
}

void vkutil::disableBlending(PipelineState &state)
{
    state.colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    state.colorBlendAttachmentState.blendEnable = VK_FALSE;
}

void vkutil::setColorAttachmentFormat(PipelineState &state, VkFormat format)
{
    state.colorAttachmentFormat = format;
    state.renderingCreateInfo.pColorAttachmentFormats = &state.colorAttachmentFormat;
    state.renderingCreateInfo.colorAttachmentCount = 1;
}

void vkutil::setDepthFormat(PipelineState &state, VkFormat format)
{
    state.renderingCreateInfo.depthAttachmentFormat = format;
}

void vkutil::disableDepthDesting(PipelineState &state)
{
    state.depthStencilState.depthTestEnable = VK_FALSE;
    state.depthStencilState.depthWriteEnable = VK_FALSE;
    state.depthStencilState.depthBoundsTestEnable = VK_FALSE;
    state.depthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER;
    state.depthStencilState.stencilTestEnable = VK_FALSE;
    state.depthStencilState.minDepthBounds = 0.0f;
    state.depthStencilState.maxDepthBounds = 1.0f;
}

void vkutil::clearPipelineState(PipelineState &state)
{
    state.stages.clear();
    state.inputAssemblyState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    state.rasterizationState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    state.multisampleState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    state.depthStencilState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    state.colorBlendAttachmentState = {};
    state.dynamicState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    state.renderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
}
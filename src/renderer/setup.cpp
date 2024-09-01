#include "render.hpp"
#include <map>
#include <set>
#include <fstream>

// using namespace std;
using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
using glm::defaultp;
using glm::quat;
using glm::ivec2,glm::ivec3,glm::ivec4;
using glm::i8vec2,glm::i8vec3,glm::i8vec4;
using glm::i16vec2,glm::i16vec3,glm::i16vec4;
using glm::uvec2,glm::uvec3,glm::uvec4;
using glm::u8vec2,glm::u8vec3,glm::u8vec4;
using glm::u16vec2,glm::u16vec3,glm::u16vec4;
using glm::vec,glm::vec2,glm::vec3,glm::vec4;
using glm::dvec2,glm::dvec3,glm::dvec4;
using glm::mat, glm::mat2, glm::mat3, glm::mat4;
using glm::dmat2, glm::dmat3, glm::dmat4;

//they are global because its easier lol
const vector<const char*> instanceLayers = {
#ifndef VKNDEBUG
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_LUNARG_monitor",
#endif
};
/*const*/vector<const char*> instanceExtensions = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#ifndef VKNDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif

    VK_KHR_SURFACE_EXTENSION_NAME,
    // "VK_KHR_win32_surface",
};
const vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, //simplifies renderer for negative cost lol
    VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, //for measuring frame times - dynamic quality to perfomance exchange
    VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, //pco / ubo +perfomance
    VK_KHR_16BIT_STORAGE_EXTENSION_NAME, //just explicit control
    VK_KHR_8BIT_STORAGE_EXTENSION_NAME, //just explicit control
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
    // VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME
};

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies (physicalDevice);
    VkCommandPoolCreateInfo
        poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicalAndCompute.value();
    VK_CHECK (vkCreateCommandPool (device, &poolInfo, NULL, &commandPool));
}

void Renderer::createCommandBuffers (vector<VkCommandBuffer>* commandBuffers, u32 size) {
    (*commandBuffers).resize (size);
    VkCommandBufferAllocateInfo 
        allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (*commandBuffers).size();
    VK_CHECK (vkAllocateCommandBuffers (device, &allocInfo, (*commandBuffers).data()));
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize (MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize (MAX_FRAMES_IN_FLIGHT);
    frameInFlightFences.resize (MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo 
        semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo 
        fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, NULL, &imageAvailableSemaphores[i]));
        VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, NULL, &renderFinishedSemaphores[i]));
        VK_CHECK (vkCreateFence (device, &fenceInfo, NULL, &frameInFlightFences[i]));
    }
}

vector<char> readFile (const char* filename) {
    std::ifstream file (filename, std::ios::ate | std::ios::binary);
    assert (file.is_open());
    u32 fileSize = file.tellg();
    vector<char> buffer (fileSize);
    file.seekg (0);
    file.read (buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule Renderer::createShaderModule (vector<char>* code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo 
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = (*code).size();
        createInfo.pCode = reinterpret_cast<const u32*> ((*code).data());
    VK_CHECK (vkCreateShaderModule (device, &createInfo, NULL, &shaderModule));
    return shaderModule;
}

VkAttachmentLoadOp  Renderer::getOpLoad (LoadStoreOp op){
    switch (op) {
        case DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        default:
            crash(wrong load op);
    }
}
VkAttachmentStoreOp Renderer::getOpStore(LoadStoreOp op){
    switch (op) {
        case DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case Clear:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        default:
          crash(wrong load op);
    }
}
void Renderer::createRenderPass(vector<AttachmentDescription> attachments, vector<SubpassAttachments> sas, VkRenderPass* rpass){
    vector<VkAttachmentDescription> adescs(attachments.size());
    vector<VkAttachmentReference  > arefs(attachments.size());
    std::map<void*, u32> img2ref = {};

    for(int i=0; i<attachments.size(); i++){
        adescs[i].format = attachments[i].image->format;
        cout << string_VkFormat(attachments[i].image->format) << "\n";
        adescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
        adescs[i].loadOp  = getOpLoad (attachments[i].load);
        adescs[i].storeOp = getOpStore(attachments[i].store);
        adescs[i].stencilLoadOp  = getOpLoad (attachments[i].sload);
        adescs[i].stencilStoreOp = getOpStore(attachments[i].sstore);
        if((attachments[i].load == DontCare) && (attachments[i].sload == DontCare)){
            adescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        } else adescs[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        adescs[i].finalLayout = attachments[i].finalLayout;
        arefs[i] = {u32(i), VK_IMAGE_LAYOUT_GENERAL};
        img2ref[attachments[i].image] = i;
    }

    vector<VkSubpassDescription > subpasses(sas.size());
    vector<SubpassAttachmentRefs> sas_refs(sas.size());

    for(int i=0; i<sas.size(); i++){
        if(sas[i].a_depth != NULL){
            sas_refs[i].a_depth = arefs[img2ref[sas[i].a_depth]];
        }
        for(auto color: sas[i].a_color){
            sas_refs[i].a_color.push_back(arefs[img2ref[color]]);
        }
        for(auto input: sas[i].a_input){
            sas_refs[i].a_input.push_back(arefs[img2ref[input]]);
        }
    }

    for(int i=0; i<sas.size(); i++){
        subpasses[i] = {};
        subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[i].colorAttachmentCount = sas_refs[i].a_color.size();
        subpasses[i].pColorAttachments = sas_refs[i].a_color.data();
        subpasses[i].inputAttachmentCount = sas_refs[i].a_input.size();
        subpasses[i].pInputAttachments = sas_refs[i].a_input.data();
        subpasses[i].pDepthStencilAttachment = (sas[i].a_depth == NULL)? NULL : &sas_refs[i].a_depth;
    }

    for(int i=0; i<sas.size(); i++){
        for(auto pipe: sas[i].pipes){
            pipe->subpassId = i;
        }
    }
    printl(sas.size())
    vector<VkSubpassDependency> 
        dependencies (1);
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        dependencies[0].dependencyFlags = 0;
    VkSubpassDependency 
        full_wait = {};
        full_wait.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        full_wait.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        full_wait.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        full_wait.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        full_wait.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    for (int i = 0; i < sas.size(); i++) {
        for (int j = i + 1; j < sas.size(); j++) {
            full_wait.srcSubpass = i;
            full_wait.dstSubpass = j;
            dependencies.push_back (full_wait);
        }
    }
    VkRenderPassCreateInfo 
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = adescs.size();
        createInfo.pAttachments = adescs.data();
        createInfo.subpassCount = subpasses.size();
        createInfo.pSubpasses = subpasses.data();
        createInfo.dependencyCount = dependencies.size();
        createInfo.pDependencies = dependencies.data();
    VK_CHECK (vkCreateRenderPass (device, &createInfo, NULL, rpass));
    
    for(int i=0; i<sas.size(); i++){
        for(auto pipe: sas[i].pipes){
            pipe->renderPass = *rpass;
        }
    }
} 

void Renderer::destroyRasterPipeline (RasterPipe* pipe) {
    vkDestroyPipeline (device, pipe->line, NULL);
    vkDestroyPipelineLayout (device, pipe->lineLayout, NULL);
    vkDestroyDescriptorSetLayout (device, pipe->setLayout, NULL);
}

static bool stencil_is_empty (VkStencilOpState stencil) {
    return
        (stencil.failOp == 0) &&
        (stencil.passOp == 0) &&
        (stencil.depthFailOp == 0) &&
        (stencil.compareOp == 0) &&
        (stencil.compareMask == 0) &&
        (stencil.writeMask == 0) &&
        (stencil.reference == 0);
}

void Renderer::createRasterPipeline (RasterPipe* pipe, VkDescriptorSetLayout extra_dynamic_layout, vector<ShaderStage> shader_stages, vector<AttrFormOffs> attr_desc,
                                       u32 stride, VkVertexInputRate input_rate, VkPrimitiveTopology topology,
                                       VkExtent2D extent, vector<BlendAttachment> blends, u32 push_size, DepthTesting depthTest, VkCullModeFlags culling, Discard discard, const VkStencilOpState stencil) {
    vector<VkShaderModule > shader_modules (shader_stages.size());
    vector<VkPipelineShaderStageCreateInfo> shaderStages (shader_stages.size());
    for (int i = 0; i < shader_stages.size(); i++) {
        vector<char> shader_code = readFile (shader_stages[i].src);
        shader_modules[i] = createShaderModule (&shader_code);
        shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[i].stage = shader_stages[i].stage;
        shaderStages[i].module = shader_modules[i];
        shaderStages[i].pName = "main";
    }
    vector<VkVertexInputAttributeDescription> attributeDescriptions (attr_desc.size());
    for (int i = 0; i < attr_desc.size(); i++) {
        attributeDescriptions[i].binding = 0;
        attributeDescriptions[i].location = i;
        attributeDescriptions[i].format = attr_desc[i].format;
        attributeDescriptions[i].offset = attr_desc[i].offset;
    }
    VkVertexInputBindingDescription
        bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = stride;
        bindingDescription.inputRate = input_rate;
    VkPipelineVertexInputStateCreateInfo
        vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    if (attributeDescriptions.size() != 0) {
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    }
    VkPipelineInputAssemblyStateCreateInfo
        inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
    VkViewport
        viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) extent.width;
        viewport.height = (float) extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
    vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo
        dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();
    VkPipelineViewportStateCreateInfo
        viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo
        rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = (discard == NO_DISCARD) ? VK_FALSE : VK_TRUE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;
        rasterizer.cullMode = culling;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineMultisampleStateCreateInfo
        multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = NULL;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;
    // blends
    vector<VkPipelineColorBlendAttachmentState> blendAttachments (blends.size());
    for (int i = 0; i < blends.size(); i++) {
        //for now only two possible states
        if (blends[i] == NO_BLEND) {
            blendAttachments[i].blendEnable = VK_FALSE;
        } else {
            blendAttachments[i].blendEnable = VK_TRUE;
        }
        blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        if (blends[i] == BLEND_MIX) {
            blendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
            blendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
        }
        if (blends[i] == BLEND_SUB) {
            blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachments[i].colorBlendOp = VK_BLEND_OP_SUBTRACT;
            blendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        }
        if (blends[i] == BLEND_REPLACE_IF_GREATER) {
            blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].colorBlendOp = VK_BLEND_OP_MAX;
            //no alpha
            blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        } else if (blends[i] == BLEND_REPLACE_IF_LESS) {
            blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].colorBlendOp = VK_BLEND_OP_MIN;
            //no alpha
            blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo 
        colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = blendAttachments.size();
        colorBlending.pAttachments = blendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;
    VkPushConstantRange 
        pushRange = {};
        pushRange.size = push_size; //trans
        pushRange.offset = 0;
    VkShaderStageFlags stageFlags = 0;
    for (auto stage : shader_stages) {
        stageFlags |= stage.stage;
    }
    pushRange.stageFlags = stageFlags;
    vector<VkDescriptorSetLayout> used_dset_layouts = {pipe->setLayout};
    if (extra_dynamic_layout != 0) {
        used_dset_layouts.push_back (extra_dynamic_layout);
    }
    VkPipelineLayoutCreateInfo
        pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = used_dset_layouts.size();
        pipelineLayoutInfo.pSetLayouts = used_dset_layouts.data();
    if (push_size != 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    } else {
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = NULL;
    }
    VK_CHECK (vkCreatePipelineLayout (device, &pipelineLayoutInfo, NULL, &pipe->lineLayout));
    //if not depthTest then just not used. Done this way because same dept state used everywhere on not used at all
    VkPipelineDepthStencilStateCreateInfo
        depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    switch (depthTest) {
        case NO_DEPTH_TEST: {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            break;
        }
        case FULL_DEPTH_TEST: {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            break;
        }
        case READ_DEPTH_TEST: {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_FALSE;
            break;
        }
        case WRITE_DEPTH_TEST: {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_TRUE;
            break;
        }
        default: crash (whats depthTest);
    }
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
    if (stencil_is_empty (stencil)) {
        depthStencil.stencilTestEnable = VK_FALSE;
    } else {
        depthStencil.stencilTestEnable = VK_TRUE;
        depthStencil.front = stencil;
        depthStencil.back = stencil;
    }
    VkGraphicsPipelineCreateInfo
        pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
    if ((depthTest == NO_DEPTH_TEST) and (stencil_is_empty (stencil))) {
        pipelineInfo.pDepthStencilState = NULL;
    } else {
        pipelineInfo.pDepthStencilState = &depthStencil;
    }
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipe->lineLayout;
        pipelineInfo.renderPass = pipe->renderPass;
        pipelineInfo.subpass = pipe->subpassId;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;
    VK_CHECK (vkCreateGraphicsPipelines (device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipe->line));
    for (int i = 0; i < shader_stages.size(); i++) {
        vkDestroyShaderModule (device, shader_modules[i], NULL);
    }
}

void Renderer::destroyComputePipeline (ComputePipe* pipe) {
    vkDestroyPipeline (device, pipe->line, NULL);
    vkDestroyPipelineLayout (device, pipe->lineLayout, NULL);
    vkDestroyDescriptorSetLayout (device, pipe->setLayout, NULL);
}

void Renderer::createComputePipeline (ComputePipe* pipe, VkDescriptorSetLayout extra_dynamic_layout, const char* src, u32 push_size, VkPipelineCreateFlags create_flags) {
    auto compShaderCode = readFile (src);
    VkShaderModule module = createShaderModule (&compShaderCode);
    // VkSpecializationInfo specConstants = {};
    VkPipelineShaderStageCreateInfo
        compShaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = module;
        compShaderStageInfo.pName = "main";
    VkPushConstantRange
        pushRange = {};
        pushRange.size = push_size;
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    vector<VkDescriptorSetLayout> used_dset_layouts = {pipe->setLayout};
    if (extra_dynamic_layout != 0) {
        used_dset_layouts.push_back (extra_dynamic_layout);
    }
    VkPipelineLayoutCreateInfo
        pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = used_dset_layouts.size();
        pipelineLayoutInfo.pSetLayouts = used_dset_layouts.data();
    if (push_size != 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    }
    VK_CHECK (vkCreatePipelineLayout (device, &pipelineLayoutInfo, NULL, &pipe->lineLayout));
    VkComputePipelineCreateInfo
        pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = compShaderStageInfo;
        pipelineInfo.layout = pipe->lineLayout;
        pipelineInfo.flags = create_flags;
    VK_CHECK (vkCreateComputePipelines (device, NULL, 1, &pipelineInfo, NULL, &pipe->line));
    vkDestroyShaderModule (device, module, NULL);
}

void Renderer::createSurface() {
    VK_CHECK (glfwCreateWindowSurface (instance, window.pointer, NULL, &surface));
}

QueueFamilyIndices Renderer::findQueueFamilies (VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties (device, &queueFamilyCount, NULL);
    std::vector<VkQueueFamilyProperties> queueFamilies (queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties (device, &queueFamilyCount, queueFamilies.data());
    i32 i = 0;
    for (auto queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) and (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicalAndCompute = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR (device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.present = i;
        }
        if (indices.isComplete()) {
            break;
        }
        i++;
    }
    return indices;
}

bool Renderer::checkPhysicalDeviceExtensionSupport (VkPhysicalDevice device) {
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties (device, NULL, &extensionCount, NULL);
    vector<VkExtensionProperties> availableExtensions (extensionCount);
    vkEnumerateDeviceExtensionProperties (device, NULL, &extensionCount, availableExtensions.data());
    //for time complexity? FOR CODE COMPLEXITY
    //does not apply to current code
    vector<const char*> requiredExtensions = deviceExtensions;
    for (auto req : requiredExtensions) {
        bool supports = false;
        for (auto avail : availableExtensions) {
            if (strcmp (req, avail.extensionName) == 0) {
                supports = true;
            }
        }
        if (not supports) {
            // cout << KRED << req << "not supported\n" << KEND;
            return false;
        }
    }
    return true;
}

SwapChainSupportDetails Renderer::querySwapchainSupport (VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR (device, surface, &details.capabilities);
    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &formatCount, NULL);
    details.formats.resize (formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR (device, surface, &formatCount, details.formats.data());
    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR (device, surface, &presentModeCount, NULL);
    details.presentModes.resize (presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR (device, surface, &presentModeCount, details.presentModes.data());
    return details;
}

bool Renderer::checkFormatSupport (VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties (device, format, &formatProps);
    return formatProps.optimalTilingFeatures& features;
}

bool Renderer::isPhysicalDeviceSuitable (VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies (device);
    SwapChainSupportDetails swapChainSupport;
    bool extensionsSupported = checkPhysicalDeviceExtensionSupport (device);
    bool imageFormatSupport = checkFormatSupport (
                                  device,
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
                              );
    // bool srgbFormatSupport = check_Format_Support(
    // device,
    // VK_FORMAT_R8G8B8A8_SRGB,
    // VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    // );
    if (extensionsSupported) {
        swapChainSupport = querySwapchainSupport (device);
    }
    return indices.isComplete()
           && extensionsSupported
           && swapChainSupport.is_Suitable()
           && imageFormatSupport
           ;
}

void Renderer::pickPhysicalDevice() {
    u32 deviceCount;
    VK_CHECK (vkEnumeratePhysicalDevices (instance, &deviceCount, NULL));
    vector<VkPhysicalDevice> devices (deviceCount);
    VK_CHECK (vkEnumeratePhysicalDevices (instance, &deviceCount, devices.data()));
    physicalDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        if (isPhysicalDeviceSuitable (device)) {
            physicalDevice = device;//could be moved out btw
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        cout << KRED "no suitable physical device\n" KEND;
        exit (1);
    }
    vkGetPhysicalDeviceProperties (physicalDevice, &physicalDeviceProperties);
}

void Renderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies (physicalDevice);
    vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    std::set<u32> uniqueQueueFamilies = {indices.graphicalAndCompute.value(), indices.present.value()};
    float queuePriority = 1.0f; //random priority?
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo 
            queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back (queueCreateInfo);
    }
    VkPhysicalDeviceFeatures 
        deviceFeatures = {};
        deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
        deviceFeatures.geometryShader = VK_TRUE;
        deviceFeatures.independentBlend = VK_TRUE;
        deviceFeatures.shaderInt16 = VK_TRUE;
    VkPhysicalDeviceVulkan11Features 
        deviceFeatures11 = {};
        deviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        deviceFeatures11.storagePushConstant16 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features 
        deviceFeatures12 = {};
        deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        deviceFeatures12.storagePushConstant8 = VK_TRUE;
        deviceFeatures12.shaderInt8 = VK_TRUE;
        deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
        deviceFeatures12.pNext = &deviceFeatures11;
    VkPhysicalDeviceFeatures2 
        physical_features2 = {};
        physical_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physical_features2.pNext = &deviceFeatures12;
        physical_features2.features = deviceFeatures;
    VkDeviceCreateInfo 
        createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
    // createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();
        createInfo.pNext = &physical_features2;
    VK_CHECK (vkCreateDevice (physicalDevice, &createInfo, NULL, &device));
    vkGetDeviceQueue (device, indices.graphicalAndCompute.value(), 0, &graphicsQueue);
    // vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &computeQueue);
    vkGetDeviceQueue (device, indices.present.value(), 0, &presentQueue);
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat (vector<VkSurfaceFormatKHR> availableFormats) {
    for (auto format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    cout << KYEL "Where is your RAYTRACED_IMAGE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR?\n" KEND;
    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode (vector<VkPresentModeKHR> availablePresentModes) {
    for (auto mode : availablePresentModes) {
        if (
            ((mode == VK_PRESENT_MODE_FIFO_KHR) && settings.vsync) ||
            ((mode == VK_PRESENT_MODE_MAILBOX_KHR) && !settings.vsync)
        ) {
            return mode;
        }
    }
    for (auto mode : availablePresentModes) {
        if (
            ((mode == VK_PRESENT_MODE_IMMEDIATE_KHR) && !settings.vsync)
        ) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent (VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT_MAX) {
        return capabilities.currentExtent;
    } else {
        i32 width, height;
        glfwGetFramebufferSize (window.pointer, &width, &height);
        VkExtent2D actualExtent { (u32) width, (u32) height};
        actualExtent.width = std::clamp (actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp (actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void Renderer::createAllocator() {
    VmaVulkanFunctions 
        vulkanFunctions;
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
        vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
        vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    VmaAllocatorCreateInfo 
        allocatorInfo = {};
        allocatorInfo.pVulkanFunctions = (const VmaVulkanFunctions*) &vulkanFunctions;
        allocatorInfo.instance = instance;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
    VK_CHECK (vmaCreateAllocator (&allocatorInfo, &VMAllocator));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    if (messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) { printf (KCYN); }
    if (messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { printf (KRED); }
    if (messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) { printf (KBLU); }
    printf ("validation layer: %s\n\n" KEND, pCallbackData->pMessage);
    return VK_FALSE;
}

// void Renderer::debugSetName(){
// VkDebugMarkerObjectNameInfoEXT name = {};
// name.pObjectName = "";
// name.objectType
// // name.objectType
// // vkSetDebugUtilsObjectNameEXT(device, &name);
// vkDebugMarkerSetObjectNameEXT(device, &name);
// // vkDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
// }
#ifndef VKNDEBUG
void Renderer::setupDebugMessenger() {
    VkDebugUtilsMessengerEXT debugMessenger;
    VkDebugUtilsMessengerCreateInfoEXT 
        debugUtilsCreateInfo = {};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;
    VK_CHECK (vkCreateDebugUtilsMessengerEXT (instance, &debugUtilsCreateInfo, NULL, &debugMessenger));
    this->debugMessenger = debugMessenger;
}
#endif

void Renderer::getInstanceExtensions() {
    u32 glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions (&glfwExtCount);
    assert (glfwExtensions != NULL);
    assert (glfwExtCount > 0);
    for (u32 i = 0; i < glfwExtCount; i++) {
        instanceExtensions.push_back (glfwExtensions[i]);
        printl (glfwExtensions[i]);
    }
    //actual verify part
    u32 supportedExtensionCount = 0;
    VK_CHECK (vkEnumerateInstanceExtensionProperties (NULL, &supportedExtensionCount, NULL));
    vector<VkExtensionProperties> supportedInstanceExtensions (supportedExtensionCount);
    VK_CHECK (vkEnumerateInstanceExtensionProperties (NULL, &supportedExtensionCount, supportedInstanceExtensions.data()));
    for (auto ext : instanceExtensions) {
        bool supported = false;
        for (auto sup : supportedInstanceExtensions) {
            if (strcmp (ext, sup.extensionName) == 0) { supported = true; }
        }
        if (not supported) {
            cout << KRED << ext << " not supported\n" << KEND;
            exit (1);
        }
    }
}

static void framebuffer_Resize_Callback (GLFWwindow* window, int width, int height) {
    auto app = (Renderer*) (glfwGetWindowUserPointer (window));
    app->resized = true;
}

void Renderer::createWindow() {
    int glfwRes = glfwInit();
    assert (glfwRes != 0);
    glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const GLFWvidmode* mode = glfwGetVideoMode (glfwGetPrimaryMonitor());
    assert (mode != NULL);
    window.width = (mode->width / 2) * 2;
    window.height = (mode->height / 2) * 2;
    if (settings.fullscreen) {
        window.pointer = glfwCreateWindow (window.width, window.height, "renderer_vk", glfwGetPrimaryMonitor(), 0);
    } else {
        window.pointer = glfwCreateWindow (window.width, window.height, "renderer_vk", 0, 0);
    }
    assert (window.pointer != NULL);
    glfwSetWindowUserPointer (window.pointer, this);
    glfwSetInputMode (window.pointer, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetInputMode (window.pointer, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
    glfwSetFramebufferSizeCallback (window.pointer, framebuffer_Resize_Callback);
}

void Renderer::createInstance() {
    VkApplicationInfo 
        app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Lol vulkan raytracer";
        app_info.applicationVersion = VK_MAKE_VERSION (1, 0, 0);
        app_info.pEngineName = "my vk engine";
        app_info.engineVersion = VK_MAKE_VERSION (1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;
    vector<VkValidationFeatureEnableEXT> enabledFeatures = {
        // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };
    VkValidationFeaturesEXT 
        features = {};
        features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        features.enabledValidationFeatureCount = enabledFeatures.size();
        features.pEnabledValidationFeatures = enabledFeatures.data();
    VkInstanceCreateInfo createInfo = {};
#ifndef VKNDEBUG
        createInfo.pNext = &features; // according to spec
#endif
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info;
        createInfo.enabledExtensionCount = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.enabledLayerCount = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();
    VK_CHECK (vkCreateInstance (&createInfo, NULL, &instance));
}

void Renderer::createImageStorages (vector<Image>* images,
                                      VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect,
                                      uvec3 size, int mipmaps, VkSampleCountFlagBits sample_count) {
    (*images).resize (MAX_FRAMES_IN_FLIGHT);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createImageStorages(&(*images)[i], type, format, usage, vma_usage, vma_flags, aspect, size, mipmaps, sample_count);
    }
}

void Renderer::createImageStorages (Image* image,
                                      VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect,
                                      uvec3 size, int mipmaps, VkSampleCountFlagBits sample_count) {
    // VkFormat chosen_format = findSupportedFormat(format, type, VK_IMAGE_TILING_OPTIMAL, usage);
    image->aspect = aspect;
    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->format = format;
    VkImageCreateInfo 
        imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = type;
        imageInfo.format = format;
        imageInfo.mipLevels = mipmaps;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = sample_count;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.extent.width = size.x;
        imageInfo.extent.height = size.y;
        imageInfo.extent.depth = size.z;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo 
        allocInfo = {};
        allocInfo.usage = vma_usage;
        allocInfo.flags = vma_flags;
    VK_CHECK (vmaCreateImage (VMAllocator, &imageInfo, &allocInfo, &image->image, &image->alloc, NULL));
    VkImageViewCreateInfo 
        viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image->image;
    if (type == VK_IMAGE_TYPE_2D) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    } else if (type == VK_IMAGE_TYPE_3D) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    }
    viewInfo.format = format;
    if ((aspect & VK_IMAGE_ASPECT_DEPTH_BIT) and (aspect & VK_IMAGE_ASPECT_STENCIL_BIT)) {
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; //create stencil view yourself
    } else {
        viewInfo.subresourceRange.aspectMask = aspect;
    }
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = mipmaps;
        viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK (vkCreateImageView (device, &viewInfo, NULL, &image->view));
    // transition_Image_Layout_Singletime(image->image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
    // VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    // 0, access);
}

//fill manually with cmd or copy
void Renderer::createBufferStorages (vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host) {
    (*buffers).resize (MAX_FRAMES_IN_FLIGHT);
    // allocs.resize(MAX_FRAMES_IN_FLIGHT);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo 
            bufferInfo = {};
            bufferInfo.size = size;
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
        if (host) {
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK (vmaCreateBuffer (VMAllocator, &bufferInfo, &allocInfo, & (*buffers)[i].buffer, & (*buffers)[i].alloc, NULL));
    }
}

void Renderer::createBufferStorages (Buffer* buffer, VkBufferUsageFlags usage, u32 size, bool host) {
    VkBufferCreateInfo 
        bufferInfo = {};
        bufferInfo.size = size;
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = usage;
    VmaAllocationCreateInfo allocInfo = {};
    if (host) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    VK_CHECK (vmaCreateBuffer (VMAllocator, &bufferInfo, &allocInfo, & (*buffer).buffer, & (*buffer).alloc, NULL));
}

vector<Image> Renderer::create_RayTrace_VoxelImages (Voxel* voxels, ivec3 size) {
    VkDeviceSize bufferSize = (sizeof (Voxel)) * size.x * size.y * size.z;
    vector<Image> voxelImages = {};
    createImageStorages (&voxelImages,
                           VK_IMAGE_TYPE_3D,
                           VK_FORMAT_R8_UINT,
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                           0, // no VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           size);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        transitionImageLayoutSingletime (&voxelImages[i], VK_IMAGE_LAYOUT_GENERAL);
    }
    VkBufferCreateInfo 
        stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo 
        stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer (VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);
    void* data;
    vmaMapMemory (VMAllocator, stagingAllocation, &data);
    memcpy (data, voxels, bufferSize);
    vmaUnmapMemory (VMAllocator, stagingAllocation);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        copyBufferSingleTime (stagingBuffer, &voxelImages[i], size);
    }
    vmaDestroyBuffer (VMAllocator, stagingBuffer, stagingAllocation);
    return voxelImages;
}

void Renderer::countDescriptor (const VkDescriptorType type) {
    if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        STORAGE_BUFFER_DESCRIPTOR_COUNT++;
    } else if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT++;
    } else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        STORAGE_IMAGE_DESCRIPTOR_COUNT++;
    } else if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        UNIFORM_BUFFER_DESCRIPTOR_COUNT++;
    } else if (type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
        INPUT_ATTACHMENT_DESCRIPTOR_COUNT++;
    } else {
        cout << KRED "ADD DESCRIPTOR TO COUNTER\n" KEND;
        abort();
    }
}

//in order
void Renderer::createDescriptorSetLayout (vector<VkDescriptorType> descriptorTypes, VkShaderStageFlags baseStages, VkDescriptorSetLayout* layout, VkDescriptorSetLayoutCreateFlags flags) {
    vector<VkDescriptorSetLayoutBinding> bindings = {};
    for (i32 i = 0; i < descriptorTypes.size(); i++) {
        countDescriptor (descriptorTypes[i]);
        VkDescriptorSetLayoutBinding
        bind = {};
        bind.binding = i;
        bind.descriptorType = descriptorTypes[i];
        bind.descriptorCount = 1;
        bind.stageFlags = baseStages;
        bindings.push_back (bind);
    }
    VkDescriptorSetLayoutCreateInfo
        layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags = flags;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();
    VK_CHECK (vkCreateDescriptorSetLayout (device, &layoutInfo, NULL, layout));
}

void Renderer::createDescriptorSetLayout (vector<VkDescriptorType> descriptorTypes, vector<VkShaderStageFlags> stages, VkDescriptorSetLayout* layout, VkDescriptorSetLayoutCreateFlags flags) {
    vector<VkDescriptorSetLayoutBinding> bindings = {};
    for (i32 i = 0; i < descriptorTypes.size(); i++) {
        countDescriptor (descriptorTypes[i]);
        VkDescriptorSetLayoutBinding
        bind = {};
        bind.binding = i;
        bind.descriptorType = descriptorTypes[i];
        bind.descriptorCount = 1;
        bind.stageFlags = stages[i];
        bindings.push_back (bind);
    }
    VkDescriptorSetLayoutCreateInfo
        layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags = flags;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();
    VK_CHECK (vkCreateDescriptorSetLayout (device, &layoutInfo, NULL, layout));
}

void Renderer::createDescriptorPool() {
    printl (STORAGE_IMAGE_DESCRIPTOR_COUNT);
    printl (COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT);
    printl (STORAGE_BUFFER_DESCRIPTOR_COUNT);
    printl (UNIFORM_BUFFER_DESCRIPTOR_COUNT);
    VkDescriptorPoolSize 
        STORAGE_IMAGE_PoolSize = {};
        STORAGE_IMAGE_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        STORAGE_IMAGE_PoolSize.descriptorCount = STORAGE_IMAGE_DESCRIPTOR_COUNT * 8 + 1;
    VkDescriptorPoolSize 
        COMBINED_IMAGE_SAMPLER_PoolSize = {};
        COMBINED_IMAGE_SAMPLER_PoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        COMBINED_IMAGE_SAMPLER_PoolSize.descriptorCount = COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT * 8 + 1;
    VkDescriptorPoolSize 
        STORAGE_BUFFER_PoolSize = {};
        STORAGE_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        STORAGE_BUFFER_PoolSize.descriptorCount = STORAGE_BUFFER_DESCRIPTOR_COUNT * 8 + 1;
    VkDescriptorPoolSize 
        UNIFORM_BUFFER_PoolSize = {};
        UNIFORM_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UNIFORM_BUFFER_PoolSize.descriptorCount = UNIFORM_BUFFER_DESCRIPTOR_COUNT * 8 + 1;
    VkDescriptorPoolSize 
        INPUT_ATTACHMENT_PoolSize = {};
        INPUT_ATTACHMENT_PoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        INPUT_ATTACHMENT_PoolSize.descriptorCount = INPUT_ATTACHMENT_DESCRIPTOR_COUNT * 8 + 1;
    vector<VkDescriptorPoolSize> poolSizes = {
        STORAGE_IMAGE_PoolSize,
        COMBINED_IMAGE_SAMPLER_PoolSize,
        // STORAGE_BUFFER_PoolSize,
        UNIFORM_BUFFER_PoolSize,
        INPUT_ATTACHMENT_PoolSize
    };
    VkDescriptorPoolCreateInfo 
        poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = descriptor_sets_count; //becuase frames_in_flight multiply of 5 differents sets, each for shader
    VK_CHECK (vkCreateDescriptorPool (device, &poolInfo, NULL, &descriptorPool));
}

static void allocate_Descriptor (vector<VkDescriptorSet>& sets, VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDevice device) {
    sets.resize (MAX_FRAMES_IN_FLIGHT);
    vector<VkDescriptorSetLayout> layouts (MAX_FRAMES_IN_FLIGHT, layout);
    VkDescriptorSetAllocateInfo 
        allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts = layouts.data();
    VK_CHECK (vkAllocateDescriptorSets (device, &allocInfo, sets.data()));
}

void Renderer::deferDescriptorSetup (VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags baseStages, VkDescriptorSetLayoutCreateFlags createFlags) {
    if (*dsetLayout == VK_NULL_HANDLE) {
        vector<VkDescriptorType> descriptorTypes (descriptions.size());
        vector<VkShaderStageFlags> descriptorStages (descriptions.size());
        for (int i = 0; i < descriptions.size(); i++) {
            descriptorTypes[i] = descriptions[i].type;
            descriptorStages[i] = (descriptions[i].stages == 0) ? baseStages : descriptions[i].stages;
        }
        createDescriptorSetLayout (descriptorTypes, descriptorStages, dsetLayout, createFlags);
    }
    descriptor_sets_count += MAX_FRAMES_IN_FLIGHT;
    DelayedDescriptorSetup delayed_setup = {dsetLayout, descriptorSets, descriptions, baseStages, createFlags};
    delayed_descriptor_setups.push_back (delayed_setup);
}

void Renderer::setupDescriptor (VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags stages) {
    for (int frame_i = 0; frame_i < (*descriptorSets).size(); frame_i++) {
        int previous_frame_i = frame_i - 1;
        if (previous_frame_i < 0) { previous_frame_i = MAX_FRAMES_IN_FLIGHT - 1; } // so frames do not intersect
        vector<VkDescriptorImageInfo > image_infos (descriptions.size());
        vector<VkDescriptorBufferInfo> buffer_infos (descriptions.size());
        vector<VkWriteDescriptorSet> writes (descriptions.size());
        for (int i = 0; i < descriptions.size(); i++) {
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[i].dstSet = (*descriptorSets)[frame_i];
            writes[i].dstBinding = i;
            writes[i].dstArrayElement = 0;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = descriptions[i].type;
            int descriptor_frame_id = 0;
            switch (descriptions[i].relativePos) {
                case RD_CURRENT: {
                    descriptor_frame_id = frame_i;
                    break;
                }
                case RD_PREVIOUS: {
                    descriptor_frame_id = previous_frame_i;
                    break;
                }
                case RD_FIRST: {
                    descriptor_frame_id = 0;
                    break;
                }
                case RD_NONE: {
                    writes[i].descriptorCount = 0;
                    continue;
                }
            }
            if (!descriptions[i].images.empty()) {
                image_infos[i].imageView = descriptions[i].images[descriptor_frame_id].view;
                image_infos[i].imageLayout = descriptions[i].imageLayout;
                if (descriptions[i].imageSampler != NULL) {
                    image_infos[i].sampler = descriptions[i].imageSampler;
                    if (descriptions[i].type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        crash (descriptor has sampler but type is not for sampler);
                    }
                }
                writes[i].pImageInfo = &image_infos[i];
                assert (descriptions[i].buffers.empty());
            } else if (!descriptions[i].buffers.empty()) {
                buffer_infos[i].buffer = descriptions[i].buffers[descriptor_frame_id].buffer;
                buffer_infos[i].offset = 0;
                buffer_infos[i].range = VK_WHOLE_SIZE;
                writes[i].pBufferInfo = &buffer_infos[i];
            } else { crash (what is descriptor type ? ); }
        }
        vkUpdateDescriptorSets (device, writes.size(), writes.data(), 0, NULL);
    }
}

void Renderer::flushDescriptorSetup() {
    createDescriptorPool();
    // for(int i=0; i<delayed_descriptor_setups.size(); i++) {
    for (auto setup : delayed_descriptor_setups) {
        if ((setup.sets->empty())) {
            allocate_Descriptor (*setup.sets, *setup.setLayout, descriptorPool, device);
        }
        setupDescriptor (setup.setLayout, setup.sets, setup.descriptions, setup.stages);
    }
}
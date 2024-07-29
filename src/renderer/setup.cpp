#include "render.hpp" 
#include <set>
#include <fstream>

using namespace std;
using namespace glm;

//they are global because its easier lol
const vector<const char*> instanceLayers = {
    #ifndef VKNDEBUG
    "VK_LAYER_KHRONOS_validation",
    #endif
    "VK_LAYER_LUNARG_monitor",
};
/*const*/vector<const char*> instanceExtensions = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    #ifndef VKNDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

    VK_KHR_SURFACE_EXTENSION_NAME,
    "VK_KHR_win32_surface",
};
const vector<const char*>   deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    // VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME
    // VK_NV_device_diagnostic_checkpoints
    // "VK_KHR_shader_non_semantic_info"
};

void Renderer::create_Command_Pool(){
    //TODO: could be saved after physical device choosen
    //btw TODO is ignored
    QueueFamilyIndices queueFamilyIndices = find_Queue_Families(physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicalAndCompute.value();

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool));
}

void Renderer::create_Command_Buffers(vector<VkCommandBuffer>* commandBuffers, u32 size){
    (*commandBuffers).resize(size);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (*commandBuffers).size();
    
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, (*commandBuffers).data())); 
}

void Renderer::create_Sync_Objects(){
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    raytraceFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    rayGenFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

       frameInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    graphicalInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
      raytraceInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &raytraceFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &rayGenFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL, &graphicalInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &raytraceInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,    &frameInFlightFences[i]));
    }
}

vector<char> read_Shader(const char* filename) {
    ifstream file(filename, ios::ate | ios::binary);
    assert(file.is_open());

    u32 fileSize = file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule Renderer::create_Shader_Module(vector<char>* code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo createInfo = {};

    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = (*code).size();
    createInfo.pCode = reinterpret_cast<const u32*>((*code).data());

    VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));

    return shaderModule;
}

void Renderer::create_RenderPass_Graphical(){
    VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; //final frame copied into swapchain image
        colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //to present

    VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //for drawing

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        dependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &overlayRenderPass));

}
void Renderer::create_RenderPass_RayGen(){
    VkAttachmentDescription caMatNorm = {};
        caMatNorm.format = VK_FORMAT_R8G8B8A8_SNORM;
        caMatNorm.samples = VK_SAMPLE_COUNT_1_BIT;
        caMatNorm.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        caMatNorm.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caMatNorm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caMatNorm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caMatNorm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        caMatNorm.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //for blit if scaled or 
    VkAttachmentDescription caOldUv = {};
        caOldUv.format = OLD_UV_FORMAT;
        caOldUv.samples = VK_SAMPLE_COUNT_1_BIT;
        caOldUv.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        caOldUv.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caOldUv.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caOldUv.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caOldUv.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        caOldUv.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 
    VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = DEPTH_FORMAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // depthAttachment.
    VkAttachmentReference MatNormRef = {};
        MatNormRef.attachment = 0;
        MatNormRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference oldUvRef = {};
        oldUvRef.attachment = 1;
        oldUvRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // VkAttachmentReference depthRef = {};
    //     depthRef.attachment = 2;
    //     depthRef.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vector<VkAttachmentDescription> attachments = {caMatNorm, caOldUv, /*caDepth,*/ depthAttachment};

    vector<VkAttachmentReference>   colorAttachmentRefs = {MatNormRef, oldUvRef, /*depthRef*/};

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // subpass.st
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &rayGenRenderPass));
}
void Renderer::create_RenderPass_RayGen_Particles(){
    VkAttachmentDescription caMatNorm = {};
        caMatNorm.format = VK_FORMAT_R8G8B8A8_SNORM;
        caMatNorm.samples = VK_SAMPLE_COUNT_1_BIT;
        caMatNorm.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        caMatNorm.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caMatNorm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caMatNorm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caMatNorm.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        caMatNorm.finalLayout = is_scaled? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription caOldUv = {};
        caOldUv.format = OLD_UV_FORMAT;
        caOldUv.samples = VK_SAMPLE_COUNT_1_BIT;
        caOldUv.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        caOldUv.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        caOldUv.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        caOldUv.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        caOldUv.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        caOldUv.finalLayout = is_scaled? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL; 
    VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = DEPTH_FORMAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = is_scaled? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        // depthAttachment.
    VkAttachmentReference MatNormRef = {};
        MatNormRef.attachment = 0;
        MatNormRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference oldUvRef = {};
        oldUvRef.attachment = 1;
        oldUvRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // VkAttachmentReference depthRef = {};
    //     depthRef.attachment = 2;
    //     depthRef.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vector<VkAttachmentDescription> attachments = {caMatNorm, caOldUv, /*caDepth,*/ depthAttachment};

    vector<VkAttachmentReference>   colorAttachmentRefs = {MatNormRef, oldUvRef, /*depthRef*/};

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // subpass.st
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &rayGenParticlesRenderPass));

}

void Renderer::destroy_Raster_Pipeline(RasterPipe* pipe){
    vkDestroyPipeline(device, pipe->pipe, NULL);
    vkDestroyPipelineLayout(device, pipe->pipeLayout, NULL);
    vkDestroyDescriptorSetLayout(device, pipe->dsetLayout, NULL);
}

void Renderer::create_Raster_Pipeline(RasterPipe* pipe, vector<ShaderStage> shader_stages, vector<AttrFormOffs> attr_desc, 
        u32 stride, VkVertexInputRate input_rate, VkPrimitiveTopology topology,
        VkExtent2D extent, vector<SimpleBlend> blends, u32 push_size, bool depthTest){
    
    // vector<vector<char>> shader_codes(shader_stages.size());
    vector<VkShaderModule > shader_modules(shader_stages.size());
    vector<VkPipelineShaderStageCreateInfo> shaderStages(shader_stages.size());

    for(int i=0; i<shader_stages.size(); i++){
        vector<char> shader_code = read_Shader(shader_stages[i].src);
        shader_modules[i] = create_Shader_Module(&shader_code);
        
        shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[i].stage = shader_stages[i].stage;
        shaderStages[i].module = shader_modules[i];
        shaderStages[i].pName = "main";
    }


    // VkPipelineShaderStageCreateInfo 
    //     vertShaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    //     vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    //     vertShaderStageInfo.module = vertShaderModule;
    //     vertShaderStageInfo.pName = "main";
    // VkPipelineShaderStageCreateInfo 
    //     fragShaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    //     fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    //     fragShaderStageInfo.module = fragShaderModule;
    //     fragShaderStageInfo.pName = "main";
    
    // vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};
    vector<VkVertexInputAttributeDescription> attributeDescriptions(attr_desc.size());
        for(int i=0; i<attr_desc.size(); i++){
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
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    VkPipelineInputAssemblyStateCreateInfo 
        inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport 
        viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) extent.width;
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
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

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
    for (int i=0; i<blends.size(); i++){
        //for now only two possible states
        if(blends[i].enabled){
            blendAttachments[i].blendEnable = VK_TRUE;
        } else {
            blendAttachments[i].blendEnable = VK_FALSE;
        }
        blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
    }
   
    VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = blendAttachments.size();
        colorBlending.pAttachments = blendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

    VkPushConstantRange pushRange = {};
        pushRange.size = push_size; //trans
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &pipe->dsetLayout; 
        if(push_size != 0){
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        } else {
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = NULL;
        }

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipe->pipeLayout));

    //if not depthTest then just not used. Done this way because same dept state used everywhere on not used at all
    VkPipelineDepthStencilStateCreateInfo
        depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};    
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};
    VkGraphicsPipelineCreateInfo 
        pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        if(depthTest){
            pipelineInfo.pDepthStencilState = &depthStencil;
        } else {
            pipelineInfo.pDepthStencilState = NULL;
        }
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        
        pipelineInfo.layout = pipe->pipeLayout;
        
        pipelineInfo.renderPass = pipe->rpass;
        pipelineInfo.subpass = pipe->subpass_id;
        
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipe->pipe));
    
    for(int i=0; i<shader_stages.size(); i++){
        vkDestroyShaderModule(device, shader_modules[i], NULL);
    }
}

void Renderer::create_Surface(){
    VK_CHECK(glfwCreateWindowSurface(instance, window.pointer, NULL, &surface));
}

QueueFamilyIndices Renderer::find_Queue_Families(VkPhysicalDevice device){
    QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    i32 i = 0;
    for (auto queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) and (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicalAndCompute = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

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

bool Renderer::check_PhysicalDevice_Extension_Support(VkPhysicalDevice device){
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data());

    //for time complexity? FOR CODE COMPLEXITY HAHAHA
    //does not apply to current code

    vector<const char*> requiredExtensions = deviceExtensions;

    for (auto req : requiredExtensions) {
        bool supports = false;
        for (auto avail : availableExtensions) {
            if(strcmp(req, avail.extensionName) == 0) {
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

SwapChainSupportDetails Renderer::query_Swapchain_Support(VkPhysicalDevice device){
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());

    return details;
}

bool Renderer::check_Format_Support(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device, format, &formatProps);

    return formatProps.optimalTilingFeatures & features;
}

bool Renderer::is_PhysicalDevice_Suitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = find_Queue_Families(device);
    SwapChainSupportDetails swapChainSupport;
    bool extensionsSupported = check_PhysicalDevice_Extension_Support(device);
    bool imageFormatSupport = check_Format_Support(
        device,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
    );
    // bool srgbFormatSupport = check_Format_Support(
    //     device,
    //     VK_FORMAT_R8G8B8A8_SRGB,
    //     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    // );

    if (extensionsSupported) {
        swapChainSupport  = query_Swapchain_Support(device);
    }

    return indices.isComplete() 
        && extensionsSupported 
        && swapChainSupport.is_Suitable() 
        && imageFormatSupport
    ;   
}

void Renderer::pick_Physical_Device(){
    u32 deviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    vector<VkPhysicalDevice> devices (deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    physicalDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        if (is_PhysicalDevice_Suitable(device)){
            physicalDevice = device;//could be moved out btw
            break;
        }
    }
    if(physicalDevice == VK_NULL_HANDLE){
        cout << KRED "no suitable physical device\n" KEND;
        exit(1);
    }
}

void Renderer::create_Logical_Device(){
    QueueFamilyIndices indices = find_Queue_Families(physicalDevice);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    set<u32> uniqueQueueFamilies = {indices.graphicalAndCompute.value(), indices.present.value()};

    float queuePriority = 1.0f; //random priority?
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
        #ifndef VKNDEBUG
        // deviceFeatures.robustBufferAccess = VK_TRUE;
        #endif
        deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        deviceFeatures.geometryShader = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pQueueCreateInfos    = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount   = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount   = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, NULL, &device));

    vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &graphicsQueue);
    // vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &computeQueue);
    vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);
}

VkSurfaceFormatKHR Renderer::choose_Swap_SurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats) {
    for (auto format : availableFormats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    cout << KYEL "Where is your RAYTRACED_IMAGE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR?\n" KEND;
    return availableFormats[0];
}

VkPresentModeKHR Renderer::choose_Swap_PresentMode(vector<VkPresentModeKHR> availablePresentModes) {
    for (auto mode : availablePresentModes) {
        if (
            ((mode == VK_PRESENT_MODE_FIFO_KHR) && is_vsync) ||
            ((mode == VK_PRESENT_MODE_MAILBOX_KHR) && !is_vsync)
            ) {
            return mode;}
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::choose_Swap_Extent(VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT_MAX) {
        return capabilities.currentExtent;
    } else {
        i32 width, height;
        glfwGetFramebufferSize(window.pointer, &width, &height);

        VkExtent2D actualExtent {(u32) width, (u32) height};

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Renderer::create_Allocator(){
    VmaAllocatorCreateInfo allocatorInfo = {};
    VmaVulkanFunctions vulkanFunctions;
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
    allocatorInfo.pVulkanFunctions = (const VmaVulkanFunctions*) &vulkanFunctions;
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &VMAllocator));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) printf(KCYN);
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) printf(KRED); 
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) printf(KBLU); 
    printf("validation layer: %s\n\n" KEND, pCallbackData->pMessage);
    return VK_FALSE;
}

void Renderer::setup_Debug_Messenger(){
    VkDebugUtilsMessengerEXT debugMessenger;
    
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, NULL, &debugMessenger));

    this->debugMessenger = debugMessenger;
}

void Renderer::get_Instance_Extensions(){
    u32 glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    assert(glfwExtensions!=NULL);
    assert(glfwExtCount>0);
    for(u32 i=0; i < glfwExtCount; i++){
        instanceExtensions.push_back(glfwExtensions[i]);
        printl(glfwExtensions[i]);
    }

    //actual verify part
    u32 supportedExtensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL));
    vector<VkExtensionProperties> supportedInstanceExtensions(supportedExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, supportedInstanceExtensions.data()));    

    for(auto ext : instanceExtensions){
        bool supported = false;
        for(auto sup : supportedInstanceExtensions){
            if(strcmp(ext, sup.extensionName) == 0) supported = true;
        }
        if(not supported) {
            cout << KRED << ext << " not supported" << KEND;
            exit(1);
        }
    }
}

static void framebuffer_Resize_Callback(GLFWwindow* window, int width, int height) {
    auto app = (Renderer*)(glfwGetWindowUserPointer(window));
    app->is_resized = true;
}
void Renderer::create_Window(){
    int glfwRes = glfwInit();
    assert(glfwRes != 0);
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    assert(mode!=NULL);
    window.width  = (mode->width  / 2) * 2;
    window.height = (mode->height / 2) * 2;
    
    if(is_fullscreen){
        window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", glfwGetPrimaryMonitor(), 0);
    } else {
        window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", 0, 0);
    }
    assert(window.pointer != NULL);

    glfwSetWindowUserPointer(window.pointer, this);
    glfwSetInputMode(window.pointer, GLFW_STICKY_KEYS         , GLFW_TRUE);
    glfwSetInputMode(window.pointer, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
    glfwSetFramebufferSizeCallback(window.pointer, framebuffer_Resize_Callback);
}

void Renderer::create_Instance(){
    VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Lol vulkan raytracer";
        app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
        app_info.pEngineName = "my vk engine";
        app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
        app_info.apiVersion = VK_API_VERSION_1_3;

    vector<VkValidationFeatureEnableEXT> enabledFeatures = {
        // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, 
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT features = {};
        features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        features.enabledValidationFeatureCount = enabledFeatures.size();
        features.pEnabledValidationFeatures    = enabledFeatures.data();

    VkInstanceCreateInfo createInfo = {};
    #ifndef VKNDEBUG 
        // createInfo.
        createInfo.pNext = &features; // according to spec

    #endif 
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info;
        createInfo.enabledExtensionCount   = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.enabledLayerCount   = instanceLayers.size();
        createInfo.ppEnabledLayerNames = instanceLayers.data();

        // createInfo.

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));
}
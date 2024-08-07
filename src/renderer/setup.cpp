#include "render.hpp" 
#include <set>
#include <fstream>

using namespace std;
using namespace glm;

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
    "VK_KHR_win32_surface",
};
const vector<const char*>   deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
    // VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME
    // VK_NV_device_diagnostic_checkpoints
    // "VK_KHR_shader_non_semantic_info"
};

void Renderer::create_Command_Pool(){
    //TODO: could be saved after physical device choosen
    //btw TODO is ignored
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

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
    // raytraceFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    // rayGenFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

       frameInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    // graphicalInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    //   raytraceInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &renderFinishedSemaphores[i]));
        // VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &raytraceFinishedSemaphores[i]));
        // VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &rayGenFinishedSemaphores[i]));
        // VK_CHECK(vkCreateFence(device, &fenceInfo, NULL, &graphicalInFlightFences[i]));
        // VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &raytraceInFlightFences[i]));
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

void Renderer::createRenderPass1(){
    VkAttachmentDescription 
        ca_mat_norm = {};
        ca_mat_norm.format = VK_FORMAT_R8G8B8A8_SNORM;
        ca_mat_norm.samples = VK_SAMPLE_COUNT_1_BIT;
        ca_mat_norm.loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_mat_norm.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        ca_mat_norm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_mat_norm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ca_mat_norm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ca_mat_norm.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription 
        ca_frame = {};
        ca_frame.format = VK_FORMAT_R16G16B16A16_UNORM;
        ca_frame.samples = VK_SAMPLE_COUNT_1_BIT;
        ca_frame.loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_frame.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        ca_frame.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_frame.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ca_frame.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ca_frame.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription 
        ca_depth = {};
        ca_depth.format = DEPTH_FORMAT;
        ca_depth.samples = VK_SAMPLE_COUNT_1_BIT;
        ca_depth.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ca_depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
        ca_depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ca_depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ca_depth.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        
    VkAttachmentReference aref_mat_norm = {};
        aref_mat_norm.attachment = 0;
        aref_mat_norm.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference aref_frame = {};
        aref_frame.layout = VK_IMAGE_LAYOUT_GENERAL;
        aref_frame.attachment = 1;
    VkAttachmentReference aref_depth = {};
        aref_depth.layout = VK_IMAGE_LAYOUT_GENERAL;
        aref_depth.attachment = 2;

    vector<VkAttachmentDescription> attachments = {ca_mat_norm, ca_frame, ca_depth};

    // vector<VkAttachmentReference> color_attachment_refs = {aref_mat_norm, aref_frame, /*depthRef*/};

    //uses vertices to produce mat_norm image
    const int SP_BLOCKS = 0;
    const int SP_PARTS = SP_BLOCKS+1;
    const int SP_GRASS = SP_PARTS+1;
    const int SP_WATER = SP_GRASS+1;
    const int SP_DIFFUSE = SP_WATER+1;
    const int SP_GLOSSY = SP_DIFFUSE+1;
    VkSubpassDescription 
        raygen_subpass = {};
        raygen_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        raygen_subpass.pDepthStencilAttachment = &aref_depth;
        raygen_subpass.colorAttachmentCount = 1;
        raygen_subpass.pColorAttachments = &aref_mat_norm;
    VkSubpassDescription 
        raygen_particles_subpass = {};
        raygen_particles_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        raygen_particles_subpass.pDepthStencilAttachment = &aref_depth; //Q
        raygen_particles_subpass.colorAttachmentCount = 1;
        raygen_particles_subpass.pColorAttachments = &aref_mat_norm;
    VkSubpassDescription 
        raygen_grass_subpass = {};
        raygen_grass_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        raygen_grass_subpass.pDepthStencilAttachment = &aref_depth; //Q
        raygen_grass_subpass.colorAttachmentCount = 1;
        raygen_grass_subpass.pColorAttachments = &aref_mat_norm;
    VkSubpassDescription 
        raygen_water_subpass = {};
        raygen_water_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        raygen_water_subpass.pDepthStencilAttachment = &aref_depth; //Q
        raygen_water_subpass.colorAttachmentCount = 1;
        raygen_water_subpass.pColorAttachments = &aref_mat_norm;
    //uses mat+norm for diffuse coloring via fullscreen triag
    //they both use depth, normal and material
    vector<VkAttachmentReference> diffuse_input_attachment_refs = {aref_mat_norm, aref_depth};
    VkSubpassDescription 
        diffuse_fs = {};
        diffuse_fs.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        //only writes to final frame
        diffuse_fs.colorAttachmentCount = 1;
        diffuse_fs.pColorAttachments = &aref_frame;
        //depth is in input cause we use it as "color", not for hw depth testing
        diffuse_fs.inputAttachmentCount = diffuse_input_attachment_refs.size();
        diffuse_fs.pInputAttachments = diffuse_input_attachment_refs.data();
    VkSubpassDescription 
        glossy_fs = {};
        glossy_fs.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        //only writes to final frame
        glossy_fs.colorAttachmentCount = 1;
        glossy_fs.pColorAttachments = &aref_frame;
        //depth is in input cause we use it as "color", not for hw depth testing
        glossy_fs.inputAttachmentCount = diffuse_input_attachment_refs.size();
        glossy_fs.pInputAttachments = diffuse_input_attachment_refs.data();

    VkSubpassDependency full_wait_color = {};
		full_wait_color.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		full_wait_color.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		full_wait_color.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		full_wait_color.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		full_wait_color.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    VkSubpassDependency wait = {};
		wait.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    VkSubpassDependency full_wait_depth = {};
        full_wait_depth.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        full_wait_depth.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        full_wait_depth.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        full_wait_depth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        full_wait_depth.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        
    vector<VkSubpassDependency> dependencies(4);
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dependencyFlags = 0;

        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = SP_PARTS;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		dependencies[1].dependencyFlags = 0;

		// This makes sure that writes to the depth image are done before we try to write to it again
		dependencies[2].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[2].dstSubpass = 0;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dependencyFlags = 0;

		dependencies[3].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[3].dstSubpass = 0;
		dependencies[3].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[3].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[3].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[3].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[3].dependencyFlags = 0;

        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_PARTS;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_GRASS;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_WATER;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_BLOCKS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_GRASS;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_WATER;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_PARTS; //mapping particles
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_PARTS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_GRASS;
        wait.dstSubpass = SP_WATER;
        wait.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;    //no waiting 
		wait.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; //no waiting
		wait.srcAccessMask = VK_ACCESS_NONE;
		wait.dstAccessMask = VK_ACCESS_NONE;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_GRASS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_GRASS;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);

        wait.srcSubpass = SP_GRASS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_GRASS;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);


        wait.srcSubpass = SP_WATER;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_WATER;
        wait.dstSubpass = SP_DIFFUSE;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        wait.srcSubpass = SP_WATER;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
        wait.srcSubpass = SP_WATER;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;


        wait.srcSubpass = SP_DIFFUSE;
        wait.dstSubpass = SP_GLOSSY;
        wait.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		wait.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		wait.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies.push_back(wait);
    //     wait.srcSubpass = SP_DIFFUSE;
    //     wait.dstSubpass = SP_GLOSSY;
    //     wait.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	// 	wait.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	// 	wait.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	// 	wait.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    // dependencies.push_back(wait);

    vector<VkSubpassDescription> subpasses = {raygen_subpass, raygen_particles_subpass, raygen_grass_subpass, raygen_water_subpass, diffuse_fs, glossy_fs};

    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = subpasses.size();
        createInfo.pSubpasses = subpasses.data();
        createInfo.dependencyCount = dependencies.size();
        createInfo.pDependencies = dependencies.data();
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &raygen2glossyRpass));
}
void Renderer::createRenderPass2(){
    VkAttachmentDescription 
        ca_mat_norm = {};
        ca_mat_norm.format = VK_FORMAT_R8G8B8A8_SNORM;
        ca_mat_norm.samples = VK_SAMPLE_COUNT_1_BIT;
        ca_mat_norm.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        ca_mat_norm.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
        ca_mat_norm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_mat_norm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ca_mat_norm.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        ca_mat_norm.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription 
        ca_frame = {};
        ca_frame.format = VK_FORMAT_R16G16B16A16_UNORM;
        ca_frame.samples = VK_SAMPLE_COUNT_1_BIT;
        ca_frame.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
        ca_frame.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        ca_frame.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ca_frame.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ca_frame.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        ca_frame.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference aref_mat_norm = {};
        aref_mat_norm.attachment = 0;
        aref_mat_norm.layout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference aref_frame = {};
        aref_frame.layout = VK_IMAGE_LAYOUT_GENERAL;
        aref_frame.attachment = 1;

    vector<VkAttachmentDescription> attachments = {ca_mat_norm, ca_frame};

    vector<VkAttachmentReference> color_attachment_refs = {aref_mat_norm, aref_frame, /*depthRef*/};

    //reads from mat_norm as just image and writes to final frame
    VkSubpassDescription
        blur_subpass = {};
        blur_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        blur_subpass.colorAttachmentCount = 1;
        blur_subpass.pColorAttachments = &aref_frame;
        blur_subpass.inputAttachmentCount = color_attachment_refs.size();
        blur_subpass.pInputAttachments = color_attachment_refs.data();
    VkSubpassDescription 
        overlay_subpass = {};
        overlay_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        overlay_subpass.colorAttachmentCount = 1;
        overlay_subpass.pColorAttachments = &aref_frame;

    VkSubpassDependency full_wait_color = {};
		full_wait_color.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		full_wait_color.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		full_wait_color.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		full_wait_color.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		full_wait_color.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		// This makes sure that writes to the depth image are done before we try to write to it again
    vector<VkSubpassDependency> dependencies(3);
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dependencyFlags = 0;

        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].dstSubpass = 0;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = 0;

        dependencies[2].srcSubpass = 0;
        dependencies[2].dstSubpass = 1;
        dependencies[2].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        dependencies[2].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_INDEX_READ_BIT;
        dependencies[2].dependencyFlags = 0;

    vector<VkSubpassDescription> subpasses = {blur_subpass, overlay_subpass};

    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = subpasses.size();
        createInfo.pSubpasses = subpasses.data();
        createInfo.dependencyCount = dependencies.size();
        createInfo.pDependencies = dependencies.data();
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &blur2presentRpass));
}

void Renderer::destroy_Raster_Pipeline(RasterPipe* pipe){
    vkDestroyPipeline(device, pipe->line, NULL);
    vkDestroyPipelineLayout(device, pipe->lineLayout, NULL);
    vkDestroyDescriptorSetLayout(device, pipe->setLayout, NULL);
}

void Renderer::create_Raster_Pipeline(RasterPipe* pipe, vector<ShaderStage> shader_stages, vector<AttrFormOffs> attr_desc, 
        u32 stride, VkVertexInputRate input_rate, VkPrimitiveTopology topology,
        VkExtent2D extent, vector<BlendAttachment> blends, u32 push_size, DepthTesting depthTest, VkCullModeFlags culling){
    
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
        if(attributeDescriptions.size() != 0){
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
    for (int i=0; i<blends.size(); i++){
        //for now only two possible states
        if(blends[i] == DO_BLEND){
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
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &pipe->setLayout; 
        if(push_size != 0){
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        } else {
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = NULL;
        }

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipe->lineLayout));

    //if not depthTest then just not used. Done this way because same dept state used everywhere on not used at all
    VkPipelineDepthStencilStateCreateInfo
        depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};    
        switch (depthTest) {
            case NO_TEST: {
                depthStencil.depthTestEnable = VK_FALSE;
                depthStencil.depthWriteEnable = VK_FALSE;
                break;
            }
            case DO_TEST: {
                depthStencil.depthTestEnable = VK_TRUE;
                depthStencil.depthWriteEnable = VK_TRUE;
                break;
            }
            case READ_TEST: {
                depthStencil.depthTestEnable = VK_TRUE;
                depthStencil.depthWriteEnable = VK_FALSE;
                break;
            }
            case WRITE_TEST: {
                depthStencil.depthTestEnable = VK_FALSE;
                depthStencil.depthWriteEnable = VK_TRUE;
                break;
            }
            default: crash(whats depthTest);
        }
        depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;
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
        if(depthTest == NO_TEST){
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

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipe->line));
    
    for(int i=0; i<shader_stages.size(); i++){
        vkDestroyShaderModule(device, shader_modules[i], NULL);
    }
}

void Renderer::destroy_Compute_Pipeline(ComputePipe* pipe){
    vkDestroyPipeline(device, pipe->line, NULL);
    vkDestroyPipelineLayout(device, pipe->lineLayout, NULL);
    vkDestroyDescriptorSetLayout(device, pipe->setLayout, NULL);
}
void Renderer::create_Compute_Pipeline(ComputePipe* pipe, const char* src, u32 push_size, VkPipelineCreateFlags create_flags){
    auto compShaderCode = read_Shader(src);

    VkShaderModule module = create_Shader_Module(&compShaderCode);
    
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

    VkPipelineLayoutCreateInfo 
        pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &pipe->setLayout;
        if(push_size != 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        }
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipe->lineLayout));

    VkComputePipelineCreateInfo 
        pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = compShaderStageInfo;
        pipelineInfo.layout = pipe->lineLayout;
        pipelineInfo.flags = create_flags;

    VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &pipe->line));
    vkDestroyShaderModule(device, module, NULL);
}


void Renderer::createSurface(){
    VK_CHECK(glfwCreateWindowSurface(instance, window.pointer, NULL, &surface));
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device){
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

bool Renderer::checkPhysicalDeviceExtensionSupport(VkPhysicalDevice device){
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

SwapChainSupportDetails Renderer::querySwapchainSupport(VkPhysicalDevice device){
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

bool Renderer::checkFormatSupport(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device, format, &formatProps);

    return formatProps.optimalTilingFeatures & features;
}

bool Renderer::isPhysicalDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    SwapChainSupportDetails swapChainSupport;
    bool extensionsSupported = checkPhysicalDeviceExtensionSupport(device);
    bool imageFormatSupport = checkFormatSupport(
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
        swapChainSupport  = querySwapchainSupport(device);
    }

    return indices.isComplete() 
        && extensionsSupported 
        && swapChainSupport.is_Suitable() 
        && imageFormatSupport
    ;   
}

void Renderer::pickPhysicalDevice(){
    u32 deviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    vector<VkPhysicalDevice> devices (deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    physicalDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        if (isPhysicalDeviceSuitable(device)){
            physicalDevice = device;//could be moved out btw
            break;
        }
    }
    if(physicalDevice == VK_NULL_HANDLE){
        cout << KRED "no suitable physical device\n" KEND;
        exit(1);
    }

    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
}

void Renderer::createLogicalDevice(){
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

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
        deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
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

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats) {
    for (auto format : availableFormats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    cout << KYEL "Where is your RAYTRACED_IMAGE_FORMAT VK_COLOR_SPACE_SRGB_NONLINEAR_KHR?\n" KEND;
    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(vector<VkPresentModeKHR> availablePresentModes) {
    for (auto mode : availablePresentModes) {
        if (
            ((mode == VK_PRESENT_MODE_FIFO_KHR) && vsync) ||
            ((mode == VK_PRESENT_MODE_MAILBOX_KHR) && !vsync)
            ) {
            return mode;}
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities) {
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

void Renderer::createAllocator(){
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

#ifndef VKNDEBUG
void Renderer::setupDebug_Messenger(){
    VkDebugUtilsMessengerEXT debugMessenger;
    
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, NULL, &debugMessenger));

    this->debugMessenger = debugMessenger;
}
#endif

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
    app->resized = true;
}
void Renderer::createWindow(){
    int glfwRes = glfwInit();
    assert(glfwRes != 0);
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    assert(mode!=NULL);
    window.width  = (mode->width  / 2) * 2;
    window.height = (mode->height / 2) * 2;
    
    if(fullscreen){
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

void Renderer::createInstance(){
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
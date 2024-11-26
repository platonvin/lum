#include "internal_render.hpp"
#include "defines/macros.hpp"

ring<Lumal::Image> LumInternal::LumInternalRenderer::create_RayTrace_VoxelImages (Voxel* voxels, ivec3 size) {
    VkDeviceSize bufferSize = (sizeof (Voxel)) * size.x * size.y * size.z;
    ring<Lumal::Image> voxelImages = {};
    lumal.createImageStorages (&voxelImages,
                           VK_IMAGE_TYPE_3D,
                           VK_FORMAT_R8_UINT,
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                           0, // no VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           Lumal::extent3d(size));
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        lumal.transitionImageLayoutSingletime (&voxelImages[i], VK_IMAGE_LAYOUT_GENERAL);
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
    vmaCreateBuffer (lumal.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);
    void* data;
    vmaMapMemory (lumal.VMAllocator, stagingAllocation, &data);
    memcpy (data, voxels, bufferSize);
    vmaUnmapMemory (lumal.VMAllocator, stagingAllocation);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        lumal.copyBufferSingleTime (stagingBuffer, &voxelImages[i], size);
    }
    vmaDestroyBuffer (lumal.VMAllocator, stagingBuffer, stagingAllocation);
    return voxelImages;
}

void LumInternal::LumInternalRenderer::createSamplers() {
    VkSamplerCreateInfo 
        samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &nearestSampler));
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &linearSampler));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &linearSampler_tiled));
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &overlaySampler));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &unnormLinear));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &unnormNearest));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    VK_CHECK (vkCreateSampler (lumal.device, &samplerInfo, NULL, &shadowSampler));
}
#include "internal_render.hpp"
#include "defines/macros.hpp"

// typedef struct UiDrawCall {
// InternalUiMesh mesh;
// vec2 shift;
// mat4 translate;
// } UiDrawCall;

// MyRenderInterface::MyRenderInterface(){}

// Called by RmlUi when it wants to render geometry that the application does not wish to optimise
// Called in active renderpass
void LumInternal::MyRenderInterface::RenderGeometry (Rml::Vertex* vertices,
                                        int num_vertices,
                                        int* indices,
                                        int num_indices,
                                        Rml::TextureHandle texture,
                                        const Rml::Vector2f& translation) {
    InternalUiMesh ui_mesh = {};
    VkDeviceSize bufferSizeV = sizeof (Rml::Vertex) * num_vertices;
    VkDeviceSize bufferSizeI = sizeof (int ) * num_indices;
    VkBufferCreateInfo 
        bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSizeV;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VmaAllocationCreateInfo 
        allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vmaCreateBuffer (render->render.VMAllocator, &bufferInfo, &allocInfo, &ui_mesh.vertexes.buffer, &ui_mesh.vertexes.alloc, NULL);
        bufferInfo.size = bufferSizeI;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vmaCreateBuffer (render->render.VMAllocator, &bufferInfo, &allocInfo, &ui_mesh.indexes.buffer, &ui_mesh.indexes.alloc, NULL);
    void* data;
    vmaMapMemory (render->render.VMAllocator, ui_mesh.vertexes.alloc, &data);
    memcpy (data, vertices, bufferSizeV);
    vmaUnmapMemory (render->render.VMAllocator, ui_mesh.vertexes.alloc);
    vmaMapMemory (render->render.VMAllocator, ui_mesh.indexes.alloc, &data);
    memcpy (data, indices, bufferSizeI);
    vmaUnmapMemory (render->render.VMAllocator, ui_mesh.indexes.alloc);
    VkCommandBuffer& renderCommandBuffer = render->graphicsCommandBuffers.current();
    VkBuffer vertexBuffers[] = {ui_mesh.vertexes.buffer};
    VkDeviceSize offsets[] = {0};
    struct {vec4 shift; mat4 trans;} ui_pc = {vec4 (translation.x, translation.y, render->render.swapChainExtent.width, render->render.swapChainExtent.height), current_transform};
    //TODO:
    vkCmdPushConstants (renderCommandBuffer, render->overlayPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (ui_pc), &ui_pc);
    Lumal::Image* texture_image = (Lumal::Image*)texture;
    VkDescriptorImageInfo
        textureInfo = {};
    if (texture_image != NULL) {
        textureInfo.imageView = texture_image->view; //CHANGE ME
    } else {
        textureInfo.imageView = default_image->view; //CHANGE ME
    }
        textureInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        textureInfo.sampler = render->overlaySampler;
    VkWriteDescriptorSet
        textureWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        textureWrite.dstSet = NULL;
        textureWrite.dstBinding = 0;
        textureWrite.dstArrayElement = 0;
        textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureWrite.descriptorCount = 1;
        textureWrite.pImageInfo = &textureInfo;
    vector<VkWriteDescriptorSet> descriptorWrites = {textureWrite};
    vkCmdPushDescriptorSetKHR (renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render->overlayPipe.lineLayout, 0, descriptorWrites.size(), descriptorWrites.data());
    vkCmdBindVertexBuffers (renderCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer (renderCommandBuffer, ui_mesh.indexes.buffer, 0, VK_INDEX_TYPE_UINT32);
    // render->render.cmdPipelineBarrier(renderCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_SHADER_STAGE_ALL_GRAPHICS, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_SHADER_STAGE_ALL_GRAPHICS);
    vkCmdDrawIndexed (renderCommandBuffer, ui_mesh.icount, 1, 0, 0, 0);
    Lumal::BufferDeletion vBufferDel = {ui_mesh.vertexes, MAX_FRAMES_IN_FLIGHT};
    Lumal::BufferDeletion iBufferDel = {ui_mesh.indexes, MAX_FRAMES_IN_FLIGHT};
    render->render.bufferDeletionQueue.push_back (vBufferDel);
    render->render.bufferDeletionQueue.push_back (iBufferDel);
}

// Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
Rml::CompiledGeometryHandle LumInternal::MyRenderInterface::CompileGeometry (Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture) {
    InternalUiMesh* ui_mesh = new InternalUiMesh;
    ui_mesh->image = (Lumal::Image*)texture;
    ui_mesh->icount = num_indices;
    // for(int i=0; i < num_vertices; i++){
    // printf("%3d tex: x%6f y%6f\n", i, vertices[i].tex_coord.x, vertices[i].tex_coord.y);
    // }
    VkDeviceSize bufferSizeV = sizeof (Rml::Vertex) * num_vertices;
    VkDeviceSize bufferSizeI = sizeof (int ) * num_indices;
    VkBufferCreateInfo 
        stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSizeV;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo 
        stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBufferV = {};
    VkBuffer stagingBufferI = {};
    VmaAllocation stagingAllocationV = {};
    VmaAllocation stagingAllocationI = {};
    VkBufferCreateInfo 
        bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSizeV;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vmaCreateBuffer (render->render.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferV, &stagingAllocationV, NULL);
    stagingBufferInfo.size = bufferSizeI;
    vmaCreateBuffer (render->render.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferI, &stagingAllocationI, NULL);
    vmaCreateBuffer (render->render.VMAllocator, &bufferInfo, &allocInfo, &ui_mesh->vertexes.buffer, &ui_mesh->vertexes.alloc, NULL);
    bufferInfo.size = bufferSizeI;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vmaCreateBuffer (render->render.VMAllocator, &bufferInfo, &allocInfo, &ui_mesh->indexes.buffer, &ui_mesh->indexes.alloc, NULL);
    void* data;
    vmaMapMemory (render->render.VMAllocator, stagingAllocationV, &data);
    memcpy (data, vertices, bufferSizeV);
    vmaUnmapMemory (render->render.VMAllocator, stagingAllocationV);
    vmaMapMemory (render->render.VMAllocator, stagingAllocationI, &data);
    memcpy (data, indices, bufferSizeI);
    vmaUnmapMemory (render->render.VMAllocator, stagingAllocationI);
    VkCommandBuffer& copyCommandBuffer = render->copyCommandBuffers.current();
    VkBufferCopy 
        staging_copy = {};
        staging_copy.size = bufferSizeV;
    vkCmdCopyBuffer (copyCommandBuffer, stagingBufferV, ui_mesh->vertexes.buffer, 1, &staging_copy);
        staging_copy.size = bufferSizeI;
    vkCmdCopyBuffer (copyCommandBuffer, stagingBufferI, ui_mesh->indexes.buffer, 1, &staging_copy);
    render->render.bufferDeletionQueue.push_back ({{stagingBufferV, stagingAllocationV}, MAX_FRAMES_IN_FLIGHT});
    render->render.bufferDeletionQueue.push_back ({{stagingBufferI, stagingAllocationI}, MAX_FRAMES_IN_FLIGHT});
    return Rml::CompiledGeometryHandle (ui_mesh);
}

// Called by RmlUi when it wants to render application-compiled geometry.
void LumInternal::MyRenderInterface::RenderCompiledGeometry (Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation) {
    VkCommandBuffer& renderCommandBuffer = render->graphicsCommandBuffers.current();
    InternalUiMesh* ui_mesh = (InternalUiMesh*)geometry;
    VkBuffer vertexBuffers[] = {ui_mesh->vertexes.buffer};
    VkDeviceSize offsets[] = {0};
    struct {vec4 shift; mat4 trans;} ui_pc = {vec4 (translation.x, translation.y, render->render.swapChainExtent.width, render->render.swapChainExtent.height), current_transform};
    //TODO:
    vkCmdPushConstants (renderCommandBuffer, render->overlayPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (ui_pc), &ui_pc);
    // vkCmdPushConstants(renderCommandBuffer, render->render.overlayPipe.pipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(rgpc), &rgpc);
    Lumal::Image* texture_image = ui_mesh->image;
// printl(texture_image);
    VkDescriptorImageInfo
    textureInfo = {};
    //if image is 0, then we just vind 1x1x1 white image
    if (texture_image != NULL) {
        textureInfo.imageView = texture_image->view; //CHANGE ME
    } else {
        textureInfo.imageView = default_image->view; //CHANGE ME
    }
    textureInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    textureInfo.sampler = render->overlaySampler;
    VkWriteDescriptorSet
    textureWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    textureWrite.dstSet = NULL;
    textureWrite.dstBinding = 0;
    textureWrite.dstArrayElement = 0;
    textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureWrite.descriptorCount = 1;
    textureWrite.pImageInfo = &textureInfo;
    vector<VkWriteDescriptorSet> descriptorWrites = {textureWrite};
    vkCmdPushDescriptorSetKHR (renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render->overlayPipe.lineLayout, 0, descriptorWrites.size(), descriptorWrites.data());
    vkCmdBindVertexBuffers (renderCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer (renderCommandBuffer, ui_mesh->indexes.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed (renderCommandBuffer, ui_mesh->icount, 1, 0, 0, 0);
}

// Called by RmlUi when it wants to release application-compiled geometry.
void LumInternal::MyRenderInterface::ReleaseCompiledGeometry (Rml::CompiledGeometryHandle geometry) {
    //we cant immediately relese it because previous frames use buffers for rendering (exept when 1 fif)
    //so we add it to deletion queue
    InternalUiMesh* mesh = (InternalUiMesh*)geometry;
    Lumal::BufferDeletion vBufferDel = {mesh->vertexes, MAX_FRAMES_IN_FLIGHT};
    Lumal::BufferDeletion iBufferDel = {mesh->indexes, MAX_FRAMES_IN_FLIGHT};
    render->render.bufferDeletionQueue.push_back (vBufferDel);
    render->render.bufferDeletionQueue.push_back (iBufferDel);
}

// Called by RmlUi when it wants to enable or disable scissoring to clip content.
void LumInternal::MyRenderInterface::EnableScissorRegion (bool enable) {
    VkCommandBuffer& renderCommandBuffer = render->graphicsCommandBuffers.current();
    if (true) {
        //do nothing...
        return;
    } else {
        VkRect2D full_scissors = {};
        full_scissors.offset = {0, 0};
        full_scissors.extent = render->render.swapChainExtent;
        vkCmdSetScissor (renderCommandBuffer, 0, 1, &full_scissors);
    }
}

// Called by RmlUi when it wants to change the scissor region.
void LumInternal::MyRenderInterface::SetScissorRegion (int x, int y, int width, int height) {
    VkCommandBuffer& renderCommandBuffer = render->graphicsCommandBuffers.current();
    last_scissors = {};
    last_scissors.offset = {std::clamp (x, 0, int (render->render.swapChainExtent.width - 1)),
                            std::clamp (y, 0, int (render->render.swapChainExtent.height - 1))
                           };
    last_scissors.extent = { (u32)std::clamp (width, 0, int (render->render.swapChainExtent.width - 1)),
                             (u32)std::clamp (height, 0, int (render->render.swapChainExtent.height - 1))
                           };
    vkCmdSetScissor (renderCommandBuffer, 0, 1, &last_scissors);
}

#pragma pack(1)
struct TGAHeader {
    char idLength;
    char colourMapType;
    char dataType;
    short int colourMapOrigin;
    short int colourMapLength;
    char colourMapDepth;
    short int xOrigin;
    short int yOrigin;
    short int width;
    short int height;
    char bitsPerPixel;
    char imageDescriptor;
};

// Restore packing
#pragma pack()

std::vector<char> readFileBuffer(const std::string& asset_name);
// Called by RmlUi when a texture is required by the library.
bool LumInternal::MyRenderInterface::LoadTexture (Rml::TextureHandle& texture_handle,
                                     Rml::Vector2i& texture_dimensions,
                                     const Rml::String& source) {
    size_t bsize;
    auto buffer = readFileBuffer (source.c_str());
    Rml::byte* bytes = (Rml::byte*) buffer.data();
    TGAHeader header = {};
    memcpy (&header, bytes, sizeof (TGAHeader));
    int color_mode = header.bitsPerPixel / 8;
    const size_t image_size = header.width * header.height * 4; // We always make 32bit textures
    if (header.dataType != 2) {
        Rml::Log::Message (Rml::Log::LT_ERROR, "Only 24/32bit uncompressed TGAs are supported.");
        return false;
    }
    // Ensure we have at least 3 colors
    if (color_mode < 3) {
        Rml::Log::Message (Rml::Log::LT_ERROR, "Only 24 and 32bit textures are supported.");
        return false;
    }
    Rml::byte* image_src = bytes + sizeof (TGAHeader);
    Rml::byte* image_dest_buffer = new Rml::byte[image_size];
    // Rml::byte* image_dest = image_dest_buffer;
    // Targa is BGR, swap to RGB, flip Y axis, and convert to premultiplied alpha.
    for (long y = 0; y < header.height; y++) {
        long read_index = y * header.width * color_mode;
        long write_index = ((header.imageDescriptor & 32) != 0) ? read_index : (header.height - y - 1) * header.width * 4;
        for (long x = 0; x < header.width; x++) {
            image_dest_buffer[write_index] = image_src[read_index + 2];
            image_dest_buffer[write_index + 1] = image_src[read_index + 1];
            image_dest_buffer[write_index + 2] = image_src[read_index];
            if (color_mode == 4) {
                const Rml::byte alpha = image_src[read_index + 3];
                for (size_t j = 0; j < 3; j++) {
                    image_dest_buffer[write_index + j] = Rml::byte ((image_dest_buffer[write_index + j] * alpha) / 255);
                }
                image_dest_buffer[write_index + 3] = alpha;
            } else {
                image_dest_buffer[write_index + 3] = 255;
            }
            write_index += 4;
            read_index += color_mode;
        }
    }
    texture_dimensions.x = header.width;
    texture_dimensions.y = header.height;
    delete[] image_dest_buffer;

    bool res = GenerateTexture (texture_handle, image_dest_buffer, texture_dimensions);
    return res;
}

// Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
bool LumInternal::MyRenderInterface::GenerateTexture (Rml::TextureHandle& texture_handle,
        const Rml::byte* source,
        const Rml::Vector2i& source_dimensions) {
    Lumal::Image* texture_image = new Lumal::Image;
    VkDeviceSize bufferSize = (sizeof (Rml::byte) * 4) * source_dimensions.x * source_dimensions.y;
    assert (bufferSize != 0);
    // if(bufferSize == 0) return false;
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
    assert (render->render.VMAllocator);
    assert (bufferSize != 0);
    // printl(bufferSize);
    // printl(source_dimensions.x);
    // printl(source_dimensions.y);
    VK_CHECK (vmaCreateBuffer (render->render.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL));
    void* data = NULL;
    VK_CHECK (vmaMapMemory (render->render.VMAllocator, stagingAllocation, &data));
    memcpy (data, source, bufferSize);
    // vmaFlushAllocation(render->render.VMAllocator, stagingAllocation, 0, 0);
    vmaUnmapMemory (render->render.VMAllocator, stagingAllocation);
    VkImageCreateInfo 
        imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.extent.width = source_dimensions.x;
        imageInfo.extent.height = source_dimensions.y;
        imageInfo.extent.depth = 1;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo imageAllocInfo = {};
        imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VK_CHECK (vmaCreateImage (render->render.VMAllocator, &imageInfo, &imageAllocInfo, &texture_image->image, &texture_image->alloc, NULL));
    VkImageViewCreateInfo 
        viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture_image->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK (vkCreateImageView (render->render.device, &viewInfo, NULL, &texture_image->view));
    VkImageMemoryBarrier 
        barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture_image->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_NONE;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        texture_image->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkCommandBuffer& copyCommandBuffer = render->copyCommandBuffers.current();
    render->render.cmdExplicitTransLayoutBarrier (copyCommandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_ACCESS_NONE, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                   texture_image);
    VkBufferImageCopy 
        staging_copy = {};
        staging_copy.imageExtent = { (u32)source_dimensions.x, (u32)source_dimensions.y, 1};
        staging_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        staging_copy.imageSubresource.baseArrayLayer = 0;
        staging_copy.imageSubresource.mipLevel = 0;
        staging_copy.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage (copyCommandBuffer, stagingBuffer, texture_image->image, VK_IMAGE_LAYOUT_GENERAL, 1, &staging_copy);
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // render->render.cmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier);
    render->render.cmdExplicitTransLayoutBarrier (copyCommandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                   texture_image);
    render->render.bufferDeletionQueue.push_back ({{stagingBuffer, stagingAllocation}, MAX_FRAMES_IN_FLIGHT});
    texture_handle = Rml::TextureHandle (texture_image);
    return true;
}

// Called by RmlUi when a loaded texture is no longer required.
void LumInternal::MyRenderInterface::ReleaseTexture (Rml::TextureHandle texture_handle) {
    render->render.imageDeletionQueue.push_back ({ * ((Lumal::Image*)texture_handle), MAX_FRAMES_IN_FLIGHT});
}

// Called by RmlUi when it wants the renderer to use a new transform matrix.
// If no transform applies to the current element, nullptr is submitted. Then it expects the renderer to use
// an identity matrix or otherwise omit the multiplication with the transform.
void LumInternal::MyRenderInterface::SetTransform (const Rml::Matrix4f* transform) {
    current_transform = (* (mat4*) (transform));
}

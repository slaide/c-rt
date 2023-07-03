#include "app/app.h"
#include "app/error.h"
#include "vulkan/vulkan_core.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief get string representation of VkResult value
 * 
 * @param res 
 * @return const char* 
 */
const char* vkRes2str(VkResult res){
    #define CASE(VAL) case(VAL): return #VAL;

    switch(res){
        CASE(VK_SUCCESS)

        CASE(VK_NOT_READY)
        CASE(VK_TIMEOUT)
        CASE(VK_EVENT_SET)
        CASE(VK_EVENT_RESET)
        CASE(VK_INCOMPLETE)
        CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
        CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
        CASE(VK_ERROR_INITIALIZATION_FAILED)
        CASE(VK_ERROR_DEVICE_LOST)
        CASE(VK_ERROR_MEMORY_MAP_FAILED)
        CASE(VK_ERROR_LAYER_NOT_PRESENT)
        CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
        CASE(VK_ERROR_FEATURE_NOT_PRESENT)
        CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
        CASE(VK_ERROR_TOO_MANY_OBJECTS)
        CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
        CASE(VK_ERROR_FRAGMENTED_POOL)
        CASE(VK_ERROR_UNKNOWN)

        // Provided by VK_VERSION_1_1
        CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
        // Provided by VK_VERSION_1_1
        CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
        // Provided by VK_VERSION_1_2
        CASE(VK_ERROR_FRAGMENTATION)
        // Provided by VK_VERSION_1_2
        CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
        // Provided by VK_VERSION_1_3
        CASE(VK_PIPELINE_COMPILE_REQUIRED)
        // Provided by VK_KHR_surface
        CASE(VK_ERROR_SURFACE_LOST_KHR)
        // Provided by VK_KHR_surface
        CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
        // Provided by VK_KHR_swapchain
        CASE(VK_SUBOPTIMAL_KHR)
        // Provided by VK_KHR_swapchain
        CASE(VK_ERROR_OUT_OF_DATE_KHR)
        // Provided by VK_KHR_display_swapchain
        CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
        // Provided by VK_EXT_debug_report
        CASE(VK_ERROR_VALIDATION_FAILED_EXT)
        // Provided by VK_NV_glsl_shader
        CASE(VK_ERROR_INVALID_SHADER_NV)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)
        // Provided by VK_KHR_video_queue
        CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)
        // Provided by VK_EXT_image_drm_format_modifier
        CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
        // Provided by VK_KHR_global_priority
        CASE(VK_ERROR_NOT_PERMITTED_KHR)
        // Provided by VK_EXT_full_screen_exclusive
        CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        // Provided by VK_KHR_deferred_host_operations
        CASE(VK_THREAD_IDLE_KHR)
        // Provided by VK_KHR_deferred_host_operations
        CASE(VK_THREAD_DONE_KHR)
        // Provided by VK_KHR_deferred_host_operations
        CASE(VK_OPERATION_DEFERRED_KHR)
        // Provided by VK_KHR_deferred_host_operations
        CASE(VK_OPERATION_NOT_DEFERRED_KHR)

        default:
            return "(unimplemented result)";
    }
}

/**
 * @brief get string representation of physical device type
 * 
 * @param device_type 
 * @return const char* NULL on invalid or unknown device type
 */
const char* device_type_name(int device_type){
    switch(device_type){
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "VK_PHYSICAL_DEVICE_TYPE_CPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "VK_PHYSICAL_DEVICE_TYPE_OTHER";
    }

    return NULL;
}

VertexData mesh[4]={
    {
        -0.7f, -0.7f, 0.0f, 1.0f,
        1.0f, 1.0f
    },
    {
        -0.7f, 0.7f, 0.0f, 1.0f,
        1.0f, 0.0f
    },
    {
        0.7f, -0.7f, 0.0f, 1.0f,
        0.0f, 1.0f
    },
    {
        0.7f, 0.7f, 0.0f, 1.0f,
        0.0f, 0.0f
    }
};

struct Texture{
    VkImage image;
    VkDeviceMemory image_memory;
    VkImageView image_view;
};
/**
 * @brief create texture fit for supplied image data (does not upload data to gpu!)
 * 
 * @param app 
 * @param image_data 
 * @return Texture* 
 */
Texture* App_create_texture(Application* app, ImageData* image_data){
    discard image_data;

    Texture* texture=malloc(sizeof(Texture));

    VkImageCreateInfo image_create_info={
        .sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .imageType=VK_IMAGE_TYPE_2D,
        .format=app->swapchain_format.format,
        .extent={
            .width=image_data->width,
            .height=image_data->height,
            .depth=1
        },
        .mipLevels=1,
        .arrayLayers=1,
        .samples=VK_SAMPLE_COUNT_1_BIT,
        .tiling=VK_IMAGE_TILING_OPTIMAL,
        .usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkResult res=vkCreateImage(app->device,&image_create_info,app->vk_allocator,&texture->image);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create image\n");
        exit(VULKAN_CREATE_IMAGE_FAILURE);
    }

    VkMemoryRequirements image_memory_requirements;
    vkGetImageMemoryRequirements(app->device, texture->image, &image_memory_requirements);

    uint32_t image_memory_type_index=UINT32_MAX;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(app->physical_device,&physical_device_memory_properties);
    for(uint32_t i=0;i<physical_device_memory_properties.memoryTypeCount;i++){
        if(image_memory_requirements.memoryTypeBits&(1<<i)){
            image_memory_type_index=i;
        }
    }
    if(image_memory_type_index==UINT32_MAX){exit(FATAL_UNEXPECTED_ERROR);}

    VkMemoryAllocateInfo image_memory_allocate_info={
        .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext=NULL,
        .allocationSize=image_memory_requirements.size,
        .memoryTypeIndex=image_memory_type_index
    };
    res=vkAllocateMemory(app->device, &image_memory_allocate_info, app->vk_allocator, &texture->image_memory);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate image memory\n");
        exit(VULKAN_ALLOCATE_MEMORY_FAILURE);
    }
    res=vkBindImageMemory(app->device, texture->image, texture->image_memory, 0);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to bind image memory\n");
        exit(VULKAN_BIND_IMAGE_MEMORY_FAILURE);
    }

    VkImageSubresourceRange image_subresource_range={
        .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel=0,
        .levelCount=1,
        .baseArrayLayer=0,
        .layerCount=1
    };
    VkImageViewCreateInfo image_view_create_info={
        .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .image=texture->image,
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=app->swapchain_format.format,
        .components={
            .r=VK_COMPONENT_SWIZZLE_IDENTITY,
            .g=VK_COMPONENT_SWIZZLE_IDENTITY,
            .b=VK_COMPONENT_SWIZZLE_IDENTITY,
            .a=VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange=image_subresource_range
    };
    res=vkCreateImageView(app->device,&image_view_create_info,app->vk_allocator,&texture->image_view);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create image view\n");
        exit(VULKAN_CREATE_IMAGE_VIEW_FAILURE);
    }

    VkSamplerCreateInfo sampler_create_info={
        .sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .magFilter=VK_FILTER_NEAREST,
        .minFilter=VK_FILTER_NEAREST,
        .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias=0.0,
        .anisotropyEnable=VK_FALSE,
        .maxAnisotropy=0.0,
        .compareEnable=VK_FALSE,
        .compareOp=VK_COMPARE_OP_NEVER,
        .minLod=1.0,
        .maxLod=1.0,
        .borderColor=VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates=VK_FALSE
    };
    res=vkCreateSampler(app->device, &sampler_create_info, app->vk_allocator, &app->shader->image_sampler);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create sampler\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }
    VkDescriptorImageInfo descriptor_image_info={
        .sampler=app->shader->image_sampler,
        .imageView=texture->image_view,
        .imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet write_descriptor_set={
        .sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext=NULL,
        .dstSet=app->shader->descriptor_set,
        .dstBinding=0,
        .dstArrayElement=0,
        .descriptorCount=1,
        .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo=&descriptor_image_info,
        .pBufferInfo=NULL,
        .pTexelBufferView=NULL
    };
    vkUpdateDescriptorSets(app->device, 1, &write_descriptor_set, 0, NULL);

    return texture;
}
void App_upload_texture(
    Application* app,
    Texture* texture,
    ImageData* image_data,
    VkCommandBuffer recording_command_buffer
){
    VkCommandBuffer command_buffer=recording_command_buffer;

    vkCmdPipelineBarrier(
        command_buffer, 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 
        0, 
        0, NULL, 
        0, NULL, 
        1, (VkImageMemoryBarrier[1]){{
            .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext=NULL,
            .srcAccessMask=0,  
            .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,  
            .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,  
            .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
            .image=texture->image, 
            {
                .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel=0,
                .levelCount=1,
                .baseArrayLayer=0,
                .layerCount=1 
            }
        }}
    );

    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(app->physical_device,&physical_device_properties);

    VkDeviceSize image_memory_size=image_data->height*image_data->width*4;
    image_memory_size=ROUND_UP(image_memory_size, physical_device_properties.limits.nonCoherentAtomSize);

    void* staging_buffer_cpu_memory;
    VkResult res=vkMapMemory(app->device, app->staging_buffer_memory, app->staging_buffer_size-app->staging_buffer_size_remaining, image_memory_size, 0, &staging_buffer_cpu_memory);
    if (res!=VK_SUCCESS) {
        fprintf(stderr,"failed to map memory\n");
        exit(VULKAN_MAP_MEMORY_FAILURE);
    }

    memcpy(staging_buffer_cpu_memory,image_data->data,image_memory_size);

    VkMappedMemoryRange flush_memory_range={
        .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext=NULL,
        .memory=app->staging_buffer_memory,
        .offset=0,
        .size=image_memory_size
    };
    res=vkFlushMappedMemoryRanges(app->device, 1, &flush_memory_range);
    if (res!=VK_SUCCESS) {
        fprintf(stderr,"failed to flush mapped memory ranges\n");
        exit(VULKAN_FLUSH_MAPPED_MEMORY_RANGES_FAILURE);
    }

    vkUnmapMemory(app->device, app->staging_buffer_memory);

    VkBufferImageCopy buffer_image_copy={
        .bufferOffset=0,
        .bufferRowLength=0,
        .bufferImageHeight=0,
        .imageSubresource={
            .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel=0,
            .baseArrayLayer=0,
            .layerCount=1,
        },
        .imageOffset={.x=0,.y=0,.z=0},
        .imageExtent={.width=image_data->width,.height=image_data->height,.depth=1}
    };
    vkCmdCopyBufferToImage(command_buffer, app->staging_buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    // TODO copy data to image
    vkCmdPipelineBarrier(
        command_buffer, 
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        0, 
        0, NULL, 
        0, NULL, 
        1, (VkImageMemoryBarrier[1]){{
            .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext=NULL,
            .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,  
            .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,  
            .oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  
            .newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
            .image=texture->image, 
            {
                .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel=0,
                .levelCount=1,
                .baseArrayLayer=0,
                .layerCount=1 
            }
        }}
    );
}
void App_destroy_texture(Application* app, Texture* texture){
    vkDestroyImageView(app->device,texture->image_view,app->vk_allocator);
    vkDestroyImage(app->device,texture->image,app->vk_allocator);
    vkFreeMemory(app->device,texture->image_memory,app->vk_allocator);

    free(texture);
}

VkShaderModule App_create_shader_module(Application* app,const char* shader_file_path){
    FILE* shader_file=fopen(shader_file_path,"rb");
    if(shader_file==NULL){
        fprintf(stderr,"failed to open shader file %s\n",shader_file_path);
        exit(VULKAN_SHADER_FILE_NOT_FOUND);
    }

    discard fseek(shader_file,0,SEEK_END);
    int shader_size=ftell(shader_file);
    rewind(shader_file);

    uint32_t* shader_code=malloc(shader_size);
    fread(shader_code, 1, shader_size, shader_file);

    fclose(shader_file);

    VkShaderModuleCreateInfo shader_module_create_info={
        .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .codeSize=shader_size,
        .pCode=shader_code
    };

    VkShaderModule shader;
    vkCreateShaderModule(app->device, &shader_module_create_info, app->vk_allocator, &shader);

    free(shader_code);

    return shader;
}
Shader* App_create_shader(
    Application* app,

    uint32_t subpass,
    uint32_t subpass_num_attachments
){
    Shader* shader=malloc(sizeof(Shader));

    shader->app=app;

    shader->fragment_shader=App_create_shader_module(app, "shaders/frag.spv");
    shader->vertex_shader=App_create_shader_module(app, "shaders/vert.spv");

    VkDescriptorSetLayoutBinding set_binding={
        .binding=0,
        .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount=1,
        .stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers=NULL
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .bindingCount=1,
        .pBindings=&set_binding
    };
    VkResult res=vkCreateDescriptorSetLayout(app->device, &descriptor_set_layout, app->vk_allocator, &shader->set_layout);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create descriptor set layout\n");
        exit(VULKAN_CREATE_DESCRIPTOR_SET_LAYOUT_FAILURE);
    }

    VkDescriptorPoolSize pool_sizes[1]={{
        .descriptorCount=1,
        .type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    }};
    VkDescriptorPoolCreateInfo descriptor_pool_create_info={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .maxSets=1,
        .poolSizeCount=1,
        .pPoolSizes=pool_sizes
    };
    res=vkCreateDescriptorPool(app->device, &descriptor_pool_create_info, app->vk_allocator, &shader->descriptor_pool);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create descriptor pool\n");
        exit(VULKAN_CREATE_DESCRIPTOR_POOL_FAILURE);
    }
    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext=NULL,
        .descriptorPool=shader->descriptor_pool,
        .descriptorSetCount=1,
        .pSetLayouts=&shader->set_layout
    };
    res=vkAllocateDescriptorSets(app->device, &descriptor_set_allocate_info, &shader->descriptor_set);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate descriptor sets\n");
        exit(VULKAN_ALLOCATE_DESCRIPTOR_SETS_FAILURE);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .setLayoutCount=1,
        .pSetLayouts=&shader->set_layout,
        .pushConstantRangeCount=0,
        .pPushConstantRanges=NULL
    };
    vkCreatePipelineLayout(app->device, &pipeline_layout_create_info, app->vk_allocator, &shader->pipeline_layout);

    VkVertexInputBindingDescription vertex_input_binding_descriptions[1]={{
        .binding=0,
        .stride=sizeof(VertexData),
        .inputRate=VK_VERTEX_INPUT_RATE_VERTEX
    }};
    VkVertexInputAttributeDescription vertex_input_attribute_descriptions[2]={
        {
            .location=0,
            .binding=vertex_input_binding_descriptions[0].binding,
            .format=VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset=offsetof(struct VertexData, x)
        },
        {
            .location=1,
            .binding=vertex_input_binding_descriptions[0].binding,
            .format=VK_FORMAT_R32G32_SFLOAT,
            .offset=offsetof( struct VertexData, u)
        }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .vertexBindingDescriptionCount=1,
        .pVertexBindingDescriptions=vertex_input_binding_descriptions,
        .vertexAttributeDescriptionCount=2,
        .pVertexAttributeDescriptions=vertex_input_attribute_descriptions
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable=VK_FALSE
    };
    VkPipelineViewportStateCreateInfo viewport_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .viewportCount=1,
        .pViewports=(VkViewport[1]){{
            .x=0,
            .y=0,
            .width=640,
            .height=480,
            .minDepth=0.0,
            .maxDepth=1.0
        }},
        .scissorCount=1,
        .pScissors=(VkRect2D[1]){{
            .offset={.x=0,.y=0},
            .extent={.height=480,.width=640},
        }}
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthClampEnable=VK_FALSE,
        .rasterizerDiscardEnable=VK_FALSE,
        .polygonMode=VK_POLYGON_MODE_FILL,
        .cullMode=VK_CULL_MODE_BACK_BIT,
        .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable=VK_FALSE,
        .depthBiasConstantFactor=0.0,
        .depthBiasClamp=0.0,
        .depthBiasSlopeFactor=0.0,
        .lineWidth=1.0
    };
    VkPipelineMultisampleStateCreateInfo multisample_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.0,
        .pSampleMask=NULL,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    };
    VkPipelineColorBlendAttachmentState subpass_attachment_color_blend_states={
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .logicOpEnable=VK_FALSE,
        .logicOp=VK_LOGIC_OP_COPY,
        .attachmentCount=subpass_num_attachments,
        .pAttachments=&subpass_attachment_color_blend_states,
        .blendConstants={0.0,0.0,0.0,0.0}
    };
    VkPipelineDynamicStateCreateInfo dynamic_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .dynamicStateCount=0,
        .pDynamicStates=NULL
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info={
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .stageCount=2,
        .pStages=(VkPipelineShaderStageCreateInfo[2]){
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //VkStructureType
                .pNext=NULL, //const void*
                .flags=0, //VkPipelineShaderStageCreateFlags
                .stage=VK_SHADER_STAGE_VERTEX_BIT, //VkShaderStageFlagBits
                .module=shader->vertex_shader, //VkShaderModule
                .pName="main", //const char*
                .pSpecializationInfo=NULL, //const VkSpecializationInfo*
            },
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //VkStructureType
                .pNext=NULL, //const void*
                .flags=0, //VkPipelineShaderStageCreateFlags
                .stage=VK_SHADER_STAGE_FRAGMENT_BIT, //VkShaderStageFlagBits
                .module=shader->fragment_shader, //VkShaderModule
                .pName="main", //const char*
                .pSpecializationInfo=NULL, //const VkSpecializationInfo*
            }
        },
        .pVertexInputState=&vertex_input_state,
        .pInputAssemblyState=&input_assembly_state,
        .pTessellationState=NULL,
        .pViewportState=&viewport_state,
        .pRasterizationState=&rasterization_state,
        .pMultisampleState=&multisample_state,
        .pDepthStencilState=NULL,
        .pColorBlendState=&color_blend_state,
        .pDynamicState=&dynamic_state,
        .layout=shader->pipeline_layout,
        .renderPass=app->render_pass,
        .subpass=subpass,
        .basePipelineHandle=NULL,
        .basePipelineIndex=-1
    };
    vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, app->vk_allocator, &shader->pipeline);

    return shader;
}
void App_destroy_shader(Shader* shader){
    vkDestroyDescriptorPool(shader->app->device, shader->descriptor_pool, shader->app->vk_allocator);
    vkDestroyDescriptorSetLayout(shader->app->device, shader->set_layout, shader->app->vk_allocator);

    vkDestroyPipeline(shader->app->device,shader->pipeline,shader->app->vk_allocator);
    vkDestroyPipelineLayout(shader->app->device, shader->pipeline_layout, shader->app->vk_allocator);

    vkDestroyShaderModule(shader->app->device,shader->fragment_shader,shader->app->vk_allocator);
    vkDestroyShaderModule(shader->app->device,shader->vertex_shader,shader->app->vk_allocator);

    free(shader);
}

/// create a new app
///
/// this struct owns all memory, unless indicated otherwise
Application* App_new(PlatformHandle* platform){
    printf("started creating app\n");
    Application* app=malloc(sizeof(Application));

    app->vk_allocator=NULL;
    app->platform_handle=platform;

    {
        uint32_t num_instance_extensions;
        vkEnumerateInstanceExtensionProperties(NULL, &num_instance_extensions, NULL);
        VkExtensionProperties* instance_extensions=malloc(num_instance_extensions*sizeof(VkExtensionProperties));
        vkEnumerateInstanceExtensionProperties(NULL, &num_instance_extensions, instance_extensions);

        uint32_t num_instance_layers;
        vkEnumerateInstanceLayerProperties(&num_instance_layers, NULL);
        VkLayerProperties* layer_extensions=malloc(num_instance_layers*sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(&num_instance_layers, layer_extensions);

        for(uint32_t i=0;i<num_instance_extensions;i++){
            printf("instance extension: %s\n",instance_extensions[i].extensionName);
        }
        for(uint32_t i=0;i<num_instance_layers;i++){
            printf("instance layer: %s\n",layer_extensions[i].layerName);
        }

        free(instance_extensions);
        free(layer_extensions);
    }

    uint32_t create_instance_flags=0;
    #ifdef VK_USE_PLATFORM_METAL_EXT
        create_instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif

    const char* instance_layers[1]={
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t num_instance_layers=1;

    const char* instance_extensions[]={
        "VK_KHR_surface",

        #ifdef VK_USE_PLATFORM_XCB_KHR
            "VK_KHR_xcb_surface"
        #elifdef VK_USE_PLATFORM_METAL_EXT
            "VK_EXT_metal_surface",
            "VK_KHR_portability_enumeration",
            "VK_KHR_get_physical_device_properties2"
        #endif
    };
    uint32_t num_instance_extensions=1;
    #ifdef VK_USE_PLATFORM_XCB_KHR
        num_instance_extensions+=1;
    #elifdef VK_USE_PLATFORM_METAL_EXT
        num_instance_extensions+=3;
    #endif

    VkApplicationInfo application_info={
        .sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext=NULL,
        .pApplicationName="my application",
        .applicationVersion=VK_MAKE_VERSION(0, 1, 0),
        .pEngineName="my engine",
        .engineVersion=VK_MAKE_VERSION(0, 1, 0),
        .apiVersion=VK_API_VERSION_1_0
    };
    VkInstanceCreateInfo instance_create_info={
        .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext=NULL,
        .flags=create_instance_flags,
        .pApplicationInfo=&application_info,
        .enabledLayerCount=num_instance_layers,
        .ppEnabledLayerNames=instance_layers,
        .enabledExtensionCount=num_instance_extensions,
        .ppEnabledExtensionNames=instance_extensions
    };
    VkResult res=vkCreateInstance(&instance_create_info,app->vk_allocator,&app->instance);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create vulkan instance because %s\n",vkRes2str(res));
        exit(VULKAN_CREATE_INSTANCE_FAILURE);
    }

    app->platform_window=App_create_window(app);

    app->window_surface=App_create_window_vk_surface(app,app->platform_window);

    uint32_t num_physical_devices;
    vkEnumeratePhysicalDevices(app->instance, &num_physical_devices, NULL);
    VkPhysicalDevice* physical_devices=malloc(num_physical_devices*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(app->instance, &num_physical_devices, physical_devices);

    // look for fit physical device, and required queue families
    int graphics_queue_family_index=-1;
    int present_queue_family_index=-1;

    for(uint32_t i=0;i<num_physical_devices;i++){
        VkPhysicalDevice physical_device=physical_devices[i];

        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device,&physical_device_properties);

        printf("got device %s of type %s\n",physical_device_properties.deviceName,device_type_name(physical_device_properties.deviceType));

        //vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats)

        uint32_t num_queue_families;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families,NULL);
        VkQueueFamilyProperties* queue_families=malloc(num_queue_families*sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families,queue_families);

        graphics_queue_family_index=-1;
        present_queue_family_index=-1;

        for(uint32_t queue_family_index=0;queue_family_index<num_queue_families;queue_family_index++){
            uint32_t qflag=queue_families[queue_family_index].queueFlags;
            if(qflag&VK_QUEUE_GRAPHICS_BIT){
                graphics_queue_family_index=queue_family_index;
            }

            VkBool32 surface_presentation_supported;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, app->window_surface, &surface_presentation_supported);

            if(surface_presentation_supported==VK_TRUE){
                present_queue_family_index=queue_family_index;
                printf("does support presentation\n");
            }else{
                printf("does not support presentation\n");
            }

            // prefer a queue that supports both
            if(graphics_queue_family_index==present_queue_family_index && present_queue_family_index!=-1){
                break;
            }
        }

        free(queue_families);

        if(graphics_queue_family_index==-1 || present_queue_family_index==-1){
            printf("  device incompatible\n");
            continue;
        }

        app->physical_device=physical_device;
    }
    free(physical_devices);

    {
        uint32_t num_device_extensions;
        vkEnumerateDeviceExtensionProperties(app->physical_device, NULL, &num_device_extensions,NULL);
        VkExtensionProperties* device_extensions=malloc(num_device_extensions*sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(app->physical_device, NULL, &num_device_extensions,device_extensions);

        uint32_t num_device_layers;
        vkEnumerateDeviceLayerProperties(app->physical_device, &num_device_layers, NULL);
        VkLayerProperties* device_layers=malloc(num_device_layers*sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(app->physical_device, &num_device_layers, device_layers);

        for(uint32_t i=0;i<num_device_extensions;i++){
            printf("device extension: %s\n",device_extensions[i].extensionName);
        }
        for(uint32_t i=0;i<num_device_layers;i++){
            printf("device layer: %s\n",device_layers[i].layerName);
        }
        
        free(device_extensions);
        free(device_layers);
    }

    app->graphics_queue_family_index=graphics_queue_family_index;
    app->present_queue_family_index=present_queue_family_index;

    VkDeviceQueueCreateInfo* queue_create_infos;
    uint32_t num_queue_create_infos;

    if(graphics_queue_family_index==present_queue_family_index){
        num_queue_create_infos=1;
        queue_create_infos=malloc(1*sizeof(VkDeviceQueueCreateInfo));

        queue_create_infos[0].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[0].pNext=NULL;
        queue_create_infos[0].flags=0;
        queue_create_infos[0].queueFamilyIndex=graphics_queue_family_index;
        queue_create_infos[0].queueCount=1;
        queue_create_infos[0].pQueuePriorities=(float[]){1.0};
    }else{
        num_queue_create_infos=2;
        queue_create_infos=malloc(2*sizeof(VkDeviceQueueCreateInfo));

        queue_create_infos[0].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[0].pNext=NULL;
        queue_create_infos[0].flags=0;
        queue_create_infos[0].queueFamilyIndex=graphics_queue_family_index;
        queue_create_infos[0].queueCount=1;
        queue_create_infos[0].pQueuePriorities=(float[]){1.0};

        queue_create_infos[1].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[1].pNext=NULL;
        queue_create_infos[1].flags=0;
        queue_create_infos[1].queueFamilyIndex=present_queue_family_index;
        queue_create_infos[1].queueCount=1;
        queue_create_infos[1].pQueuePriorities=(float[]){1.0};
    };

    uint32_t num_device_layers=1;
    const char *device_layers[1]={
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t num_device_extensions=1;
    const char *device_extensions[]={
        "VK_KHR_swapchain"
        #ifdef VK_USE_PLATFORM_METAL_EXT
        ,
        "VK_KHR_portability_subset"
        #endif
    };
    #ifdef VK_USE_PLATFORM_METAL_EXT
        num_device_extensions+=1;
    #endif

    VkPhysicalDeviceFeatures device_features;
    memset(&device_features,VK_FALSE,sizeof(VkPhysicalDeviceFeatures));
    VkDeviceCreateInfo device_create_info={
        .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .queueCreateInfoCount=num_queue_create_infos,
        .pQueueCreateInfos=queue_create_infos,
        .enabledLayerCount=num_device_layers,
        .ppEnabledLayerNames=device_layers,
        .enabledExtensionCount=num_device_extensions,
        .ppEnabledExtensionNames=device_extensions,
        .pEnabledFeatures=&device_features
    };
    res=vkCreateDevice(app->physical_device,&device_create_info,app->vk_allocator,&app->device);
    if(res!=VK_SUCCESS){
        exit(VULKAN_CREATE_DEVICE_FAILURE);
    }
    free(queue_create_infos);

    // if these are the same queue family, they will just point to the same queue, which is fine
    vkGetDeviceQueue(app->device, graphics_queue_family_index, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, present_queue_family_index, 0, &app->present_queue);

    uint32_t num_surface_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->window_surface, &num_surface_formats, NULL);
    VkSurfaceFormatKHR *surface_formats=malloc(num_surface_formats*sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->window_surface, &num_surface_formats, surface_formats);
    app->swapchain_format=surface_formats[0];
    free(surface_formats);
    // get surface present mode

    VkSurfaceCapabilitiesKHR surface_capabilities;
    res=vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device,app->window_surface,&surface_capabilities);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with %d\n",res);
        exit(VULKAN_GET_PHYSICAL_DEVICE_SURFACE_CAPABILITIES_KHR_FAILURE);
    }

    uint32_t num_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->window_surface, &num_present_modes, NULL);
    VkPresentModeKHR* present_modes=malloc(num_present_modes*sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->window_surface, &num_present_modes, present_modes);
    VkPresentModeKHR swapchain_present_mode=present_modes[0];
    free(present_modes);

    int swapchain_image_count=1;
    if(surface_capabilities.minImageCount>0){
        swapchain_image_count=surface_capabilities.minImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info={
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .surface=app->window_surface,
        .minImageCount=swapchain_image_count,
        .imageFormat=app->swapchain_format.format,
        .imageColorSpace=app->swapchain_format.colorSpace,
        .imageExtent=surface_capabilities.currentExtent,
        .imageArrayLayers=1,
        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL,
        .preTransform=surface_capabilities.currentTransform,
        .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode=swapchain_present_mode,
        .clipped=VK_TRUE,
        .oldSwapchain=VK_NULL_HANDLE
    };
    vkCreateSwapchainKHR(app->device, &swapchain_create_info, app->vk_allocator, &app->swapchain);

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->num_swapchain_images, NULL);
    app->swapchain_images=malloc(app->num_swapchain_images*sizeof(VkImage));
    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->num_swapchain_images, app->swapchain_images);

    app->swapchain_image_views=malloc(app->num_swapchain_images*sizeof(VkImageView));
    app->swapchain_framebuffers=malloc(app->num_swapchain_images*sizeof(VkFramebuffer));

    uint32_t num_render_pass_attachments=1;
    VkAttachmentDescription render_pass_attachments[1]={
        {
            /*VkAttachmentDescriptionFlags*/    .flags=0,
            /*VkFormat*/                        .format=app->swapchain_format.format,
            /*VkSampleCountFlagBits*/           .samples=VK_SAMPLE_COUNT_1_BIT,
            /*VkAttachmentLoadOp*/              .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
            /*VkAttachmentStoreOp*/             .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
            /*VkAttachmentLoadOp*/              .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            /*VkAttachmentStoreOp*/             .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
            /*VkImageLayout*/                   .initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            /*VkImageLayout*/                   .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };
    uint32_t num_render_pass_subpass=1;
    VkSubpassDescription render_pass_subpasses[1]={
        {
            /*VkSubpassDescriptionFlags */      .flags=0,
            /*VkPipelineBindPoint */            .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
            /*uint32_t */                       .inputAttachmentCount=0,
            /*const VkAttachmentReference**/    .pInputAttachments=NULL,
            /*uint32_t */                       .colorAttachmentCount=1,
            /*const VkAttachmentReference**/    .pColorAttachments=(VkAttachmentReference[1]){{
                /*uint32_t*/         .attachment=0,
                /*VkImageLayout*/    .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }},
            /*const VkAttachmentReference**/    .pResolveAttachments=NULL,
            /*const VkAttachmentReference**/    .pDepthStencilAttachment=NULL,
            /*uint32_t */                       .preserveAttachmentCount=0,
            /*const uint32_t**/                 .pPreserveAttachments=NULL,
        }
    };
    VkRenderPassCreateInfo render_pass_create_info={
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .attachmentCount=num_render_pass_attachments,
        .pAttachments=render_pass_attachments,
        .subpassCount=num_render_pass_subpass,
        .pSubpasses=render_pass_subpasses,
        .dependencyCount=2,
        .pDependencies=(VkSubpassDependency[2]){
            {
                .srcSubpass= VK_SUBPASS_EXTERNAL,
                .dstSubpass= 0,
                .srcStageMask= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask= VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags= VK_DEPENDENCY_BY_REGION_BIT
            },
            {
                .srcSubpass= 0, 
                .dstSubpass= VK_SUBPASS_EXTERNAL, 
                .srcStageMask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
                .dstStageMask= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                .srcAccessMask= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                .dstAccessMask= VK_ACCESS_MEMORY_READ_BIT, 
                .dependencyFlags= VK_DEPENDENCY_BY_REGION_BIT
            }
        }
    };
    res=vkCreateRenderPass(app->device,&render_pass_create_info,app->vk_allocator,&app->render_pass);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create renderpass\n");
        exit(VULKAN_CREATE_RENDER_PASS_FAILURE);
    }

    VkImageViewCreateInfo image_view_create_info={
        .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .image=VK_NULL_HANDLE,
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=app->swapchain_format.format,
        .components={
            .r=VK_COMPONENT_SWIZZLE_IDENTITY,
            .g=VK_COMPONENT_SWIZZLE_IDENTITY,
            .b=VK_COMPONENT_SWIZZLE_IDENTITY,
            .a=VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange={
            .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };
    VkFramebufferCreateInfo framebuffer_create_info={
        .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .renderPass=app->render_pass,
        .attachmentCount=1,
        .pAttachments=NULL,
        .width=surface_capabilities.currentExtent.width,
        .height=surface_capabilities.currentExtent.height,
        .layers=1
    };
    for(uint32_t i=0;i<app->num_swapchain_images;i++){
        image_view_create_info.image=app->swapchain_images[i];
        res=vkCreateImageView(app->device, &image_view_create_info, app->vk_allocator, &app->swapchain_image_views[i]);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to create image view for swapchain image %d\n",i);
        }

        framebuffer_create_info.pAttachments=&app->swapchain_image_views[i];
        res=vkCreateFramebuffer(app->device,&framebuffer_create_info,app->vk_allocator,&app->swapchain_framebuffers[i]);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to create framebuffer for swapchain image %d\n",i);
        }
    }

    app->shader=App_create_shader(app,0,render_pass_create_info.subpassCount);

    app->staging_buffer_size=1024*1024*128;
    app->staging_buffer_size_remaining=app->staging_buffer_size_remaining;

    VkBufferCreateInfo staging_buffer_create_info={
        .sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .size=app->staging_buffer_size,
        .usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL
    };
    res=vkCreateBuffer(app->device, &staging_buffer_create_info, app->vk_allocator, &app->staging_buffer);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create staging buffer\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }

    VkMemoryRequirements staging_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(app->device, app->staging_buffer, &staging_buffer_memory_requirements);

    uint32_t staging_buffer_memory_type_index=UINT32_MAX;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(app->physical_device,&physical_device_memory_properties);

    for(uint32_t i=0;i<physical_device_memory_properties.memoryTypeCount;i++){
        if((1<<i)&staging_buffer_memory_requirements.memoryTypeBits){
            staging_buffer_memory_type_index=i;
        }
    }

    VkMemoryAllocateInfo staging_buffer_memory_allocate_info={
        .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext=NULL,
        .allocationSize=staging_buffer_memory_requirements.size,
        .memoryTypeIndex=staging_buffer_memory_type_index
    };
    res=vkAllocateMemory(app->device, &staging_buffer_memory_allocate_info, app->vk_allocator, &app->staging_buffer_memory);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate staging buffer memory\n");
        exit(VULKAN_ALLOCATE_MEMORY_FAILURE);
    }

    res=vkBindBufferMemory(app->device, app->staging_buffer, app->staging_buffer_memory, 0);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to bind staging buffer memory\n");
        exit(VULKAN_BIND_BUFFER_MEMORY_FAILURE);
    }

    printf("created app\n");

    return app;
}
/// destroy an app
///
/// frees owned memory
void App_destroy(Application* app){
    if(app->instance!=VK_NULL_HANDLE){
        if(app->device!=VK_NULL_HANDLE){
            vkDeviceWaitIdle(app->device);

            App_destroy_shader(app->shader);

            for(uint32_t i=0;i<app->num_swapchain_images;i++){
                vkDestroyFramebuffer(app->device,app->swapchain_framebuffers[i],app->vk_allocator);
                vkDestroyImageView(app->device,app->swapchain_image_views[i],app->vk_allocator);
            }

            vkDestroyRenderPass(app->device,app->render_pass,app->vk_allocator);
            
            vkDestroySwapchainKHR(app->device, app->swapchain, app->vk_allocator);

            vkDestroyDevice(app->device,app->vk_allocator);
        }

        vkDestroySurfaceKHR(app->instance, app->window_surface, app->vk_allocator);

        vkDestroyInstance(app->instance,app->vk_allocator);
    }

    free(app);
}

void App_run(Application* app){
    VkResult res;

    VkSemaphoreCreateInfo semaphore_create_info={
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext=NULL,
        .flags=0
    };
    VkSemaphore image_available_semaphore;
    res=vkCreateSemaphore(app->device,&semaphore_create_info,app->vk_allocator,&image_available_semaphore);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create semaphore\n");
        exit(VULKAN_CREATE_SEMAPHORE_FAILURE);
    }
    VkSemaphore rendering_finished_semaphore;
    res=vkCreateSemaphore(app->device,&semaphore_create_info,app->vk_allocator,&rendering_finished_semaphore);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create semaphore\n");
        exit(VULKAN_CREATE_SEMAPHORE_FAILURE);
    }

    VkCommandPoolCreateInfo command_pool_create_info={
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex=app->graphics_queue_family_index
    };
    VkCommandPool command_pool;
    res=vkCreateCommandPool(app->device, &command_pool_create_info, app->vk_allocator, &command_pool);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create command pool\n");
        exit(VULKAN_CREATE_COMMAND_POOL_FAILURE);
    }

    VkCommandBufferAllocateInfo command_buffer_allocate_info={
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext=NULL,
        .commandPool=command_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1
    };
    VkCommandBuffer* command_buffers=malloc(command_buffer_allocate_info.commandBufferCount*sizeof(VkCommandBuffer));
    res=vkAllocateCommandBuffers(app->device, &command_buffer_allocate_info, command_buffers);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate command buffers\n");
        exit(VULKAN_ALLOCATE_COMMAND_BUFFERS_FAILURE);
    }

    VkCommandBuffer command_buffer=command_buffers[0];

    Mesh* quadmesh=NULL;

    ImageData image_data_jpeg;
    ImageParseResult image_parse_res=Image_read_jpeg("garbage.jpg",&image_data_jpeg);
    if (image_parse_res!=IMAGE_PARSE_RESULT_OK) {
        fprintf(stderr, "failed to parse jpeg\n");
        exit(-31);
    }

    ImageData image_data_ex={
        .data=(uint8_t[4]){255,255,0,255},
        .height=1,
        .width=1,
        .pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8,
        .interleaved=true
    };
    discard image_data_ex;

    ImageData* image_data=&image_data_jpeg;//&image_data_ex;

    Texture* sometexture=App_create_texture(app,image_data);

    int frame=0;
    bool window_should_close=false;
    while(1){
        if(window_should_close){
            break;
        }

        InputEvent event;
        while(App_get_input_event(app,&event)){
            switch(event.generic.input_event_type){
                case INPUT_EVENT_TYPE_WINDOW_CLOSE:
                    window_should_close=true;
                    break;
                default:
                    ;
            }
        }

        app->staging_buffer_size_remaining=app->staging_buffer_size;

        uint32_t next_swapchain_image_index;
        res=vkAcquireNextImageKHR(app->device, app->swapchain, 0xffffffffffffffff, image_available_semaphore, VK_NULL_HANDLE, &next_swapchain_image_index);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to acquire next swapchain image\n");
            exit(VULKAN_ACQUIRE_NEXT_IMAGE_KHR_FAILURE);
        }

        VkCommandBufferBeginInfo command_buffer_begin_info={
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext=NULL,
            .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo=NULL
        };
        vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        if(frame==0){
            quadmesh=App_upload_mesh(app, command_buffer, 4, mesh);

            App_upload_texture(app, sometexture, image_data, command_buffer);
        }

        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            0, 
            0, NULL, 
            0, NULL, 
            1, (VkImageMemoryBarrier[1]){{
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_MEMORY_READ_BIT,  
                .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  
                .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,  
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  
                .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
                .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,  
                .image=app->swapchain_images[next_swapchain_image_index], 
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1 
                }
            }}
        );

        VkRenderPassBeginInfo render_pass_begin_info={
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext=NULL,
            .renderPass=app->render_pass,
            .framebuffer=app->swapchain_framebuffers[next_swapchain_image_index],
            .renderArea={
                .offset={.x=0,.y=0},
                .extent={.width=640,.height=480}
            },
            .clearValueCount=1,
            .pClearValues=(VkClearValue[1]){{
                .color={.float32={1.0,1.0,1.0,1.0}}
            }}
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->shader->pipeline);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, (VkBuffer[1]){quadmesh->buffer}, (VkDeviceSize[1]){0});
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->shader->pipeline_layout, 0, 1, &app->shader->descriptor_set, 0, NULL);
        vkCmdDraw(command_buffer, 4, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            0, 
            0, NULL, 
            0, NULL, 
            1, (VkImageMemoryBarrier[1]){{
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex=app->present_queue_family_index,
                .dstQueueFamilyIndex=app->present_queue_family_index,
                .image=app->swapchain_images[next_swapchain_image_index],
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1 
                }
            }}
        );

        res=vkEndCommandBuffer(command_buffer);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to end command buffer\n");
            exit(VULKAN_END_COMMAND_BUFFER_FAILURE);
        }

        res=vkQueueSubmit(app->present_queue,1,(VkSubmitInfo[1]){{
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=&image_available_semaphore,
            .pWaitDstStageMask=(VkPipelineStageFlags[1]){VK_PIPELINE_STAGE_TRANSFER_BIT},
            .commandBufferCount=1,
            .pCommandBuffers=&command_buffer,
            .signalSemaphoreCount=1,
            .pSignalSemaphores=&rendering_finished_semaphore
        }},VK_NULL_HANDLE);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to submit swapchain presentation image\n");
            exit(VULKAN_QUEUE_SUBMIT_FAILURE);
        }

        VkPresentInfoKHR swapchain_present_info={
            .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=(VkSemaphore[1]){rendering_finished_semaphore},
            .swapchainCount=1,
            .pSwapchains=(VkSwapchainKHR[1]){app->swapchain},
            .pImageIndices=(uint32_t[1]){next_swapchain_image_index},
            NULL
        };
        res=vkQueuePresentKHR(app->present_queue, &swapchain_present_info);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to present swapchain image\n");
            exit(VULKAN_QUEUE_PRESENT_KHR_FAILURE);
        }

        vkDeviceWaitIdle(app->device);

        struct timespec time_to_sleep={
            .tv_nsec=33000000,
            .tv_sec=0
        };
        nanosleep(&time_to_sleep, NULL);

        frame+=1;
        discard frame;
    }

    App_destroy_texture(app,sometexture);

    App_destroy_mesh(app,quadmesh);

    vkDestroyBuffer(app->device, app->staging_buffer, app->vk_allocator);
    vkFreeMemory(app->device, app->staging_buffer_memory, app->vk_allocator);
    vkDestroySampler(app->device, app->shader->image_sampler, app->vk_allocator);

    vkDestroySemaphore(app->device,image_available_semaphore,app->vk_allocator);
    vkDestroySemaphore(app->device,rendering_finished_semaphore,app->vk_allocator);
    vkFreeCommandBuffers(app->device, command_pool, command_buffer_allocate_info.commandBufferCount, command_buffers);
    vkDestroyCommandPool(app->device,command_pool,app->vk_allocator);

    free(command_buffers);
}

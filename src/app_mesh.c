#include "app/app.h"

Mesh* App_upload_mesh(
    Application* app,

    VkCommandBuffer recording_command_buffer,

    uint32_t num_vertices,
    VertexData* vertex_data
){
    Mesh* mesh=malloc(sizeof(Mesh));

    VkDeviceSize mesh_memory_size=num_vertices*sizeof(VertexData);

    VkBufferCreateInfo buffer_create_info={
        .sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .size=mesh_memory_size,
        .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL
    };
    VkResult res=vkCreateBuffer(app->device, &buffer_create_info, app->vk_allocator, &mesh->buffer);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create buffer\n");
        exit(VULKAN_CREATE_BUFFER_FAILURE);
    }

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(app->device, mesh->buffer, &buffer_memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(app->physical_device, &memory_properties);

    uint32_t memory_type_index=UINT32_MAX;
    for(uint32_t i=0;i<memory_properties.memoryTypeCount;i++){
        if(
            (1<<i)&buffer_memory_requirements.memoryTypeBits
            && memory_properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        ){
            memory_type_index=i;
            break;
        }
    }
    if(memory_type_index==UINT32_MAX){exit(FATAL_UNEXPECTED_ERROR);}

    VkMemoryAllocateInfo memory_allocate_info={
        .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext=NULL,
        .allocationSize=buffer_memory_requirements.size,
        .memoryTypeIndex=memory_type_index
    };
    res=vkAllocateMemory(app->device, &memory_allocate_info, app->vk_allocator, &mesh->buffer_memory);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate memory\n");
        exit(VULKAN_ALLOCATE_MEMORY_FAILURE);
    }

    res=vkBindBufferMemory(app->device, mesh->buffer, mesh->buffer_memory, 0);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to bind buffer memory\n");
        exit(VULKAN_BIND_BUFFER_MEMORY_FAILURE);
    }

    // map device memory
    VertexData* mapped_gpu_memory;
    vkMapMemory(app->device, mesh->buffer_memory, 0, buffer_memory_requirements.size, 0, (void**)&mapped_gpu_memory);
    // copy mesh data
    memcpy(mapped_gpu_memory, vertex_data, num_vertices*sizeof(VertexData));
    // flush memory to gpu
    VkMappedMemoryRange mapped_memory_range={
        .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext=NULL,
        .memory=mesh->buffer_memory,
        .offset=0,
        .size=buffer_memory_requirements.size
    };
    vkFlushMappedMemoryRanges(app->device, 1, &mapped_memory_range);
    vkUnmapMemory(app->device, mesh->buffer_memory);

    discard recording_command_buffer;

    return mesh;
}
void App_destroy_mesh(Application* app,Mesh* mesh){
    vkFreeMemory(app->device, mesh->buffer_memory, app->vk_allocator);
    vkDestroyBuffer(app->device, mesh->buffer, app->vk_allocator);
}

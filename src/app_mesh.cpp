#include "app/app.hpp"
#include <vulkan/vulkan.h>

Mesh* Application::upload_mesh(
    VkCommandBuffer recording_command_buffer,

    uint32_t num_vertices,
    VertexData* vertex_data
){
    Mesh* mesh=new Mesh();

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
    VkResult res=vkCreateBuffer(this->device, &buffer_create_info, this->vk_allocator, &mesh->buffer);
    if(res!=VK_SUCCESS)
        bail(VULKAN_CREATE_BUFFER_FAILURE,"failed to create buffer\n");

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(this->device, mesh->buffer, &buffer_memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(this->physical_device, &memory_properties);

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
    res=vkAllocateMemory(this->device, &memory_allocate_info, this->vk_allocator, &mesh->buffer_memory);
    if(res!=VK_SUCCESS)
        bail(VULKAN_ALLOCATE_MEMORY_FAILURE,"failed to allocate memory\n");

    res=vkBindBufferMemory(this->device, mesh->buffer, mesh->buffer_memory, 0);
    if(res!=VK_SUCCESS)
        bail(VULKAN_BIND_BUFFER_MEMORY_FAILURE,"failed to bind buffer memory\n");

    // map device memory
    VertexData* mapped_gpu_memory;
    vkMapMemory(this->device, mesh->buffer_memory, 0, buffer_memory_requirements.size, 0, (void**)&mapped_gpu_memory);
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
    vkFlushMappedMemoryRanges(this->device, 1, &mapped_memory_range);
    vkUnmapMemory(this->device, mesh->buffer_memory);

    discard recording_command_buffer;

    return mesh;
}
void Application::destroy_mesh(Mesh* mesh){
    vkFreeMemory(this->device, mesh->buffer_memory, this->vk_allocator);
    vkDestroyBuffer(this->device, mesh->buffer, this->vk_allocator);

    delete mesh;
}

void Application::upload_data(
    VkCommandBuffer recording_command_buffer,

    VkBufferUsageFlagBits buffer_usage_flags,
    VkBuffer* buffer,
    VkDeviceMemory* buffer_memory,

    VkDeviceSize data_size_bytes,
    void* data
){
    VkDeviceSize memory_size=data_size_bytes;

    bool buffer_initially_null=*buffer == VK_NULL_HANDLE;
    bool memory_initially_null=*buffer_memory == VK_NULL_HANDLE;
    if (buffer_initially_null) {
        VkBufferCreateInfo buffer_create_info={
            .sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .size=memory_size,
            .usage=static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_DST_BIT|buffer_usage_flags),
            .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount=0,
            .pQueueFamilyIndices=NULL
        };
        VkResult res=vkCreateBuffer(
            this->device, 
            &buffer_create_info, 
            this->vk_allocator, 
            buffer
        );
        if(res!=VK_SUCCESS)
            bail(VULKAN_CREATE_BUFFER_FAILURE,"failed to create buffer\n");
    }

    if(memory_initially_null){
        VkMemoryRequirements buffer_memory_requirements;
        vkGetBufferMemoryRequirements(this->device, *buffer, &buffer_memory_requirements);

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(this->physical_device, &memory_properties);

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
        VkResult res=vkAllocateMemory(this->device, &memory_allocate_info, this->vk_allocator, buffer_memory);
        if(res!=VK_SUCCESS)
            bail(VULKAN_ALLOCATE_MEMORY_FAILURE,"failed to allocate memory\n");
    }

    if (buffer_initially_null || memory_initially_null) {
        VkResult res=vkBindBufferMemory(this->device, *buffer, *buffer_memory, 0);
        if(res!=VK_SUCCESS)
            bail(VULKAN_BIND_BUFFER_MEMORY_FAILURE,"failed to bind buffer memory\n");
    }

    // map device memory
    VertexData* mapped_gpu_memory;
    vkMapMemory(this->device, *buffer_memory, 0, memory_size, 0, (void**)&mapped_gpu_memory);
    // copy data
    memcpy(mapped_gpu_memory, data, data_size_bytes);
    // flush memory to gpu
    VkMappedMemoryRange mapped_memory_range={
        .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext=NULL,
        .memory=*buffer_memory,
        .offset=0,
        .size=memory_size
    };
    vkFlushMappedMemoryRanges(this->device, 1, &mapped_memory_range);
    vkUnmapMemory(this->device, *buffer_memory);

    discard recording_command_buffer;
}

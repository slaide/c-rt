#pragma once

#include "vulkan/vulkan_core.h"
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include "app/error.h"

#define block if(1)
#define discard (void)

typedef struct PlatformWindow PlatformWindow;
typedef struct PlatformHandle PlatformHandle;
typedef struct Application Application;

typedef struct VertexData{
    float x;
    float y;
    float z;
    float w;

    float r;
    float g;
    float b;
    float a;
}VertexData;

typedef struct Mesh{
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
}Mesh;

typedef struct Shader{
    Application* app;

    VkShaderModule fragment_shader;
    VkShaderModule vertex_shader;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
}Shader;

typedef struct Application{
    PlatformHandle* platform_handle;
    PlatformWindow* platform_window;

    VkInstance instance;
    VkAllocationCallbacks* vk_allocator;

    VkSurfaceKHR window_surface;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR swapchain_format;
    uint32_t num_swapchain_images;
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;
    VkFramebuffer* swapchain_framebuffers;

    VkPhysicalDevice physical_device;
    VkDevice device;

    uint32_t graphics_queue_family_index;
    VkQueue graphics_queue;
    uint32_t present_queue_family_index;
    VkQueue present_queue;

    VkRenderPass render_pass;

    Shader* shader;
} Application;

Application* App_new(PlatformHandle* platform);
void App_run(Application* app);
void App_destroy(Application* app);

PlatformWindow* App_create_window(Application* app);
VkSurfaceKHR App_create_window_vk_surface(Application* app,PlatformWindow* platform_window);
void App_destroy_window(Application* app, PlatformWindow* window);
void App_set_window_title(Application* app, PlatformWindow* window, const char* title);

Mesh* App_upload_mesh(
    Application* app,

    VkCommandBuffer recording_command_buffer,

    uint32_t num_vertices,
    VertexData* vertex_data
);
void App_destroy_mesh(Application* app,Mesh* mesh);

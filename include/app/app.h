#pragma once

#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <vulkan/vulkan.h>

#include "app/error.h"

/**
 * @brief utility macro
 * 
 */
#define discard (void)

#define ROUND_UP(VALUE,MULTIPLE_OF) (((VALUE) + ((MULTIPLE_OF)-1)) / (MULTIPLE_OF)) * (MULTIPLE_OF)
#define MASK(LENGTH) ((1<<(LENGTH))-1)

typedef enum InputKeyCode{
    INPUT_KEY_ARROW_RIGHT=1,
    INPUT_KEY_ARROW_LEFT=2,
    INPUT_KEY_ARROW_UP=3,
    INPUT_KEY_ARROW_DOWN=4,

    INPUT_KEY_UNKNOWN=0,
}InputKeyCode;

typedef enum InputEventType{
    INPUT_EVENT_TYPE_KEY_PRESS,
    INPUT_EVENT_TYPE_KEY_RELEASE,
    INPUT_EVENT_TYPE_BUTTON_PRESS,
    INPUT_EVENT_TYPE_BUTTON_RELEASE,

    INPUT_EVENT_TYPE_POINTER_MOVE,

    INPUT_EVENT_TYPE_SCROLL,

    INPUT_EVENT_TYPE_WINDOW_RESIZED,
    INPUT_EVENT_TYPE_WINDOW_CLOSE,

    INPUT_EVENT_TYPE_UNIMPLEMENTED,
}InputEventType;

typedef enum InputButton{
    INPUT_BUTTON_NONE=0x0,

    INPUT_BUTTON_LEFT=0x1,
    INPUT_BUTTON_RIGHT=0x2,
    INPUT_BUTTON_MIDDLE=0x4,
    INPUT_BUTTON_FORWARD=0x8,
    INPUT_BUTTON_BACK=0x10,

    INPUT_BUTTON_UNKNOWN=0x1000,
}InputButton;

typedef struct InputEventKeyPress{
    int input_event_type;
    InputKeyCode key;
}InputEventKeyPress;
typedef struct InputEventKeyRelease{
    int input_event_type;
    InputKeyCode key;
}InputEventKeyRelease;
typedef struct InputEventButtonPress{
    int input_event_type;

    InputButton button;
    /// pointer x position relative to window origin
    int32_t pointer_x;
    /// pointer y position relative to window origin
    int32_t pointer_y;
}InputEventButtonPress;
typedef struct InputEventButtonRelease{
    int input_event_type;

    InputButton button;
    /// pointer x position relative to window origin
    int32_t pointer_x;
    /// pointer y position relative to window origin
    int32_t pointer_y;
}InputEventButtonRelease;
typedef struct InputEventPointerMove{
    int input_event_type;

    /// pointer x position relative to window origin
    int32_t pointer_x;
    /// pointer y position relative to window origin
    int32_t pointer_y;

    InputButton button_pressed;
}InputEventPointerMove;
/// scroll event in x/y. distance in either dimension may be zero.
typedef struct InputEventScroll{
    int input_event_type;

    float scroll_x;
    float scroll_y;
}InputEventScroll;

/// the outer size of the window has changed
typedef struct InputEventWindowResized{
    int input_event_type;

    uint16_t old_width;
    uint16_t old_height;

    uint16_t new_width;
    uint16_t new_height;
}InputEventWindowResized; 
typedef struct InputEventWindowClose{
    int input_event_type;
}InputEventWindowClose;

typedef struct InputEventGeneric{
    int input_event_type;
}InputEventGeneric;

typedef union InputEvent{
    InputEventKeyPress keypress;
    InputEventKeyRelease keyrelease;

    InputEventButtonPress buttonpress;
    InputEventButtonRelease buttonrelease;

    InputEventPointerMove pointermove;

    InputEventScroll scroll;

    InputEventWindowResized windowresized;
    InputEventWindowClose windowclose;
    
    InputEventGeneric generic;
}InputEvent;

/**
 * @brief platform specific window handle
 * 
 */
typedef struct PlatformWindow PlatformWindow;

uint16_t PlatformWindow_get_window_height(PlatformWindow* platform_window);
uint16_t PlatformWindow_get_window_width(PlatformWindow* platform_window);
uint16_t PlatformWindow_get_render_area_height(PlatformWindow* platform_window);
uint16_t PlatformWindow_get_render_area_width(PlatformWindow* platform_window);
void PlatformWindow_set_render_area_height(PlatformWindow* platform_window,uint16_t new_render_area_height);
void PlatformWindow_set_render_area_width(PlatformWindow* platform_window,uint16_t new_render_area_width);
/**
 * @brief platform specific handle to internal system resources used as abstraction layer for system interaction of the application
 * 
 */
typedef struct PlatformHandle PlatformHandle;

typedef struct Application Application;

/**
 * @brief struct containing relevant data for a single vertex
 * 
 */
typedef struct VertexData{
    float x; /**< The x-coordinate of the vertex. */
    float y; /**< The y-coordinate of the vertex. */
    float z; /**< The z-coordinate of the vertex. */
    float w; /**< The w-coordinate of the vertex. */

    float u; /**< The u texture coordinate. */
    float v; /**< The v texture coordinate. */
}VertexData;

/**
 * @brief struct containing a mesh on the gpu
 * 
 * a reference to the internally used container for mesh data that is present on the gpu
 */
typedef struct Mesh{
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
}Mesh;

/**
 * @brief gpu shader object for object drawing
 * 
 * not to be confused with a shader module
 */
typedef struct Shader{
    Application* app;

    VkShaderModule fragment_shader;
    VkShaderModule vertex_shader;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkSampler image_sampler;
}Shader;

/**
 * @brief main application handle
 */
typedef struct Application{
    int cli_num_args;
    char** cli_args;
    
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

    VkDeviceSize staging_buffer_size;
    VkDeviceSize staging_buffer_size_remaining;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    VkRenderPass render_pass;

    Shader* shader;
} Application;

/**
 * \brief create a new app
 * 
 * \param platform a handle to a platform-specific handle. will be internally passed to all environment interactions that are platform-specific.
 *
 * \return a handle to a new application
 */
Application* App_new(PlatformHandle* platform);
/**
 * @brief run the main loop of an application
 * runs until the open window is closed
 * 
 * @param app 
 */
void App_run(Application* app);
/**
 * @brief destroy an application and free all resources
 * 
 * @param app 
 */
void App_destroy(Application* app);

typedef struct Texture Texture;

typedef enum PixelFormat{
    PIXEL_FORMAT_Ru8Gu8Bu8Au8
}PixelFormat;
typedef struct ImageData{
    uint8_t* data;

    uint32_t height;
    uint32_t width;

    PixelFormat pixel_format;
    bool interleaved;
}ImageData;
/**
 * @brief create a new texture on the gpu for use by a shader
 * 
 * @param app 
 * @param image_data contains image data on the cpu
 * @return Texture* 
 */
Texture* App_create_texture(Application* app, ImageData* image_data);

/**
 * @brief create a new window
 * 
 * @param app 
 * @return PlatformWindow* 
 */
PlatformWindow* App_create_window(
    Application* app,
    uint16_t width,
    uint16_t height
);
/**
 * @brief create a vulkan surface for the specified window
 * 
 * @param app 
 * @param platform_window 
 * @return VkSurfaceKHR 
 */
VkSurfaceKHR App_create_window_vk_surface(Application* app,PlatformWindow* platform_window);

#define INPUT_EVENT_PRESENT 1
#define INPUT_EVENT_NOT_PRESENT 0
/**
 * @brief attempt to retrieve a user input event
 * 
 * @param app 
 * @param event 
 * @return 1 if an event is present, otherwise 0
 */
int App_get_input_event(Application* app,InputEvent* event);
/**
 * @brief destroy a window
 * 
 * destroy a window. note that this cleans up the resources used by the window, and does not indicate destruction of a window during the main application event loop.
 * 
 * @param app 
 * @param window 
 */
void App_destroy_window(Application* app, PlatformWindow* window);
/**
 * @brief set the title of the specified window
 * 
 * @param app 
 * @param window 
 * @param title 
 */
void App_set_window_title(Application* app, PlatformWindow* window, const char* title);

/**
 * @brief upload a mesh to the gpu
 * 
 * @param app 
 * @param recording_command_buffer a vulkan command buffer in recording state where the upload commands will be recorded
 * @param num_vertices number of vertices in the specified cpu buffer
 * @param vertex_data vertex data on the cpu
 * @return Mesh* 
 */
Mesh* App_upload_mesh(
    Application* app,

    VkCommandBuffer recording_command_buffer,

    uint32_t num_vertices,
    VertexData* vertex_data
);
/**
 * @brief destroy a mesh and free all resources on the gpu
 * 
 * @param app 
 * @param mesh 
 */
void App_destroy_mesh(Application* app,Mesh* mesh);

typedef enum ImageParseResult{
    IMAGE_PARSE_RESULT_OK,
    IMAGE_PARSE_RESULT_FILE_NOT_FOUND,
}ImageParseResult;

ImageParseResult Image_read_jpeg(const char* filepath,ImageData* image_data);

/**
 * @brief upload data to gpu
 * allocates device memory, if memory handle is VK_NULL_HANDLE
 * creates buffer, if buffer handle is VK_NULL_HANDLE
 * and copies data to target memory (directly into target memory, i.e. target memory is host visible)
 * @param app 
 * @param recording_command_buffer 
 * @param buffer 
 * @param buffer_memory 
 * @param data_size_bytes 
 * @param data 
 */
void App_upload_data(
    Application* app,

    VkCommandBuffer recording_command_buffer,

    VkBufferUsageFlagBits buffer_usage_flags,
    VkBuffer* buffer,
    VkDeviceMemory* buffer_memory,

    VkDeviceSize data_size_bytes,
    void* data
);

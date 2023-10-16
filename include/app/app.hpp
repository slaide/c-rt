#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <memory>

#include <vulkan/vulkan.h>

#include "app/error.hpp"
#include "app/image.hpp"
#include "app/macros.hpp"

typedef void*(*pthread_callback)(void*);

#define MAX_NSEC 999999999

/**
 * @brief get current time from CLOCK_MONOTONIC as single value (seconds as double)
 * 
 * @return double 
 */
double current_time(void);

#define MASK(LENGTH) ((1<<(LENGTH))-1)

typedef enum InputKeyCode{
    INPUT_KEY_ARROW_RIGHT=1,
    INPUT_KEY_ARROW_LEFT=2,
    INPUT_KEY_ARROW_UP=3,
    INPUT_KEY_ARROW_DOWN=4,

    INPUT_KEY_LETTER_A,
    INPUT_KEY_LETTER_B,
    INPUT_KEY_LETTER_C,
    INPUT_KEY_LETTER_D,
    INPUT_KEY_LETTER_E,
    INPUT_KEY_LETTER_F,
    INPUT_KEY_LETTER_G,
    INPUT_KEY_LETTER_H,
    INPUT_KEY_LETTER_I,
    INPUT_KEY_LETTER_J,
    INPUT_KEY_LETTER_K,
    INPUT_KEY_LETTER_L,
    INPUT_KEY_LETTER_M,
    INPUT_KEY_LETTER_N,
    INPUT_KEY_LETTER_O,
    INPUT_KEY_LETTER_P,
    INPUT_KEY_LETTER_Q,
    INPUT_KEY_LETTER_R,
    INPUT_KEY_LETTER_S,
    INPUT_KEY_LETTER_T,
    INPUT_KEY_LETTER_U,
    INPUT_KEY_LETTER_V,
    INPUT_KEY_LETTER_W,
    INPUT_KEY_LETTER_X,
    INPUT_KEY_LETTER_Y,
    INPUT_KEY_LETTER_Z,

    INPUT_KEY_NUMROW_0,
    INPUT_KEY_NUMROW_1,
    INPUT_KEY_NUMROW_2,
    INPUT_KEY_NUMROW_3,
    INPUT_KEY_NUMROW_4,
    INPUT_KEY_NUMROW_5,
    INPUT_KEY_NUMROW_6,
    INPUT_KEY_NUMROW_7,
    INPUT_KEY_NUMROW_8,
    INPUT_KEY_NUMROW_9,

    INPUT_KEY_BACKSPACE,
    INPUT_KEY_ENTER,

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

class _VkCore{
    public:
        VkInstance instance=VK_NULL_HANDLE;
        VkAllocationCallbacks* vk_allocator=nullptr;
        VkPhysicalDevice physical_device=VK_NULL_HANDLE;
        VkDevice device=VK_NULL_HANDLE;

        _VkCore(){
            this->instance=VK_NULL_HANDLE;
            this->vk_allocator=nullptr;
            this->physical_device=VK_NULL_HANDLE;
            this->device=VK_NULL_HANDLE;
        }

        auto create_semaphore(VkSemaphoreCreateInfo* semaphore_create_info,VkSemaphore* semaphore){
            return vkCreateSemaphore(device,semaphore_create_info,vk_allocator,semaphore);
        }
};
typedef std::shared_ptr<_VkCore> VkCore;

class Application;

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

class Texture;

/**
 * @brief gpu shader object for object drawing
 * 
 * not to be confused with a shader module
 */
typedef struct Shader{
    VkCore core;

    VkShaderModule fragment_shader;
    VkShaderModule vertex_shader;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;

    VkSampler image_sampler;
}Shader;

/**
 * @brief main application handle
 */
class Application{
    public:

        uint32_t cli_num_args=0;
        char** cli_args=nullptr;

        VkCore core;
        
        PlatformHandle* platform_handle=nullptr;
        PlatformWindow* platform_window=nullptr;

        VkSurfaceKHR window_surface=VK_NULL_HANDLE;
        VkSwapchainKHR swapchain=VK_NULL_HANDLE;
        VkSurfaceFormatKHR swapchain_format;
        uint32_t num_swapchain_images;
        VkImage* swapchain_images=nullptr;
        VkImageView* swapchain_image_views=nullptr;
        VkFramebuffer* swapchain_framebuffers=nullptr;

        uint32_t graphics_queue_family_index=0;
        VkQueue graphics_queue=VK_NULL_HANDLE;
        uint32_t present_queue_family_index=0;
        VkQueue present_queue=VK_NULL_HANDLE;

        VkDeviceSize staging_buffer_size=0;
        VkDeviceSize staging_buffer_size_occupied=0;
        VkBuffer staging_buffer=VK_NULL_HANDLE;
        VkDeviceMemory staging_buffer_memory=VK_NULL_HANDLE;

        VkRenderPass render_pass=VK_NULL_HANDLE;

        Shader* shader=nullptr;

    public:

        /**
         * \brief create a new app
         * 
         * \param platform a handle to a platform-specific handle. will be internally passed to all environment interactions that are platform-specific.
         *
         * \return a handle to a new application
         */
        Application(PlatformHandle* platform);

        /**
         * @brief run the main loop of an application
         * runs until the open window is closed
         * 
         * @param app 
         */
        void run();

        /**
         * @brief destroy an application and free all resources
         * 
         * @param app 
         */
        void destroy();

        /**
         * @brief create a new texture on the gpu for use by a shader
         * 
         * @param app 
         * @param image_data contains image data on the cpu
         * @return Texture* 
         */
        Texture* create_texture(ImageData* image_data);
        void upload_texture(
            Texture* const texture,
            const ImageData* const image_data,
            VkCommandBuffer recording_command_buffer
        );
        void destroy_texture(Texture* texture);

        /**
         * @brief create a new window
         * 
         * @param app 
         * @return PlatformWindow* 
         */
        PlatformWindow* create_window(
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
        VkSurfaceKHR create_window_vk_surface(PlatformWindow* platform_window);

        #define INPUT_EVENT_PRESENT 1
        #define INPUT_EVENT_NOT_PRESENT 0
        /**
         * @brief attempt to retrieve a user input event
         * 
         * @param app 
         * @param event 
         * @return 1 if an event is present, otherwise 0
         */
        int get_input_event(InputEvent* event);
        /**
         * @brief destroy a window
         * 
         * destroy a window. note that this cleans up the resources used by the window, and does not indicate destruction of a window during the main application event loop.
         * 
         * @param app 
         * @param window 
         */
        void destroy_window(PlatformWindow* window);
        /**
         * @brief set the title of the specified window
         * 
         * @param app 
         * @param window 
         * @param title 
         */
        void set_window_title(PlatformWindow* window, const char* title);

        /**
         * @brief upload a mesh to the gpu
         * 
         * @param app 
         * @param recording_command_buffer a vulkan command buffer in recording state where the upload commands will be recorded
         * @param num_vertices number of vertices in the specified cpu buffer
         * @param vertex_data vertex data on the cpu
         * @return Mesh* 
         */
        Mesh* upload_mesh(
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
        void destroy_mesh(Mesh* mesh);

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
        void upload_data(
            VkCommandBuffer recording_command_buffer,

            VkBufferUsageFlagBits buffer_usage_flags,
            VkBuffer* buffer,
            VkDeviceMemory* buffer_memory,

            VkDeviceSize data_size_bytes,
            void* data
        );

        VkShaderModule create_shader_module(
            const char* const shader_file_path
        );
        Shader* create_shader(
            const uint32_t subpass,
            const uint32_t subpass_num_attachments
        );
        void destroy_shader(Shader* const shader);

        VkSwapchainCreateInfoKHR create_swapchain();
};


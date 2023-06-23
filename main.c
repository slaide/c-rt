#include<stdio.h>
#include<memory.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>

#include<xcb/xcb.h>
#include<xcb/xcb_util.h>
#include <xcb/xproto.h>
#define VK_USE_PLATFORM_XCB_KHR
#include<vulkan/vulkan.h>

#define block if(1)

enum AppError{
    // xcb errors
    XCB_CONNECT_FAILURE            = -0x001,
    XCB_WINDOW_CREATE_FAILURE      = -0x002,

    // vulkan errors
    VULKAN_CREATE_INSTANCE_FAILURE = -0x100,
    VULKAN_CREATE_XCB_SURFACE_FAILURE = -0x101,
};

const char* XCB_ERROR_NAMES[]={
    "0",
    [XCB_CONN_ERROR]="XCB_CONN_ERROR",
    "2",
    [XCB_WINDOW]="XCB_WINDOW",
    "3",
    "3",
    "3",
};
/// return string representation of a physical device type
///
/// returns NULL on invalid (or unknown) device type
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

typedef struct Application{
    xcb_connection_t* connection;
    xcb_window_t window;
    xcb_atom_t delete_window_atom;

    VkInstance instance;
    VkAllocationCallbacks* vk_allocator;

    VkSurfaceKHR window_surface;

    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;
} Application;

xcb_atom_t App_xcb_intern_atom(Application* app,const char* atom_name){
    xcb_intern_atom_cookie_t atom_cookie=xcb_intern_atom(app->connection, 0, strlen(atom_name), atom_name);
    xcb_intern_atom_reply_t* atom_reply=xcb_intern_atom_reply(app->connection, atom_cookie, NULL);
    xcb_atom_t atom=atom_reply->atom;
    free(atom_reply);
    return atom;
}

/// create a new app
///
/// this struct owns all memory, unless indicated otherwise
Application* App_new(void){
    Application* app=malloc(sizeof(Application));

    app->connection=xcb_connect(NULL,NULL);
    int xcb_connection_error_state=xcb_connection_has_error(app->connection);
    if(xcb_connection_error_state!=0){
        fprintf(stderr,"xcb connection error state %d\n",xcb_connection_error_state);
        exit(XCB_CONNECT_FAILURE);
    }
    const xcb_setup_t* setup=xcb_get_setup(app->connection);
    xcb_screen_iterator_t screens=xcb_setup_roots_iterator(setup);

    app->window=xcb_generate_id(app->connection);
    xcb_flush(app->connection);

    // possible value mask values are in enum xcb_cw_t
    xcb_cw_t value_mask=XCB_CW_EVENT_MASK;
    const uint32_t value_list[]={
        XCB_EVENT_MASK_BUTTON_PRESS
        | XCB_EVENT_MASK_BUTTON_RELEASE
        | XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_KEY_RELEASE
        | XCB_EVENT_MASK_POINTER_MOTION
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_void_cookie_t window_create_reply=xcb_create_window_checked(
        app->connection, 
        XCB_COPY_FROM_PARENT, 
        app->window, 
        screens.data->root, 
        0,0, 
        640,480, 
        10, 
        XCB_WINDOW_CLASS_INPUT_OUTPUT, 
        screens.data->root_visual, 
        value_mask,
        value_list
    );
    xcb_flush(app->connection);
    xcb_generic_error_t* window_create_error=xcb_request_check(app->connection, window_create_reply);
    if(window_create_error!=NULL && window_create_error->error_code!=0){
        fprintf(stderr,"failed to create window %d\n",window_create_error->error_code);
        exit(XCB_WINDOW_CREATE_FAILURE);
    }

    app->delete_window_atom=App_xcb_intern_atom(app, "WM_DELETE_WINDOW");
    xcb_atom_t wm_protocols_atom=App_xcb_intern_atom(app, "WM_PROTOCOLS");

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(app->connection, XCB_PROP_MODE_REPLACE, app->window, wm_protocols_atom, XCB_ATOM_ATOM, 32, 1, (xcb_atom_t[]){app->delete_window_atom});
    xcb_generic_error_t* change_property_error=xcb_request_check(app->connection, change_property_cookie);
    if(change_property_error!=NULL){
        fprintf(stderr,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));
        exit(-7);
    }

    xcb_map_window(app->connection,app->window);

    block{
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

    const char* instance_layers[1]={
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t num_instance_layers=1;

    const char* instance_extensions[2]={
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    uint32_t num_instance_extensions=2;

    VkInstanceCreateInfo instance_create_info={
        .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .pApplicationInfo=NULL,
        .enabledLayerCount=num_instance_layers,
        .ppEnabledLayerNames=instance_layers,
        .enabledExtensionCount=num_instance_extensions,
        .ppEnabledExtensionNames=instance_extensions
    };
    VkResult res=vkCreateInstance(&instance_create_info,app->vk_allocator,&app->instance);
    if(res!=VK_SUCCESS){
        exit(VULKAN_CREATE_INSTANCE_FAILURE);
    }

    VkXcbSurfaceCreateInfoKHR surface_create_info={
        .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .connection=app->connection,
        .window=app->window
    };
    res=vkCreateXcbSurfaceKHR(app->instance, &surface_create_info, app->vk_allocator, &app->window_surface);
    if(res!=VK_SUCCESS){
        exit(VULKAN_CREATE_XCB_SURFACE_FAILURE);
    }

    uint32_t num_physical_devices;
    vkEnumeratePhysicalDevices(app->instance, &num_physical_devices, NULL);
    VkPhysicalDevice* physical_devices=malloc(num_physical_devices*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(app->instance, &num_physical_devices, physical_devices);

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

    block{
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
    const char *device_extensions[1]={
        "VK_KHR_swapchain"
    };
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
        exit(-6);
    }
    free(queue_create_infos);

    // if these are the same queue family, they will just point to the same queue, which is fine
    vkGetDeviceQueue(app->device, graphics_queue_family_index, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, present_queue_family_index, 0, &app->present_queue);

    return app;
}
/// destroy an app
///
/// frees owned memory
void App_destroy(Application* app){

    if(app->instance!=VK_NULL_HANDLE){
        if(app->device!=VK_NULL_HANDLE){
            vkDeviceWaitIdle(app->device);
            
            vkDestroyDevice(app->device,app->vk_allocator);
        }

        vkDestroySurfaceKHR(app->instance, app->window_surface, app->vk_allocator);

        vkDestroyInstance(app->instance,app->vk_allocator);
    }

    xcb_unmap_window(app->connection,app->window);
    xcb_destroy_window(app->connection,app->window);
    xcb_disconnect(app->connection);

    free(app);
}

void App_run(Application* app){
    int frame=0;
    int window_should_close=0;
    while(1){
        if(frame==300){
            break;
        }

        if(window_should_close){
            break;
        }

        xcb_generic_event_t* event;
        while((event=xcb_poll_for_event(app->connection))){
            // full sequence is essentially event id
            // sequence is the lower half of the event id
            uint8_t event_type=XCB_EVENT_RESPONSE_TYPE(event);
            printf("got event %s\n",xcb_event_get_label(event_type));
            switch(event_type){
                case XCB_CLIENT_MESSAGE:
                    block{
                        xcb_client_message_event_t* client_message = (xcb_client_message_event_t*) event;
                        if(client_message->data.data32[0]==app->delete_window_atom){
                            window_should_close=1;
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        vkDeviceWaitIdle(app->device);

        struct timespec time_to_sleep={
            .tv_nsec=33000000,
            .tv_sec=0
        };
        nanosleep(&time_to_sleep, NULL);

        frame+=1;
    }
}

int main(int argc, char**argv){
    for(int i=0;i<argc;i++){
        printf("got arg: %s\n",argv[i]);
    }

    Application *app=App_new();

    App_run(app);

    App_destroy(app);
    
    return 0;
}

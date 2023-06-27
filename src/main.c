#include <stdio.h>

#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>

#include <app/app.h>

struct PlatformHandle{
    xcb_connection_t* connection;
    xcb_window_t window;
    xcb_atom_t delete_window_atom;
}

xcb_atom_t App_xcb_intern_atom(Application* app,const char* atom_name){
    xcb_intern_atom_cookie_t atom_cookie=xcb_intern_atom(app->connection, 0, strlen(atom_name), atom_name);
    xcb_intern_atom_reply_t* atom_reply=xcb_intern_atom_reply(app->connection, atom_cookie, NULL);
    xcb_atom_t atom=atom_reply->atom;
    free(atom_reply);
    return atom;
}

void App_set_window_title(Application* app,const char* title){
    xcb_atom_t net_wm_name=App_xcb_intern_atom(app,"_NET_WM_NAME");
    xcb_atom_t net_wm_visible_name=App_xcb_intern_atom(app,"_NET_WM_VISIBLE_NAME");

    xcb_change_property_checked(app->connection, XCB_PROP_MODE_REPLACE, app->window, net_wm_name, App_xcb_intern_atom(app,"UTF8_STRING"), 8, strlen(title), title);
    xcb_change_property_checked(app->connection, XCB_PROP_MODE_REPLACE, app->window, net_wm_visible_name, App_xcb_intern_atom(app,"UTF8_STRING"), 8, strlen(title), title);
}
VkSurfaceKHR App_create_window_vk_surface(Application* app,PlatformWindow* platform_window){
    VkXcbSurfaceCreateInfoKHR surface_create_info={
        .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .connection=app->connection,
        .window=app->window
    };
    VkSurfaceKHR surface;
    VkResult res=vkCreateXcbSurfaceKHR(app->instance, &surface_create_info, app->vk_allocator, &surface);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create xcb surface\n");
        exit(VULKAN_CREATE_XCB_SURFACE_FAILURE);
    }

    return surface;
}

PlatformHandle* Platform_new(){
    PlatformHandle* platform=malloc(sizeof(PlatformHandle));

    platform_handle->connection=xcb_connect(NULL,NULL);
    int xcb_connection_error_state=xcb_connection_has_error(platform_handle->connection);
    if(xcb_connection_error_state!=0){
        fprintf(stderr,"xcb connection error state %d\n",xcb_connection_error_state);
        exit(XCB_CONNECT_FAILURE);
    }
    const xcb_setup_t* setup=xcb_get_setup(platform_handle->connection);
    xcb_screen_iterator_t screens=xcb_setup_roots_iterator(setup);

    platform_handle->window=xcb_generate_id(platform_handle->connection);
    xcb_flush(platform_handle->connection);

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
        platform_handle->connection, 
        XCB_COPY_FROM_PARENT, 
        platform_handle->window, 
        screens.data->root, 
        0,0, 
        640,480, 
        10, 
        XCB_WINDOW_CLASS_INPUT_OUTPUT, 
        screens.data->root_visual, 
        value_mask,
        value_list
    );
    xcb_flush(platform_handle->connection);
    xcb_generic_error_t* window_create_error=xcb_request_check(platform_handle->connection, window_create_reply);
    if(window_create_error!=NULL && window_create_error->error_code!=0){
        fprintf(stderr,"failed to create window %d\n",window_create_error->error_code);
        exit(XCB_WINDOW_CREATE_FAILURE);
    }

    platform_handle->delete_window_atom=App_xcb_intern_atom(platform_handle, "WM_DELETE_WINDOW");
    xcb_atom_t wm_protocols_atom=App_xcb_intern_atom(platform_handle, "WM_PROTOCOLS");

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(platform_handle->connection, XCB_PROP_MODE_REPLACE, platform_handle->window, wm_protocols_atom, XCB_ATOM_ATOM, 32, 1, (xcb_atom_t[]){app->delete_window_atom});
    xcb_generic_error_t* change_property_error=xcb_request_check(platform_handle->connection, change_property_cookie);
    if(change_property_error!=NULL){
        fprintf(stderr,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));
        exit(XCB_CHANGE_PROPERTY_FAILURE);
    }

    xcb_map_window(platform_handle->connection,platform_handle->window);

    return platform_handle;
}
void Platform_destroy(PlatformHandle* platform){
    xcb_unmap_window(app->connection,app->window);
    xcb_destroy_window(app->connection,app->window);
    xcb_disconnect(app->connection);
}

int main(int argc, char**argv){
    for(int i=0;i<argc;i++){
        printf("got arg: %s\n",argv[i]);
    }

    PlatformHandle* platform=Platform_new();
    Application *app=App_new(platform);

    App_set_window_title(app,"my window");

    App_run(app);

    App_destroy(app);
    Platform_destroy(platform);
    
    return 0;
}

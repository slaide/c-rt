#include <stdio.h>

#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#include <xcb/xcb_util.h>

#include <app/app.h>

struct PlatformHandle{
    xcb_connection_t* connection;

    int num_open_windows;
    PlatformWindow** open_windows;
};
struct PlatformWindow{
    xcb_window_t window;
    xcb_atom_t delete_window_atom;
};

xcb_atom_t Platform_xcb_intern_atom(PlatformHandle* platform,const char* atom_name){
    xcb_intern_atom_cookie_t atom_cookie=xcb_intern_atom(platform->connection, 0, strlen(atom_name), atom_name);
    xcb_generic_error_t* error;
    xcb_intern_atom_reply_t* atom_reply=xcb_intern_atom_reply(platform->connection, atom_cookie, &error);
    xcb_atom_t atom=atom_reply->atom;
    if(error){
        fprintf(stderr,"got some error\n");
        exit(XCB_INTERN_ATOM_FAILURE);
    }
    free(atom_reply);
    return atom;
}

void App_set_window_title(Application* app, PlatformWindow* window, const char* title){
    xcb_atom_t net_wm_name=Platform_xcb_intern_atom(app->platform_handle,"_NET_WM_NAME");
    xcb_atom_t net_wm_visible_name=Platform_xcb_intern_atom(app->platform_handle,"_NET_WM_VISIBLE_NAME");

    xcb_change_property_checked(app->platform_handle->connection, 
        XCB_PROP_MODE_REPLACE, 
        window->window, 
        net_wm_name, 
        Platform_xcb_intern_atom(app->platform_handle,"UTF8_STRING"), 
        8, strlen(title), title
    );
    xcb_change_property_checked(app->platform_handle->connection, 
        XCB_PROP_MODE_REPLACE, 
        window->window, 
        net_wm_visible_name, 
        Platform_xcb_intern_atom(app->platform_handle,"UTF8_STRING"), 
        8, strlen(title), title
    );
}
VkSurfaceKHR App_create_window_vk_surface(Application* app,PlatformWindow* platform_window){
    VkXcbSurfaceCreateInfoKHR surface_create_info={
        .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .connection=app->platform_handle->connection,
        .window=platform_window->window
    };
    VkSurfaceKHR surface;
    VkResult res=vkCreateXcbSurfaceKHR(app->instance, &surface_create_info, app->vk_allocator, &surface);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create xcb surface\n");
        exit(VULKAN_CREATE_XCB_SURFACE_FAILURE);
    }

    return surface;
}

int App_get_input_event(Application* app,InputEvent* input_event){
    xcb_generic_event_t* xcb_event=xcb_poll_for_event(app->platform_handle->connection);
    if(xcb_event == NULL){
        return INPUT_EVENT_NOT_PRESENT;
    }

    input_event->generic.input_event_type=INPUT_EVENT_TYPE_UNIMPLEMENTED;
    
    // full sequence is essentially event id
    // sequence is the lower half of the event id
    uint8_t event_type=XCB_EVENT_RESPONSE_TYPE(xcb_event);
    switch(event_type){
        case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t* client_message = (xcb_client_message_event_t*) xcb_event;
                for(int window_id=0;window_id<app->platform_handle->num_open_windows;window_id++){
                    PlatformWindow* open_window=app->platform_handle->open_windows[window_id];

                    xcb_atom_t client_message_atom=client_message->data.data32[0];
                    if(client_message_atom==open_window->delete_window_atom){
                        input_event->generic.input_event_type=INPUT_EVENT_TYPE_WINDOW_CLOSE;
                    }
                }
            }
            break;
        default:
            printf("got event %s\n",xcb_event_get_label(event_type));
    }

    return INPUT_EVENT_PRESENT;
}

PlatformHandle* Platform_new(){
    PlatformHandle* platform=malloc(sizeof(PlatformHandle));

    platform->connection=xcb_connect(NULL,NULL);
    int xcb_connection_error_state=xcb_connection_has_error(platform->connection);
    if(xcb_connection_error_state!=0){
        fprintf(stderr,"xcb connection error state %d\n",xcb_connection_error_state);
        exit(XCB_CONNECT_FAILURE);
    }

    platform->num_open_windows=0;
    platform->open_windows=NULL;

    return platform;
}
void Platform_destroy(PlatformHandle* platform){
    if(platform->num_open_windows>0){
        free(platform->open_windows);
    }
    xcb_disconnect(platform->connection);
}

PlatformWindow* App_create_window(Application* application){
    const xcb_setup_t* setup=xcb_get_setup(application->platform_handle->connection);
    xcb_screen_iterator_t screens=xcb_setup_roots_iterator(setup);

    PlatformWindow* window=malloc(sizeof(PlatformWindow));
    window->window=xcb_generate_id(application->platform_handle->connection);
    xcb_flush(application->platform_handle->connection);

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
        application->platform_handle->connection, 
        XCB_COPY_FROM_PARENT, 
        window->window, 
        screens.data->root, 
        0,0, 
        640,480, 
        10, 
        XCB_WINDOW_CLASS_INPUT_OUTPUT, 
        screens.data->root_visual, 
        value_mask,
        value_list
    );
    xcb_flush(application->platform_handle->connection);
    xcb_generic_error_t* window_create_error=xcb_request_check(application->platform_handle->connection, window_create_reply);
    if(window_create_error!=NULL && window_create_error->error_code!=0){
        fprintf(stderr,"failed to create window %d\n",window_create_error->error_code);
        exit(XCB_WINDOW_CREATE_FAILURE);
    }

    window->delete_window_atom=Platform_xcb_intern_atom(application->platform_handle, "WM_DELETE_WINDOW");
    printf("delete window atom %d\n",window->delete_window_atom);
    xcb_atom_t wm_protocols_atom=Platform_xcb_intern_atom(application->platform_handle, "WM_PROTOCOLS");
    printf("delete window atom %d\n",wm_protocols_atom);

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(application->platform_handle->connection, XCB_PROP_MODE_REPLACE, window->window, wm_protocols_atom, XCB_ATOM_ATOM, 32, 1, &window->delete_window_atom);
    xcb_generic_error_t* change_property_error=xcb_request_check(application->platform_handle->connection, change_property_cookie);
    if(change_property_error!=NULL){
        fprintf(stderr,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));
        exit(XCB_CHANGE_PROPERTY_FAILURE);
    }

    int xcb_connection_error_state=xcb_connection_has_error(application->platform_handle->connection);
    if(xcb_connection_error_state!=0){
        fprintf(stderr,"xcb connection error state %d\n",xcb_connection_error_state);
        exit(XCB_CONNECT_FAILURE);
    }

    xcb_map_window(application->platform_handle->connection,window->window);

    int new_window_id=application->platform_handle->num_open_windows;
    application->platform_handle->num_open_windows+=1;
    application->platform_handle->open_windows=realloc(application->platform_handle->open_windows, application->platform_handle->num_open_windows*sizeof(PlatformWindow*));

    application->platform_handle->open_windows[new_window_id]=window;

    return window;
}
void App_destroy_window(Application *app, PlatformWindow *window){
    xcb_unmap_window(app->platform_handle->connection,window->window);
    xcb_destroy_window(app->platform_handle->connection,window->window);

    app->platform_handle->num_open_windows-=1;
    if(app->platform_handle->num_open_windows>0){
        app->platform_handle->open_windows=realloc(app->platform_handle->open_windows,app->platform_handle->num_open_windows*sizeof(PlatformWindow*));
    }else{
        free(app->platform_handle->open_windows);
    }
}

int main(int argc, char**argv){
    for(int i=0;i<argc;i++){
        printf("got arg: %s\n",argv[i]);
    }

    PlatformHandle* platform=Platform_new();
    Application *app=App_new(platform);

    App_set_window_title(app,app->platform_window,"my window");

    App_run(app);

    App_destroy(app);
    Platform_destroy(platform);
    
    return 0;
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <vector>

#include <xcb/xcb.h>
#include <xcb/xcb_util.h>
#include <vulkan/vulkan.h>

#include <app/app.hpp>

struct PlatformHandle{
    xcb_connection_t* connection;

    std::vector<PlatformWindow*> open_windows;
};
struct PlatformWindow{
    xcb_window_t window;
    xcb_atom_t delete_window_atom;

    uint16_t window_width;
    uint16_t window_height;
    uint16_t render_area_width;
    uint16_t render_area_height;
};
uint16_t PlatformWindow_get_window_height(PlatformWindow* platform_window){
    return  platform_window->window_height;
}
uint16_t PlatformWindow_get_window_width(PlatformWindow* platform_window){
    return  platform_window->window_width;
}
uint16_t PlatformWindow_get_render_area_height(PlatformWindow* platform_window){
    return  platform_window->render_area_height;
}
uint16_t PlatformWindow_get_render_area_width(PlatformWindow* platform_window){
    return  platform_window->render_area_width;
}
void PlatformWindow_set_render_area_height(PlatformWindow* platform_window,uint16_t new_render_area_height){
    platform_window->render_area_height=new_render_area_height;
}
void PlatformWindow_set_render_area_width(PlatformWindow* platform_window,uint16_t new_render_area_width){
    platform_window->render_area_width=new_render_area_width;
}

xcb_atom_t Platform_xcb_intern_atom(PlatformHandle* platform,const char* atom_name){
    xcb_intern_atom_cookie_t atom_cookie=xcb_intern_atom(platform->connection, 0, (uint16_t)strlen(atom_name), atom_name);
    xcb_generic_error_t* error;
    xcb_intern_atom_reply_t* atom_reply=xcb_intern_atom_reply(platform->connection, atom_cookie, &error);
    xcb_atom_t atom=atom_reply->atom;
    if(error)
        bail(XCB_INTERN_ATOM_FAILURE,"got some error\n");

    free(atom_reply);
    return atom;
}

void Application::set_window_title(PlatformWindow* window, const char* title){
    xcb_atom_t net_wm_name=Platform_xcb_intern_atom(this->platform_handle,"_NET_WM_NAME");
    xcb_atom_t net_wm_visible_name=Platform_xcb_intern_atom(this->platform_handle,"_NET_WM_VISIBLE_NAME");

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(this->platform_handle->connection, 
        XCB_PROP_MODE_REPLACE, 
        window->window, 
        net_wm_name, 
        Platform_xcb_intern_atom(this->platform_handle,"UTF8_STRING"), 
        8, (uint32_t)strlen(title), title
    );
    xcb_generic_error_t* change_property_error=xcb_request_check(this->platform_handle->connection, change_property_cookie);
    if(change_property_error!=NULL)
        bail(XCB_CHANGE_PROPERTY_FAILURE,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));
    change_property_cookie=xcb_change_property_checked(this->platform_handle->connection, 
        XCB_PROP_MODE_REPLACE, 
        window->window, 
        net_wm_visible_name, 
        Platform_xcb_intern_atom(this->platform_handle,"UTF8_STRING"), 
        8, (uint32_t)strlen(title), title
    );
    change_property_error=xcb_request_check(this->platform_handle->connection, change_property_cookie);
    if(change_property_error!=NULL)
        bail(XCB_CHANGE_PROPERTY_FAILURE,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));
}
VkSurfaceKHR Application::create_window_vk_surface(PlatformWindow* platform_window){
    VkXcbSurfaceCreateInfoKHR surface_create_info={
        .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .connection=this->platform_handle->connection,
        .window=platform_window->window
    };
    VkSurfaceKHR surface;
    VkResult res=vkCreateXcbSurfaceKHR(this->instance, &surface_create_info, this->vk_allocator, &surface);
    if(res!=VK_SUCCESS)
        bail(VULKAN_CREATE_XCB_SURFACE_FAILURE,"failed to create xcb surface\n");

    return surface;
}

InputKeyCode XCBKeycode_to_InputKeyCode(xcb_keycode_t keycode){
    switch(keycode){
        case 24: return INPUT_KEY_LETTER_Q;
        case 25: return INPUT_KEY_LETTER_W;
        case 26: return INPUT_KEY_LETTER_E;
        case 27: return INPUT_KEY_LETTER_R;
        case 28: return INPUT_KEY_LETTER_T;
        case 29: return INPUT_KEY_LETTER_Y;

        case 113: return INPUT_KEY_ARROW_LEFT;
        case 114: return INPUT_KEY_ARROW_RIGHT;
        case 111: return INPUT_KEY_ARROW_UP;
        case 116: return INPUT_KEY_ARROW_DOWN;

        default:
            #ifdef DEBUG
                printf("pressed unknown key %d\n",keycode);
            #endif

            return INPUT_KEY_UNKNOWN;
    }
}

int Application::get_input_event(InputEvent* input_event){
    xcb_generic_event_t* xcb_event=xcb_poll_for_event(this->platform_handle->connection);
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
                for(PlatformWindow* open_window : this->platform_handle->open_windows){
                    xcb_atom_t client_message_atom=client_message->data.data32[0];
                    if(client_message_atom==open_window->delete_window_atom){
                        input_event->generic.input_event_type=INPUT_EVENT_TYPE_WINDOW_CLOSE;
                    }
                }
            }
            break;
        case XCB_CONFIGURE_NOTIFY:
            {
                xcb_configure_notify_event_t* config_event=(xcb_configure_notify_event_t*)xcb_event;

                input_event->windowresized.input_event_type=INPUT_EVENT_TYPE_WINDOW_RESIZED;
                input_event->windowresized.new_height=config_event->height;
                input_event->windowresized.new_width=config_event->width;

                input_event->windowresized.old_height=this->platform_window->window_height;
                input_event->windowresized.old_width=this->platform_window->window_width;

                this->platform_window->window_height=config_event->height;
                this->platform_window->window_width=config_event->width;
            }
            break;

        enum MouseButton{
            MOUSE_BUTTON_LEFT=XCB_BUTTON_INDEX_1,
            MOUSE_BUTTON_MIDDLE=XCB_BUTTON_INDEX_2,
            MOUSE_BUTTON_RIGHT=XCB_BUTTON_INDEX_3,
            MOUSE_BUTTON_SCROLL_UP=XCB_BUTTON_INDEX_4,
            MOUSE_BUTTON_SCROLL_DOWN=XCB_BUTTON_INDEX_5,
        };

        case XCB_MOTION_NOTIFY:{
                xcb_motion_notify_event_t* motion_event=(xcb_motion_notify_event_t*)xcb_event;
                input_event->pointermove.input_event_type=INPUT_EVENT_TYPE_POINTER_MOVE;
                input_event->pointermove.pointer_x=motion_event->event_x;
                input_event->pointermove.pointer_y=this->platform_window->render_area_height-motion_event->event_y;

                if(motion_event->state&256){
                    input_event->pointermove.button_pressed=INPUT_BUTTON_LEFT;
                }else if(motion_event->state&512){
                    input_event->pointermove.button_pressed=INPUT_BUTTON_MIDDLE;
                }else if(motion_event->state&1024){
                    input_event->pointermove.button_pressed=INPUT_BUTTON_RIGHT;
                }else{
                    input_event->pointermove.button_pressed=INPUT_BUTTON_NONE;
                }
            }
            break;

        case XCB_BUTTON_PRESS:{
                xcb_button_press_event_t* button_event=(xcb_button_press_event_t*)xcb_event;
                switch(button_event->detail){
                    case MOUSE_BUTTON_LEFT:
                        input_event->buttonpress.input_event_type=INPUT_EVENT_TYPE_BUTTON_PRESS;
                        input_event->buttonpress.button=INPUT_BUTTON_LEFT;
                        input_event->buttonpress.pointer_x=button_event->event_x;
                        input_event->buttonpress.pointer_y=this->platform_window->render_area_height-button_event->event_y;
                        break;
                    case MOUSE_BUTTON_MIDDLE:
                        input_event->buttonpress.input_event_type=INPUT_EVENT_TYPE_BUTTON_PRESS;
                        input_event->buttonpress.button=INPUT_BUTTON_MIDDLE;
                        input_event->buttonpress.pointer_x=button_event->event_x;
                        input_event->buttonpress.pointer_y=this->platform_window->render_area_height-button_event->event_y;
                        break;
                    case MOUSE_BUTTON_RIGHT:
                        input_event->buttonpress.input_event_type=INPUT_EVENT_TYPE_BUTTON_PRESS;
                        input_event->buttonpress.button=INPUT_BUTTON_RIGHT;
                        input_event->buttonpress.pointer_x=button_event->event_x;
                        input_event->buttonpress.pointer_y=this->platform_window->render_area_height-button_event->event_y;
                        break;
                    case MOUSE_BUTTON_SCROLL_UP:{
                            input_event->scroll.input_event_type=INPUT_EVENT_TYPE_SCROLL;
                            input_event->scroll.scroll_x=0.0;
                            input_event->scroll.scroll_y=1.0;
                        }
                        break;
                    case MOUSE_BUTTON_SCROLL_DOWN:{
                            input_event->scroll.input_event_type=INPUT_EVENT_TYPE_SCROLL;
                            input_event->scroll.scroll_x=0.0;
                            input_event->scroll.scroll_y=-1.0;
                        }
                        break;
                }
            }
            break;
        case XCB_BUTTON_RELEASE:{
                xcb_button_release_event_t* button_event=(xcb_button_release_event_t*)xcb_event;
                switch(button_event->detail){
                    case MOUSE_BUTTON_LEFT:
                        input_event->buttonrelease.input_event_type=INPUT_EVENT_TYPE_BUTTON_RELEASE;
                        input_event->buttonrelease.button=INPUT_BUTTON_LEFT;
                        break;
                    case MOUSE_BUTTON_MIDDLE:
                        input_event->buttonrelease.input_event_type=INPUT_EVENT_TYPE_BUTTON_RELEASE;
                        input_event->buttonrelease.button=INPUT_BUTTON_MIDDLE;
                        break;
                    case MOUSE_BUTTON_RIGHT:
                        input_event->buttonrelease.input_event_type=INPUT_EVENT_TYPE_BUTTON_RELEASE;
                        input_event->buttonrelease.button=INPUT_BUTTON_RIGHT;
                        break;
                }
            }
            break;
        case XCB_KEY_RELEASE:{
                xcb_key_release_event_t* key_event=(xcb_key_release_event_t*)xcb_event;
                input_event->keyrelease.input_event_type=INPUT_EVENT_TYPE_KEY_RELEASE;
                input_event->keyrelease.key=XCBKeycode_to_InputKeyCode(key_event->detail);
            }
            break;
        case XCB_KEY_PRESS:{
                xcb_key_press_event_t* key_event=(xcb_key_press_event_t*)xcb_event;
                input_event->keypress.input_event_type=INPUT_EVENT_TYPE_KEY_PRESS;
                input_event->keypress.key=XCBKeycode_to_InputKeyCode(key_event->detail);
            }
            break;

        case XCB_REPARENT_NOTIFY:
        case XCB_MAP_NOTIFY:
        case 35: // this event id pops up sometimes, but it is not recognized
            break;
        
        default:{
                #ifdef DEBUG
                    const char* event_label=xcb_event_get_label(event_type);
                    printf("got unhandled event %s ( %d )\n",event_label,event_type);
                #else
                    ;
                #endif
            }
    }

    free(xcb_event);

    return INPUT_EVENT_PRESENT;
}

PlatformHandle* Platform_new(){
    PlatformHandle* platform=new PlatformHandle();

    platform->connection=xcb_connect(NULL,NULL);
    int xcb_connection_error_state=xcb_connection_has_error(platform->connection);
    if(xcb_connection_error_state!=0)
        bail(XCB_CONNECT_FAILURE,"xcb connection error state %d\n",xcb_connection_error_state);

    return platform;
}
void Platform_destroy(PlatformHandle* platform){
    xcb_disconnect(platform->connection);
    delete platform;
}

PlatformWindow* Application::create_window(
    uint16_t width,
    uint16_t height
){
    const xcb_setup_t* setup=xcb_get_setup(this->platform_handle->connection);
    xcb_screen_iterator_t screens=xcb_setup_roots_iterator(setup);

    PlatformWindow* window=new PlatformWindow();
    window->window_height=height;
    window->window_width=width;
    
    window->window=xcb_generate_id(this->platform_handle->connection);
    xcb_flush(this->platform_handle->connection);

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
        this->platform_handle->connection, 
        XCB_COPY_FROM_PARENT, 
        window->window, 
        screens.data->root, 
        0,0, 
        width,height, 
        10, 
        XCB_WINDOW_CLASS_INPUT_OUTPUT, 
        screens.data->root_visual, 
        value_mask,
        value_list
    );
    xcb_generic_error_t* window_create_error=xcb_request_check(this->platform_handle->connection, window_create_reply);
    if(window_create_error!=NULL && window_create_error->error_code!=0)
        bail(XCB_WINDOW_CREATE_FAILURE,"failed to create window %d\n",window_create_error->error_code);

    xcb_flush(this->platform_handle->connection);

    window->delete_window_atom=Platform_xcb_intern_atom(this->platform_handle, "WM_DELETE_WINDOW");
    xcb_atom_t wm_protocols_atom=Platform_xcb_intern_atom(this->platform_handle, "WM_PROTOCOLS");

    xcb_void_cookie_t change_property_cookie=xcb_change_property_checked(this->platform_handle->connection, XCB_PROP_MODE_REPLACE, window->window, wm_protocols_atom, XCB_ATOM_ATOM, 32, 1, &window->delete_window_atom);
    xcb_generic_error_t* change_property_error=xcb_request_check(this->platform_handle->connection, change_property_cookie);
    if(change_property_error!=NULL)
        bail(XCB_CHANGE_PROPERTY_FAILURE,"failed to set property because %s\n",xcb_event_get_error_label(change_property_error->error_code));

    int xcb_connection_error_state=xcb_connection_has_error(this->platform_handle->connection);
    if(xcb_connection_error_state!=0)
        bail(XCB_CONNECT_FAILURE,"xcb connection error state %d\n",xcb_connection_error_state);

    xcb_map_window(this->platform_handle->connection,window->window);

    this->platform_handle->open_windows.push_back(window);

    return window;
}
void Application::destroy_window(PlatformWindow* window){
    xcb_unmap_window(this->platform_handle->connection,window->window);
    xcb_destroy_window(this->platform_handle->connection,window->window);

    delete window;

    for(auto it=this->platform_handle->open_windows.begin();it<this->platform_handle->open_windows.end();it++){
        if(*it==window){
            this->platform_handle->open_windows.erase(it);
            break;
        }
    }
}

int main(int argc, char** argv){
    PlatformHandle* platform=Platform_new();
    Application *app=new Application(platform);

    app->cli_num_args=(uint32_t)argc;
    app->cli_args=argv;

    if (app->cli_num_args>1) {
        app->set_window_title(app->platform_window,app->cli_args[1]);
    }else{
        app->set_window_title(app->platform_window,"unknown image");
    }

    app->run();

    app->destroy();
    Platform_destroy(platform);
    
    return 0;
}

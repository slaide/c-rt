#include <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <objc/objc.h>
#include <string.h>

#include <vulkan/vulkan.h>

#import <QuartzCore/QuartzCore.h>
#import <AppKit/AppKit.h>

#include "app/app.hpp"

@interface MyView: NSView
    - (instancetype)init;
@end

@implementation MyView
    - (instancetype)init{
        self=[super init];

        self.wantsLayer=YES;

        return self;
    }
    - (CALayer *)makeBackingLayer{
        return [CAMetalLayer layer];
    }
@end

@interface MyWindow: NSWindow
    @property(nonatomic, strong) NSMutableArray *eventList;
    - (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)style;
@end

struct PlatformWindow{
    MyWindow* window;

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

@implementation MyWindow
    - (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)style{
        self=[
            super
            initWithContentRect:contentRect
            styleMask:style
            backing:NSBackingStoreBuffered
            defer:NO
        ];
    
        self.contentView=[[MyView alloc] init];

        self.eventList=[NSMutableArray array];

        return self;
    }

    // intercept certain events to notify the OS that we are actually using them
    - (void)keyDown:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    - (void)keyUp:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseDown:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseUp:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseDragged:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseEntered:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseExited:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)mouseMoved:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
    -(void)scrollWheel:(NSEvent *)event {
        [event retain];
        [self.eventList addObject:event];
    }
@end

@interface MyAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> 

    @property(assign, nonatomic) Application* app;
    @property(nonatomic) CVDisplayLinkRef display_link;
    @property (nonatomic, strong) NSMutableArray *eventList;

    @property(assign, nonatomic) uint32_t cli_argc;
    @property(assign, nonatomic) char** cli_argv;

@end

struct PlatformHandle{
    MyAppDelegate* app;
};

CVReturn display_link_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext
){
    discard inNow;
    discard inOutputTime;
    discard flagsIn;
    discard flagsOut;

    Application* app=(Application*)displayLinkContext;
    app->run();

    app->destroy();
    
    CVDisplayLinkStop(displayLink);

    [[NSApplication sharedApplication] terminate:nil];

    return kCVReturnSuccess;
}

@interface WindowCloseEvent : NSObject
    @property (nonatomic, strong) NSWindow *window;
@end
@implementation WindowCloseEvent
@end

@interface WindowResizeEvent : NSObject
    @property (nonatomic, strong) NSWindow *window;

    @property(assign,nonatomic) uint16_t new_width;
    @property(assign,nonatomic) uint16_t new_height;
@end
@implementation WindowResizeEvent
@end

@implementation MyAppDelegate

    - (void)applicationDidFinishLaunching:(NSNotification *)notification{
        PlatformHandle* myplatform=new PlatformHandle();
        myplatform->app=self;
        Application* main_app=new Application(myplatform);

        main_app->cli_num_args=self.cli_argc;
        main_app->cli_args=self.cli_argv;

        if (main_app->cli_num_args>1) {
            [main_app->platform_window->window setTitle:[NSString stringWithUTF8String:main_app->cli_args[1]]];
        }

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
        CVDisplayLinkSetOutputCallback(display_link, display_link_callback, main_app);
        CVDisplayLinkStart(display_link);

        self.display_link=display_link;

        self.eventList=[NSMutableArray array];
    }
    - (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
        return NO;
    }
    - (void)windowWillClose:(NSNotification *)notification {
        NSWindow *closingWindow = notification.object;

        WindowCloseEvent* windowCloseEvent=[WindowCloseEvent alloc];

        windowCloseEvent.window=closingWindow;

        [self.eventList addObject:windowCloseEvent];
    }
    - (void)windowDidResize:(NSNotification *)notification {
        NSWindow *resizedWindow = [notification object];

        WindowResizeEvent* windowResizeEvent=[WindowResizeEvent alloc];

        windowResizeEvent.window=resizedWindow;
        windowResizeEvent.new_height=(uint16_t)resizedWindow.frame.size.height;
        windowResizeEvent.new_width=(uint16_t)resizedWindow.frame.size.width;

        [self.eventList addObject:windowResizeEvent];
    }

@end

PlatformWindow* Application::create_window(
    uint16_t width,
    uint16_t height
){
    PlatformWindow* window=new PlatformWindow();
    window->window_height=height;
    window->window_width=width;
    window->window=[
        [MyWindow alloc]
        initWithContentRect: NSMakeRect(0.0, 0.0, (CGFloat)width, (CGFloat)height)
        styleMask: NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
    ];
    [window->window center];
    [window->window setTitle:@"unknown image"];
    [window->window orderFrontRegardless];

    // from docs: Moves the window to the front of the screen list, within its level, and makes it the key window; that is, it shows the window.
    [window->window makeKeyAndOrderFront:nil];

    window->window.delegate=this->platform_handle->app;

    return window;
}
VkSurfaceKHR Application::create_window_vk_surface(PlatformWindow* platform_window){
    VkMetalSurfaceCreateInfoEXT surface_create_info={
        .sType=VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext=NULL,
        .flags=0,
        .pLayer=(CAMetalLayer*)platform_window->window.contentView.layer
    };
    VkSurfaceKHR surface;
    VkResult res=vkCreateMetalSurfaceEXT(this->core->instance, &surface_create_info, this->core->vk_allocator, &surface);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create metal surface\n");
        exit(VULKAN_CREATE_XCB_SURFACE_FAILURE);
    }

    return surface;
}
void Application::destroy_window(PlatformWindow* window){
    [window->window.contentView release];
    [window->window release];
    delete window;
}
void Application::set_window_title(PlatformWindow* window, const char* title){
    // window interaction needs to happen on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        [window->window setTitle:[NSString stringWithUTF8String:title]];
    });
}

InputButton NSEventButton_to_InputButton(NSInteger ns_button){
    switch(ns_button){
        case 0:
            return INPUT_BUTTON_LEFT;
        case 1:
            return INPUT_BUTTON_RIGHT;
        case 2:
            return INPUT_BUTTON_MIDDLE;
        case 3:
            return INPUT_BUTTON_BACK;
        case 4:
            return INPUT_BUTTON_FORWARD;
        default:
            return INPUT_BUTTON_UNKNOWN;
    }
}

/// the 'nskeycode's are not enumerated anywhere.
/// nskeycode is not even a real class/enum, it's just whatever value NSEvent.keyCode holds.
/// these values are extracted using testing on a MacOS device.
InputKeyCode NSKeyCode_to_InputKeyCode(unsigned short nskeycode){
    switch (nskeycode) {
        case 12: return INPUT_KEY_LETTER_Q;
        case 13: return INPUT_KEY_LETTER_W;
        case 14: return INPUT_KEY_LETTER_E;
        case 15: return INPUT_KEY_LETTER_R;
        case 17: return INPUT_KEY_LETTER_T;
        case 16: return INPUT_KEY_LETTER_Y;

        case 123: return INPUT_KEY_ARROW_LEFT;
        case 124: return INPUT_KEY_ARROW_RIGHT;
        case 125: return INPUT_KEY_ARROW_DOWN;
        case 126: return INPUT_KEY_ARROW_UP;
        default:
            #ifdef DEBUG
                printf("unknown key %d\n",nskeycode);
            #endif

            return INPUT_KEY_UNKNOWN;
    }
}

int Application::get_input_event(InputEvent *event){
    if (this->platform_window->window.eventList.count>0) {
        NSEvent* ns_event=[this->platform_window->window.eventList firstObject];
        [this->platform_window->window.eventList removeObjectAtIndex:0];

        switch (ns_event.type) {
            case NSEventTypeLeftMouseDown:
            case NSEventTypeRightMouseDown:
            case NSEventTypeOtherMouseDown:
                event->buttonpress.input_event_type=INPUT_EVENT_TYPE_BUTTON_PRESS;
                event->buttonpress.button=NSEventButton_to_InputButton(ns_event.buttonNumber);
                event->buttonpress.pointer_x=(int32_t)ns_event.locationInWindow.x;
                event->buttonpress.pointer_y=(int32_t)ns_event.locationInWindow.y;
                break;

            case NSEventTypeLeftMouseUp:
            case NSEventTypeRightMouseUp:
            case NSEventTypeOtherMouseUp:
                event->buttonrelease.input_event_type=INPUT_EVENT_TYPE_BUTTON_RELEASE;
                event->buttonrelease.button=NSEventButton_to_InputButton(ns_event.buttonNumber);
                event->buttonrelease.pointer_x=(int32_t)ns_event.locationInWindow.x;
                event->buttonrelease.pointer_y=(int32_t)ns_event.locationInWindow.y;
                break;

            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged:
            case NSEventTypeMouseMoved:
                event->pointermove.input_event_type=INPUT_EVENT_TYPE_POINTER_MOVE;
                event->pointermove.pointer_x=(int32_t)ns_event.locationInWindow.x;
                event->pointermove.pointer_y=(int32_t)ns_event.locationInWindow.y;

                if(ns_event.pressure==0){
                    event->pointermove.button_pressed=INPUT_BUTTON_NONE;
                }else{
                    event->pointermove.button_pressed=NSEventButton_to_InputButton(ns_event.buttonNumber);
                }
                break;
            case NSEventTypeScrollWheel:
                event->scroll.input_event_type=INPUT_EVENT_TYPE_SCROLL;

                event->scroll.scroll_x=(float)ns_event.scrollingDeltaX;
                event->scroll.scroll_y=(float)ns_event.scrollingDeltaY;
                break;

            case NSEventTypeKeyDown:
                event->keypress.input_event_type=INPUT_EVENT_TYPE_KEY_PRESS;
                event->keypress.key=NSKeyCode_to_InputKeyCode(ns_event.keyCode);
                break;
            case NSEventTypeKeyUp:
                event->keypress.input_event_type=INPUT_EVENT_TYPE_KEY_RELEASE;
                event->keypress.key=NSKeyCode_to_InputKeyCode(ns_event.keyCode);
                break;

            default:
                break;
        }

        [ns_event release];

        return INPUT_EVENT_PRESENT;
    }
    if (this->platform_handle->app.eventList.count>0) {
        id nextEvent=[this->platform_handle->app.eventList firstObject];
        [this->platform_handle->app.eventList removeObjectAtIndex:0];

        if ([nextEvent isKindOfClass:[WindowResizeEvent class]]) {
            event->generic.input_event_type=INPUT_EVENT_TYPE_WINDOW_RESIZED;

            WindowResizeEvent* resize_event=nextEvent;
            event->windowresized.new_height=resize_event.new_height;
            event->windowresized.new_width=resize_event.new_width;

            event->windowresized.old_width=this->platform_window->window_width;
            event->windowresized.old_height=this->platform_window->window_height;

            this->platform_window->window_width=resize_event.new_width;
            this->platform_window->window_height=resize_event.new_height;

            [nextEvent release];
        }else if ([nextEvent isKindOfClass:[WindowCloseEvent class]]) {
            event->generic.input_event_type=INPUT_EVENT_TYPE_WINDOW_CLOSE;
            [nextEvent release];
        }else{
            NSLog(@"got input event of unknown type %@",nextEvent);
        }

        return INPUT_EVENT_PRESENT;
    }

    return INPUT_EVENT_NOT_PRESENT;
}

int main(int argc, char** argv){
    NSApplication *app=[NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];

    MyAppDelegate *myDelegate=[[MyAppDelegate alloc] init];
    [app setDelegate:myDelegate];

    myDelegate.cli_argc=(uint32_t)argc;
    myDelegate.cli_argv=argv;

    [app run];
}

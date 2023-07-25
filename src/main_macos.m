#include "vulkan/vulkan_core.h"
#include <CoreVideo/CoreVideo.h>
#include <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory.h>
#include <objc/objc.h>
#include <string.h>

#include <vulkan/vulkan.h>

#import <QuartzCore/QuartzCore.h>
#import <AppKit/AppKit.h>

#include "app/app.h"

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
    - (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)style;
@end

struct PlatformWindow{
    MyWindow* window;
};

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

        return self;
    }

    - (void)sendEvent:(NSEvent *)event {
        //NSLog(@"Received event: %@", event);

        // Pass the event to the superclass for default handling
        [super sendEvent:event];
    }
@end

@interface MyAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> 

    @property(assign, nonatomic) Application* app;
    @property(nonatomic) CVDisplayLinkRef display_link;
    @property (nonatomic, strong) NSMutableArray *eventList;

    @property(assign, nonatomic) int cli_argc;
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
    App_run(app);

    App_destroy(app);
    
    CVDisplayLinkStop(displayLink);

    [[NSApplication sharedApplication] terminate:nil];

    return kCVReturnSuccess;
}

@interface WindowCloseEvent : NSObject

    @property (nonatomic, strong) NSWindow *window;

@end
@implementation WindowCloseEvent

@end

@implementation MyAppDelegate

    - (void)applicationDidFinishLaunching:(NSNotification *)notification{
        PlatformHandle* myplatform=malloc(sizeof(PlatformHandle));
        myplatform->app=self;
        Application* main_app=App_new(myplatform);

        main_app->cli_num_args=self.cli_argc;
        main_app->cli_args=self.cli_argv;

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
        CVDisplayLinkSetOutputCallback(display_link, display_link_callback, main_app);
        CVDisplayLinkStart(display_link);

        self.display_link=display_link;
        printf("app finished launching\n");

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

@end

PlatformWindow* App_create_window(Application* app){
    discard app;

    PlatformWindow* window=malloc(sizeof(PlatformWindow));
    window->window=[
        [MyWindow alloc]
        initWithContentRect: NSMakeRect(0.0, 0.0, 640.0, 480.0)
        styleMask: NSWindowStyleMaskClosable | NSWindowStyleMaskTitled
    ];
    [window->window center];
    [window->window setTitle:@"my window title"];
    [window->window orderFrontRegardless];

    // from docs: Moves the window to the front of the screen list, within its level, and makes it the key window; that is, it shows the window.
    [window->window makeKeyAndOrderFront:nil];

    window->window.delegate=app->platform_handle->app;

    return window;
}
VkSurfaceKHR App_create_window_vk_surface(Application* app,PlatformWindow* platform_window){
    VkMetalSurfaceCreateInfoEXT surface_create_info={
        .sType=VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext=NULL,
        .flags=0,
        .pLayer=(CAMetalLayer*)platform_window->window.contentView.layer
    };
    VkSurfaceKHR surface;
    VkResult res=vkCreateMetalSurfaceEXT(app->instance, &surface_create_info, app->vk_allocator, &surface);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create metal surface\n");
        exit(VULKAN_CREATE_XCB_SURFACE_FAILURE);
    }

    return surface;
}
void App_destroy_window(Application* app, PlatformWindow* window){
    discard app;
    [window->window.contentView release];
    [window->window release];
    free(window);
}
void App_set_window_title(Application* app, PlatformWindow* window, const char* title){
    discard app;
    [window->window setTitle:[NSString stringWithUTF8String:title]];
}

int App_get_input_event(Application *app, InputEvent *event){
    discard app;
    discard event;

    if (app->platform_handle->app.eventList.count>0) {
        id nextEvent=[app->platform_handle->app.eventList firstObject];
        [app->platform_handle->app.eventList removeObjectAtIndex:0];

        if ([nextEvent isKindOfClass:[WindowCloseEvent class]]) {
            event->generic.input_event_type=INPUT_EVENT_TYPE_WINDOW_CLOSE;
            [nextEvent release];
        }

        return INPUT_EVENT_PRESENT;
    }

    return INPUT_EVENT_NOT_PRESENT;
}

int main(int argc, char *argv[]){
    NSApplication *app=[NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];

    MyAppDelegate *myDelegate=[[MyAppDelegate alloc] init];
    [app setDelegate:myDelegate];

    myDelegate.cli_argc=argc;
    myDelegate.cli_argv=argv;

    [app run];
}

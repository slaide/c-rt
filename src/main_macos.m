#include "vulkan/vulkan_core.h"
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
    free(window);
}
void App_set_window_title(Application* app, PlatformWindow* window, const char* title){
    discard app;
    [window->window setTitle:[NSString stringWithUTF8String:title]];
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

        return self;
    }
@end

@interface MyAppDelegate : NSObject <NSApplicationDelegate> 

    @property(assign, nonatomic) Application* app;
    @property(nonatomic) CVDisplayLinkRef display_link;

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
    discard displayLink;
    discard inNow;
    discard inOutputTime;
    discard flagsIn;
    discard flagsOut;

    Application* app=(Application*)displayLinkContext;
    App_run(app);

    App_destroy(app);

    return kCVReturnSuccess;
}

@implementation MyAppDelegate

    - (void)applicationDidFinishLaunching:(NSNotification *)notification{
        PlatformHandle* myplatform=malloc(sizeof(PlatformHandle));
        myplatform->app=self;
        Application* main_app=App_new(myplatform);

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
        CVDisplayLinkSetOutputCallback(display_link, display_link_callback, main_app);
        CVDisplayLinkStart(display_link);

        self.display_link=display_link;
        printf("app finished launching\n");
    }
    - (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
        return YES;
    }

@end

int App_get_input_event(Application *app, InputEvent *event){
    discard app;
    discard event;
    
    return  INPUT_EVENT_NOT_PRESENT;
}

int main(int argc, char *argv[]){
    discard argc;
    discard argv;

    NSApplication *app=[NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];

    MyAppDelegate *myDelegate=[[MyAppDelegate alloc] init];
    [app setDelegate:myDelegate];

    [app run];
}

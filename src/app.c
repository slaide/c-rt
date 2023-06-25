#include "app/app.h"

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

VertexData mesh[4]={
    {
        -0.7f, -0.7f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f
    },
    {
        -0.7f, 0.7f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f
    },
    {
        0.7f, -0.7f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    },
    {
        0.7f, 0.7f, 0.0f, 1.0f,
        0.3f, 0.3f, 0.3f, 0.0f
    }
};

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

VkShaderModule App_create_shader_module(Application* app,const char* shader_file_path){
    FILE* shader_file=fopen(shader_file_path,"rb");
    if(shader_file==NULL){
        fprintf(stderr,"failed to open shader file %s\n",shader_file_path);
        exit(VULKAN_SHADER_FILE_NOT_FOUND);
    }

    discard fseek(shader_file,0,SEEK_END);
    int shader_size=ftell(shader_file);
    rewind(shader_file);

    uint32_t* shader_code=malloc(shader_size);
    fread(shader_code, 1, shader_size, shader_file);

    fclose(shader_file);

    VkShaderModuleCreateInfo shader_module_create_info={
        .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .codeSize=shader_size,
        .pCode=shader_code
    };

    VkShaderModule shader;
    vkCreateShaderModule(app->device, &shader_module_create_info, app->vk_allocator, &shader);

    free(shader_code);

    return shader;
}
Shader* App_create_shader(
    Application* app,

    uint32_t subpass,
    uint32_t subpass_num_attachments
){
    Shader* shader=malloc(sizeof(Shader));

    shader->app=app;

    shader->fragment_shader=App_create_shader_module(app, "shaders/frag.spv");
    shader->vertex_shader=App_create_shader_module(app, "shaders/vert.spv");

    VkDescriptorSetLayoutBinding set_binding={
        .binding=0,
        .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount=1,
        .stageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .pImmutableSamplers=NULL
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .bindingCount=1,
        .pBindings=&set_binding
    };
    VkResult res=vkCreateDescriptorSetLayout(app->device, &descriptor_set_layout, app->vk_allocator, &shader->set_layout);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create descriptor set layout\n");
        exit(VULKAN_CREATE_DESCRIPTOR_SET_LAYOUT_FAILURE);
    }

    VkDescriptorPoolSize pool_sizes[1]={{
        .descriptorCount=1,
        .type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    }};
    VkDescriptorPoolCreateInfo descriptor_pool_create_info={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .maxSets=1,
        .poolSizeCount=1,
        .pPoolSizes=pool_sizes
    };
    res=vkCreateDescriptorPool(app->device, &descriptor_pool_create_info, app->vk_allocator, &shader->descriptor_pool);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create descriptor pool\n");
        exit(VULKAN_CREATE_DESCRIPTOR_POOL_FAILURE);
    }
    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info={
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext=NULL,
        .descriptorPool=shader->descriptor_pool,
        .descriptorSetCount=1,
        .pSetLayouts=&shader->set_layout
    };
    res=vkAllocateDescriptorSets(app->device, &descriptor_set_allocate_info, &shader->descriptor_set);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate descriptor sets\n");
        exit(VULKAN_ALLOCATE_DESCRIPTOR_SETS_FAILURE);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .setLayoutCount=0,
        .pSetLayouts=NULL,
        .pushConstantRangeCount=0,
        .pPushConstantRanges=NULL
    };
    vkCreatePipelineLayout(app->device, &pipeline_layout_create_info, app->vk_allocator, &shader->pipeline_layout);

    VkVertexInputBindingDescription vertex_input_binding_descriptions[1]={{
        .binding=0,
        .stride=sizeof(VertexData),
        .inputRate=VK_VERTEX_INPUT_RATE_VERTEX
    }};
    VkVertexInputAttributeDescription vertex_input_attribute_descriptions[2]={
        {
            .location=0,
            .binding=vertex_input_binding_descriptions[0].binding,
            .format=VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset=offsetof(struct VertexData, x)
        },
        {
            .location=1,
            .binding=vertex_input_binding_descriptions[0].binding,
            .format=VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset=offsetof( struct VertexData, r)
        }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .vertexBindingDescriptionCount=1,
        .pVertexBindingDescriptions=vertex_input_binding_descriptions,
        .vertexAttributeDescriptionCount=2,
        .pVertexAttributeDescriptions=vertex_input_attribute_descriptions
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable=VK_FALSE
    };
    VkPipelineViewportStateCreateInfo viewport_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .viewportCount=1,
        .pViewports=(VkViewport[1]){{
            .x=0,
            .y=0,
            .width=640,
            .height=480,
            .minDepth=0.0,
            .maxDepth=1.0
        }},
        .scissorCount=1,
        .pScissors=(VkRect2D[1]){{
            .offset={.x=0,.y=0},
            .extent={.height=480,.width=640},
        }}
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthClampEnable=VK_FALSE,
        .rasterizerDiscardEnable=VK_FALSE,
        .polygonMode=VK_POLYGON_MODE_FILL,
        .cullMode=VK_CULL_MODE_BACK_BIT,
        .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable=VK_FALSE,
        .depthBiasConstantFactor=0.0,
        .depthBiasClamp=0.0,
        .depthBiasSlopeFactor=0.0,
        .lineWidth=1.0
    };
    VkPipelineMultisampleStateCreateInfo multisample_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.0,
        .pSampleMask=NULL,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    };
    VkPipelineColorBlendAttachmentState subpass_attachment_color_blend_states={
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .logicOpEnable=VK_FALSE,
        .logicOp=VK_LOGIC_OP_COPY,
        .attachmentCount=subpass_num_attachments,
        .pAttachments=&subpass_attachment_color_blend_states,
        .blendConstants={0.0,0.0,0.0,0.0}
    };
    VkPipelineDynamicStateCreateInfo dynamic_state={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .dynamicStateCount=0,
        .pDynamicStates=NULL
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info={
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .stageCount=2,
        .pStages=(VkPipelineShaderStageCreateInfo[2]){
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //VkStructureType
                .pNext=NULL, //const void*
                .flags=0, //VkPipelineShaderStageCreateFlags
                .stage=VK_SHADER_STAGE_VERTEX_BIT, //VkShaderStageFlagBits
                .module=shader->vertex_shader, //VkShaderModule
                .pName="main", //const char*
                .pSpecializationInfo=NULL, //const VkSpecializationInfo*
            },
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //VkStructureType
                .pNext=NULL, //const void*
                .flags=0, //VkPipelineShaderStageCreateFlags
                .stage=VK_SHADER_STAGE_FRAGMENT_BIT, //VkShaderStageFlagBits
                .module=shader->fragment_shader, //VkShaderModule
                .pName="main", //const char*
                .pSpecializationInfo=NULL, //const VkSpecializationInfo*
            }
        },
        .pVertexInputState=&vertex_input_state,
        .pInputAssemblyState=&input_assembly_state,
        .pTessellationState=NULL,
        .pViewportState=&viewport_state,
        .pRasterizationState=&rasterization_state,
        .pMultisampleState=&multisample_state,
        .pDepthStencilState=NULL,
        .pColorBlendState=&color_blend_state,
        .pDynamicState=&dynamic_state,
        .layout=shader->pipeline_layout,
        .renderPass=app->render_pass,
        .subpass=subpass,
        .basePipelineHandle=NULL,
        .basePipelineIndex=-1
    };
    vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, app->vk_allocator, &shader->pipeline);

    return shader;
}
void App_destroy_shader(Shader* shader){
    vkDestroyDescriptorPool(shader->app->device, shader->descriptor_pool, shader->app->vk_allocator);
    vkDestroyDescriptorSetLayout(shader->app->device, shader->set_layout, shader->app->vk_allocator);

    vkDestroyPipeline(shader->app->device,shader->pipeline,shader->app->vk_allocator);
    vkDestroyPipelineLayout(shader->app->device, shader->pipeline_layout, shader->app->vk_allocator);

    vkDestroyShaderModule(shader->app->device,shader->fragment_shader,shader->app->vk_allocator);
    vkDestroyShaderModule(shader->app->device,shader->vertex_shader,shader->app->vk_allocator);

    free(shader);
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
        exit(XCB_CHANGE_PROPERTY_FAILURE);
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

    // look for fit physical device, and required queue families
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

    app->graphics_queue_family_index=graphics_queue_family_index;
    app->present_queue_family_index=present_queue_family_index;

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
        exit(VULKAN_CREATE_DEVICE_FAILURE);
    }
    free(queue_create_infos);

    // if these are the same queue family, they will just point to the same queue, which is fine
    vkGetDeviceQueue(app->device, graphics_queue_family_index, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, present_queue_family_index, 0, &app->present_queue);

    uint32_t num_surface_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->window_surface, &num_surface_formats, NULL);
    VkSurfaceFormatKHR *surface_formats=malloc(num_surface_formats*sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->window_surface, &num_surface_formats, surface_formats);
    app->swapchain_format=surface_formats[0];
    free(surface_formats);
    // get surface present mode

    VkSurfaceCapabilitiesKHR surface_capabilities;
    res=vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device,app->window_surface,&surface_capabilities);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with %d\n",res);
        exit(VULKAN_GET_PHYSICAL_DEVICE_SURFACE_CAPABILITIES_KHR_FAILURE);
    }

    uint32_t num_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->window_surface, &num_present_modes, NULL);
    VkPresentModeKHR* present_modes=malloc(num_present_modes*sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->window_surface, &num_present_modes, present_modes);
    VkPresentModeKHR swapchain_present_mode=present_modes[0];
    free(present_modes);

    int swapchain_image_count=1;
    if(surface_capabilities.minImageCount>0){
        swapchain_image_count=surface_capabilities.minImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info={
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .surface=app->window_surface,
        .minImageCount=swapchain_image_count,
        .imageFormat=app->swapchain_format.format,
        .imageColorSpace=app->swapchain_format.colorSpace,
        .imageExtent=surface_capabilities.currentExtent,
        .imageArrayLayers=1,
        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL,
        .preTransform=surface_capabilities.currentTransform,
        .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode=swapchain_present_mode,
        .clipped=VK_TRUE,
        .oldSwapchain=VK_NULL_HANDLE
    };
    vkCreateSwapchainKHR(app->device, &swapchain_create_info, app->vk_allocator, &app->swapchain);

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->num_swapchain_images, NULL);
    app->swapchain_images=malloc(app->num_swapchain_images*sizeof(VkImage));
    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->num_swapchain_images, app->swapchain_images);

    app->swapchain_image_views=malloc(app->num_swapchain_images*sizeof(VkImageView));
    app->swapchain_framebuffers=malloc(app->num_swapchain_images*sizeof(VkFramebuffer));

    uint32_t num_render_pass_attachments=1;
    VkAttachmentDescription render_pass_attachments[1]={
        {
                /*VkAttachmentDescriptionFlags*/    .flags=0,
                /*VkFormat*/                        .format=app->swapchain_format.format,
                /*VkSampleCountFlagBits*/           .samples=VK_SAMPLE_COUNT_1_BIT,
                /*VkAttachmentLoadOp*/              .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
                /*VkAttachmentStoreOp*/             .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
                /*VkAttachmentLoadOp*/              .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                /*VkAttachmentStoreOp*/             .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
                /*VkImageLayout*/                   .initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                /*VkImageLayout*/                   .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };
    uint32_t num_render_pass_subpass=1;
    VkSubpassDescription render_pass_subpasses[1]={
        {
            /*VkSubpassDescriptionFlags */      .flags=0,
            /*VkPipelineBindPoint */            .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
            /*uint32_t */                       .inputAttachmentCount=0,
            /*const VkAttachmentReference**/    .pInputAttachments=NULL,
            /*uint32_t */                       .colorAttachmentCount=1,
            /*const VkAttachmentReference**/    .pColorAttachments=(VkAttachmentReference[1]){{
                /*uint32_t*/         .attachment=0,
                /*VkImageLayout*/    .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }},
            /*const VkAttachmentReference**/    .pResolveAttachments=NULL,
            /*const VkAttachmentReference**/    .pDepthStencilAttachment=NULL,
            /*uint32_t */                       .preserveAttachmentCount=0,
            /*const uint32_t**/                 .pPreserveAttachments=NULL,
        }
    };
    VkRenderPassCreateInfo render_pass_create_info={
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .attachmentCount=num_render_pass_attachments,
        .pAttachments=render_pass_attachments,
        .subpassCount=num_render_pass_subpass,
        .pSubpasses=render_pass_subpasses,
        .dependencyCount=2,
        .pDependencies=(VkSubpassDependency[2]){
            {
                .srcSubpass= VK_SUBPASS_EXTERNAL,
                .dstSubpass= 0,
                .srcStageMask= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask= VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags= VK_DEPENDENCY_BY_REGION_BIT
            },
            {
                .srcSubpass= 0, 
                .dstSubpass= VK_SUBPASS_EXTERNAL, 
                .srcStageMask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
                .dstStageMask= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                .srcAccessMask= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 
                .dstAccessMask= VK_ACCESS_MEMORY_READ_BIT, 
                .dependencyFlags= VK_DEPENDENCY_BY_REGION_BIT
            }
        }
    };
    res=vkCreateRenderPass(app->device,&render_pass_create_info,app->vk_allocator,&app->render_pass);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create renderpass\n");
        exit(VULKAN_CREATE_RENDER_PASS_FAILURE);
    }

    VkImageViewCreateInfo image_view_create_info={
        .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .image=VK_NULL_HANDLE,
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=app->swapchain_format.format,
        .components={
            .r=VK_COMPONENT_SWIZZLE_IDENTITY,
            .g=VK_COMPONENT_SWIZZLE_IDENTITY,
            .b=VK_COMPONENT_SWIZZLE_IDENTITY,
            .a=VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange={
            .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };
    VkFramebufferCreateInfo framebuffer_create_info={
        .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .renderPass=app->render_pass,
        .attachmentCount=1,
        .pAttachments=NULL,
        .width=surface_capabilities.currentExtent.width,
        .height=surface_capabilities.currentExtent.height,
        .layers=1
    };
    for(uint32_t i=0;i<app->num_swapchain_images;i++){
        image_view_create_info.image=app->swapchain_images[i];
        res=vkCreateImageView(app->device, &image_view_create_info, app->vk_allocator, &app->swapchain_image_views[i]);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to create image view for swapchain image %d\n",i);
        }

        framebuffer_create_info.pAttachments=&app->swapchain_image_views[i];
        res=vkCreateFramebuffer(app->device,&framebuffer_create_info,app->vk_allocator,&app->swapchain_framebuffers[i]);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to create framebuffer for swapchain image %d\n",i);
        }
    }

    app->shader=App_create_shader(app,0,render_pass_create_info.subpassCount);

    return app;
}
/// destroy an app
///
/// frees owned memory
void App_destroy(Application* app){
    if(app->instance!=VK_NULL_HANDLE){
        if(app->device!=VK_NULL_HANDLE){
            vkDeviceWaitIdle(app->device);

            App_destroy_shader(app->shader);

            for(uint32_t i=0;i<app->num_swapchain_images;i++){
                vkDestroyFramebuffer(app->device,app->swapchain_framebuffers[i],app->vk_allocator);
                vkDestroyImageView(app->device,app->swapchain_image_views[i],app->vk_allocator);
            }

            vkDestroyRenderPass(app->device,app->render_pass,app->vk_allocator);
            
            vkDestroySwapchainKHR(app->device, app->swapchain, app->vk_allocator);

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
    VkResult res;

    VkSemaphoreCreateInfo semaphore_create_info={
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext=NULL,
        .flags=0
    };
    VkSemaphore image_available_semaphore;
    res=vkCreateSemaphore(app->device,&semaphore_create_info,app->vk_allocator,&image_available_semaphore);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create semaphore\n");
        exit(VULKAN_CREATE_SEMAPHORE_FAILURE);
    }
    VkSemaphore rendering_finished_semaphore;
    res=vkCreateSemaphore(app->device,&semaphore_create_info,app->vk_allocator,&rendering_finished_semaphore);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create semaphore\n");
        exit(VULKAN_CREATE_SEMAPHORE_FAILURE);
    }

    VkCommandPoolCreateInfo command_pool_create_info={
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex=app->graphics_queue_family_index
    };
    VkCommandPool command_pool;
    res=vkCreateCommandPool(app->device, &command_pool_create_info, app->vk_allocator, &command_pool);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to create command pool\n");
        exit(VULKAN_CREATE_COMMAND_POOL_FAILURE);
    }

    VkCommandBufferAllocateInfo command_buffer_allocate_info={
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext=NULL,
        .commandPool=command_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1
    };
    VkCommandBuffer* command_buffers=malloc(command_buffer_allocate_info.commandBufferCount*sizeof(VkCommandBuffer));
    res=vkAllocateCommandBuffers(app->device, &command_buffer_allocate_info, command_buffers);
    if(res!=VK_SUCCESS){
        fprintf(stderr,"failed to allocate command buffers\n");
        exit(VULKAN_ALLOCATE_COMMAND_BUFFERS_FAILURE);
    }

    VkCommandBuffer command_buffer=command_buffers[0];

    Mesh* quadmesh=NULL;

    int frame=0;
    int window_should_close=0;
    while(1){
        if(window_should_close){
            break;
        }

        xcb_generic_event_t* event;
        while((event=xcb_poll_for_event(app->connection))){
            // full sequence is essentially event id
            // sequence is the lower half of the event id
            uint8_t event_type=XCB_EVENT_RESPONSE_TYPE(event);
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
                    printf("got event %s\n",xcb_event_get_label(event_type));
            }
        }

        uint32_t next_swapchain_image_index;
        res=vkAcquireNextImageKHR(app->device, app->swapchain, 0xffffffffffffffff, image_available_semaphore, VK_NULL_HANDLE, &next_swapchain_image_index);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to acquire next swapchain image\n");
            exit(VULKAN_ACQUIRE_NEXT_IMAGE_KHR_FAILURE);
        }

        VkCommandBufferBeginInfo command_buffer_begin_info={
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext=NULL,
            .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo=NULL
        };
        vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        if(frame==0){
            quadmesh=App_upload_mesh(app, command_buffer, 4, mesh);
        }

        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            0, 
            0, NULL, 
            0, NULL, 
            1, (VkImageMemoryBarrier[1]){{
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_MEMORY_READ_BIT,  
                .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  
                .oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,  
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  
                .srcQueueFamilyIndex=app->present_queue_family_index,  
                .dstQueueFamilyIndex=app->present_queue_family_index,  
                .image=app->swapchain_images[next_swapchain_image_index], 
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1 
                }
            }}
        );

        VkRenderPassBeginInfo render_pass_begin_info={
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext=NULL,
            .renderPass=app->render_pass,
            .framebuffer=app->swapchain_framebuffers[next_swapchain_image_index],
            .renderArea={
                .offset={.x=0,.y=0},
                .extent={.width=640,.height=480}
            },
            .clearValueCount=1,
            .pClearValues=(VkClearValue[1]){{
                .color={.float32={1.0,1.0,1.0,1.0}}
            }}
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->shader->pipeline);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, (VkBuffer[1]){quadmesh->buffer}, (VkDeviceSize[1]){0});
        vkCmdDraw(command_buffer, 4, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            0, 
            0, NULL, 
            0, NULL, 
            1, (VkImageMemoryBarrier[1]){{
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext=NULL,
                .srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex=app->present_queue_family_index,
                .dstQueueFamilyIndex=app->present_queue_family_index,
                .image=app->swapchain_images[next_swapchain_image_index],
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1 
                }
            }}
        );

        res=vkEndCommandBuffer(command_buffer);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to end command buffer\n");
            exit(VULKAN_END_COMMAND_BUFFER_FAILURE);
        }

        res=vkQueueSubmit(app->present_queue,1,(VkSubmitInfo[1]){{
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=&image_available_semaphore,
            .pWaitDstStageMask=(VkPipelineStageFlags[1]){VK_PIPELINE_STAGE_TRANSFER_BIT},
            .commandBufferCount=1,
            .pCommandBuffers=&command_buffer,
            .signalSemaphoreCount=1,
            .pSignalSemaphores=&rendering_finished_semaphore
        }},VK_NULL_HANDLE);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to submit swapchain presentation image\n");
            exit(VULKAN_QUEUE_SUBMIT_FAILURE);
        }

        VkPresentInfoKHR swapchain_present_info={
            .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext=NULL,
            .waitSemaphoreCount=1,
            .pWaitSemaphores=(VkSemaphore[1]){rendering_finished_semaphore},
            .swapchainCount=1,
            .pSwapchains=(VkSwapchainKHR[1]){app->swapchain},
            .pImageIndices=(uint32_t[1]){next_swapchain_image_index},
            NULL
        };
        res=vkQueuePresentKHR(app->present_queue, &swapchain_present_info);
        if(res!=VK_SUCCESS){
            fprintf(stderr,"failed to present swapchain image\n");
            exit(VULKAN_QUEUE_PRESENT_KHR_FAILURE);
        }

        vkDeviceWaitIdle(app->device);

        struct timespec time_to_sleep={
            .tv_nsec=33000000,
            .tv_sec=0
        };
        nanosleep(&time_to_sleep, NULL);

        frame+=1;
        discard frame;
    }

    App_destroy_mesh(app,quadmesh);

    vkDestroySemaphore(app->device,image_available_semaphore,app->vk_allocator);
    vkDestroySemaphore(app->device,rendering_finished_semaphore,app->vk_allocator);
    vkFreeCommandBuffers(app->device, command_pool, command_buffer_allocate_info.commandBufferCount, command_buffers);
    vkDestroyCommandPool(app->device,command_pool,app->vk_allocator);

    free(command_buffers);
}

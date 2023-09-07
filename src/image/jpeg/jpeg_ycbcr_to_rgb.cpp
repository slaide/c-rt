#ifdef  VK_USE_PLATFORM_XCB_KHR
    #include "jpeg_x64.cpp"
#elif defined( VK_USE_PLATFORM_METAL_EXT)
    #include "jpeg_arm64.cpp"
#endif

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb(
    const JpegParser* const  parser,
    const uint32_t mcu_row
){
    const ImageComponent image_components[3]={
        parser->image_components[0],
        parser->image_components[1],
        parser->image_components[2]
    };

    uint32_t pixels_in_scan=image_components[0].horz_samples*8*image_components[0].vert_sample_factor;
    uint32_t scan_offset=mcu_row*pixels_in_scan;

    uint8_t* const  image_data_data=parser->image_data->data+scan_offset*4;

    const uint32_t rescale_factor[3]={
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const  y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const  cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const  cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i++) {
        #ifdef USE_FLOAT_PRECISION
            // -- re-order from block-orientation to final image orientation

            const OUT_EL Y=y[image_components[0].conversion_indices[i]];
            const OUT_EL Cr=cr[image_components[1].conversion_indices[i]];
            const OUT_EL Cb=cb[image_components[2].conversion_indices[i]];

            // -- convert ycbcr to rgb

            const OUT_EL R = Y +                1.402f * Cr;
            const OUT_EL B = Y +  1.772f * Cb;
            const OUT_EL G = Y - (0.343f * Cb + 0.718f * Cr );

            // -- deinterlace and convert to uint8

            image_data_data[i* 4 + 0] = static_cast<uint8_t>(bitUtil::clamp(0.0f,255.0f,R+128.0f));
            image_data_data[i* 4 + 1] = static_cast<uint8_t>(bitUtil::clamp(0.0f,255.0f,G+128.0f));
            image_data_data[i* 4 + 2] = static_cast<uint8_t>(bitUtil::clamp(0.0f,255.0f,B+128.0f));
            image_data_data[i* 4 + 3] = UINT8_MAX;
        #else
            // -- re-order from block-orientation to final image orientation

            const OUT_EL Y= static_cast<OUT_EL>( y [image_components[0].conversion_indices[i]]     >>PRECISION);
            const OUT_EL Cr=static_cast<OUT_EL>((cr[image_components[1].conversion_indices[i]]-128)>>PRECISION);
            const OUT_EL Cb=static_cast<OUT_EL>((cb[image_components[2].conversion_indices[i]]-128)>>PRECISION);

            // -- convert ycbcr to rgb

            const OUT_EL R = static_cast<OUT_EL>(Y + ((            45 * Cr ) >> 5 ));
            const OUT_EL B = static_cast<OUT_EL>(Y + (( 113 * Cb           ) >> 6 ));
            const OUT_EL G = static_cast<OUT_EL>(Y - ((  11 * Cb + 23 * Cr ) >> 5 ));

            // -- deinterlace and convert to uint8

            image_data_data[i* 4 + 0] = static_cast<uint8_t>(bitUtil::clamp(0,255,R+128));
            image_data_data[i* 4 + 1] = static_cast<uint8_t>(bitUtil::clamp(0,255,G+128));
            image_data_data[i* 4 + 2] = static_cast<uint8_t>(bitUtil::clamp(0,255,B+128));
            image_data_data[i* 4 + 3] = UINT8_MAX;
        #endif
    }
}

[[gnu::flatten,gnu::hot,gnu::nonnull(1)]]
static void JpegParser_convert_colorspace(
    JpegParser* const  parser,
    const uint32_t scan_index_start,
    const uint32_t scan_index_end
){
    if (parser->component_label==0x221111){
        for (uint32_t s=scan_index_start; s<scan_index_end; s++){
            #ifdef  USE_FLOAT_PRECISION
                #ifdef VK_USE_PLATFORM_METAL_EXT
                    scan_ycbcr_to_rgb_neon_float(parser,s);
                #elif defined( VK_USE_PLATFORM_XCB_KHR)
                    scan_ycbcr_to_rgb_sse_float(parser,s);
                #endif
            #else
                #ifdef VK_USE_PLATFORM_METAL_EXT
                    scan_ycbcr_to_rgb_neon_fixed(parser,s);
                #elif defined( VK_USE_PLATFORM_XCB_KHR)
                    scan_ycbcr_to_rgb_sse_fixed(parser,s);
                #endif
            #endif
        }

        return;
    }

    for (uint32_t s=scan_index_start; s<scan_index_end; s++) {
        scan_ycbcr_to_rgb(parser, s);
    }
}

#ifdef USE_FLOAT_PRECISION

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb_neon_float(
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

    uint8_t* const image_data_data=parser->image_data->data+scan_offset*4;

    const uint32_t rescale_factor[3]={
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i+=4) {
        // -- re-order from block-orientation to final image orientation

        const float32x4_t y_simd=vld1q_f32(&y[image_components[0].conversion_indices[i]]);
        float32x4_t cr_simd=vld1q_f32(&cr[image_components[1].conversion_indices[i]]);
        cr_simd=vzip1q_f32(cr_simd,cr_simd);
        float32x4_t cb_simd=vld1q_f32(&cb[image_components[2].conversion_indices[i]]);
        cb_simd=vzip1q_f32(cb_simd,cb_simd);

        // -- convert ycbcr to rgb

        float32x4_t r_simd=y_simd+1.402f*cr_simd;
        float32x4_t b_simd=y_simd+1.772f*cb_simd;
        float32x4_t g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

        float32x4_t v_simd;

        v_simd=vdupq_n_f32(128.0);
        r_simd+=v_simd;
        g_simd+=v_simd;
        b_simd+=v_simd;
        
        v_simd=vdupq_n_f32(0.0);
        r_simd=vmaxq_f32(r_simd, v_simd);
        g_simd=vmaxq_f32(g_simd, v_simd);
        b_simd=vmaxq_f32(b_simd, v_simd);
        
        v_simd=vdupq_n_f32(255.0);
        r_simd=vminq_f32(r_simd, v_simd);
        g_simd=vminq_f32(g_simd, v_simd);
        b_simd=vminq_f32(b_simd, v_simd);

        const int32x4_t r_s32=vcvtq_s32_f32(r_simd);
        const uint16x4_t r_u16=vqmovun_s32(r_s32);
        const int32x4_t g_s32=vcvtq_s32_f32(g_simd);
        const uint16x4_t g_u16=vqmovun_s32(g_s32);
        const int32x4_t b_s32=vcvtq_s32_f32(b_simd);
        const uint16x4_t b_u16=vqmovun_s32(b_s32);

        const uint16x4_t a_u16=vdup_n_u16(255);

        const uint16x8_t rg_u16=vcombine_u16(r_u16,g_u16);
        const uint16x8_t ba_u16=vcombine_u16(b_u16,a_u16);

        const uint8x8_t rg_u8=vqmovn_u16(ba_u16);
        const uint8x8_t ba_u8=vqmovn_u16(rg_u16);

        uint8x16_t rgba_u8=vcombine_u8(ba_u8,rg_u8);

        // -- deinterlace and convert to uint8

        static const uint8_t indices [[gnu::aligned(16)]] [16] = {
            0, 4, 8, 12, 
            1, 5, 9, 13, 
            2, 6, 10, 14, 
            3, 7, 11, 15
        };
        const uint8x16_t indices_vector = vld1q_u8(indices);

        rgba_u8=vqtbl1q_u8(rgba_u8, indices_vector);

        vst1q_u8(image_data_data+i*4,rgba_u8);
    }
}

#else

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb_neon_fixed(
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

    uint8_t* const image_data_data=parser->image_data->data+scan_offset*4;

    const uint32_t rescale_factor[3]={
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        (uint32_t)parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i+=4) {
        // -- re-order from block-orientation to final image orientation

        int16x8_t y_simd=vld1q_s16(&y[image_components[0].conversion_indices[i]]);
        int16x8_t cr_simd=vld1q_s16(&cr[image_components[1].conversion_indices[i]]);
        cr_simd=vzip1q_s16(cr_simd,cr_simd);
        int16x8_t cb_simd=vld1q_s16(&cb[image_components[2].conversion_indices[i]]);
        cb_simd=vzip1q_s16(cb_simd,cb_simd);

        y_simd=vshrq_n_s16(y_simd,PRECISION);
        cr_simd=vshrq_n_s16(cr_simd,PRECISION);
        cb_simd=vshrq_n_s16(cb_simd,PRECISION);

        // -- convert ycbcr to rgb

        int16x8_t r_simd = vaddq_s16(y_simd, vshrq_n_s16(vmulq_n_s16(cr_simd, 45), 5));
        int16x8_t b_simd = vaddq_s16(y_simd, vshrq_n_s16(vmulq_n_s16(cb_simd, 113), 6));
        int16x8_t g_simd = vsubq_s16(y_simd, vshrq_n_s16(vaddq_s16(vmulq_n_s16(cb_simd, 11), vmulq_n_s16(cr_simd, 23)), 5));

        int16x8_t v_simd;

        v_simd=vdupq_n_s16(128);
        r_simd+=v_simd;
        g_simd+=v_simd;
        b_simd+=v_simd;
        
        v_simd=vdupq_n_s16(0);
        r_simd=vmaxq_s16(r_simd, v_simd);
        g_simd=vmaxq_s16(g_simd, v_simd);
        b_simd=vmaxq_s16(b_simd, v_simd);
        
        v_simd=vdupq_n_s16(255);
        r_simd=vminq_s16(r_simd, v_simd);
        g_simd=vminq_s16(g_simd, v_simd);
        b_simd=vminq_s16(b_simd, v_simd);

        // -- deinterlace and convert to uint8

        uint8x8_t r_u8x8=vqmovn_u16(vreinterpretq_s16_u16(r_simd));
        uint8x8_t g_u8x8=vqmovn_u16(vreinterpretq_s16_u16(g_simd));
        uint8x8_t b_u8x8=vqmovn_u16(vreinterpretq_s16_u16(b_simd));

        uint8x16_t r_u8=vcombine_u8(r_u8x8, r_u8x8);
        uint8x16_t g_u8=vcombine_u8(g_u8x8, g_u8x8);
        uint8x16_t b_u8=vcombine_u8(b_u8x8, b_u8x8);
        uint8x16_t a_u8=vdupq_n_u8(255);

        uint8x16_t rb_u8=vzip1q_u8(r_u8, b_u8);
        uint8x16_t ga_u8=vzip1q_u8(g_u8, a_u8);

        uint8x16_t o1=vzip1q_u8(rb_u8,ga_u8);
        uint8x16_t o2=vzip2q_u8(rb_u8,ga_u8);

        uint8_t* output_ptr = &image_data_data[i * 4];
        vst1q_u8(output_ptr, o1);
        output_ptr = &image_data_data[(i+4) * 4];
        vst1q_u8(output_ptr, o2);
    }
}

#endif

#include <stdint.h>
#include <x86intrin.h>

#ifdef USE_FLOAT_PRECISION

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
void scan_ycbcr_to_rgb_sse_float(
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
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const  y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const  cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const  cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i+=4) {
        // -- re-order from block-orientation to final image orientation

        __m128 y_simd=_mm_loadu_ps(&y[image_components[0].conversion_indices[i]]);
        __m128 cr_simd=_mm_loadu_ps(&cr[image_components[1].conversion_indices[i]]);
        cr_simd=_mm_shuffle_epi32(cr_simd,(1<<4)+(1<<6));
        __m128 cb_simd=_mm_loadu_ps(&cb[image_components[2].conversion_indices[i]]);
        cb_simd=_mm_shuffle_epi32(cb_simd,(1<<4)+(1<<6));

        // -- convert ycbcr to rgb

        __m128 r_simd=y_simd+1.402f*cr_simd;
        __m128 b_simd=y_simd+1.772f*cb_simd;
        __m128 g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

        const __m128 v_simd=_mm_set1_ps(128.0);

        r_simd+=v_simd;
        g_simd+=v_simd;
        b_simd+=v_simd;

        // conversion from i32->i16->u8 is clamping, i.e. clamp to [0;255] is implicit

        const __m128i r_s32=_mm_cvtps_epi32(r_simd);
        const __m128i g_s32=_mm_cvtps_epi32(g_simd);
        const __m128i b_s32=_mm_cvtps_epi32(b_simd);
        const __m128i a_s32=_mm_set1_epi32(255);

        const __m128i rg_u16=_mm_packs_epi32(r_s32,g_s32);
        const __m128i ba_u16=_mm_packs_epi32(b_s32,a_s32);

        __m128i rgba_u8=_mm_packus_epi16(rg_u16,ba_u16);

        // -- deinterlace and convert to uint8

        static const uint8_t indices [[gnu::aligned(16)]] [16] = {
            0, 4, 8, 12, 
            1, 5, 9, 13, 
            2, 6, 10, 14, 
            3, 7, 11, 15
        };
        const __m128i indices_vector = _mm_load_ps((void*)indices);

        rgba_u8=_mm_shuffle_epi8(rgba_u8, indices_vector);

        _mm_storeu_si128((void*)(image_data_data+i*4),rgba_u8);
    }
}

#else

[[gnu::hot,gnu::flatten,gnu::nonnull(1),maybe_unused]]
static inline void scan_ycbcr_to_rgb_sse_fixed(
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

    for (uint32_t i=0; i<pixels_in_scan; i+=8) {
        // -- re-order from block-orientation to final image orientation

        const int16_t* y_ptr = &y[image_components[0].conversion_indices[i]];
        const int16_t* cr_ptr = &cr[image_components[1].conversion_indices[i]];
        const int16_t* cb_ptr = &cb[image_components[2].conversion_indices[i]];

        // Load 8 Y, Cr, and Cb values
        __m128i y_values = _mm_loadu_si128((__m128i*)y_ptr);
        __m128i cr_values = _mm_loadu_si128((__m128i*)cr_ptr);
        cr_values=_mm_unpacklo_epi16(cr_values,cr_values);
        __m128i cb_values = _mm_loadu_si128((__m128i*)cb_ptr);
        cb_values=_mm_unpacklo_epi16(cb_values,cb_values);

        // Shift and subtract constants
        y_values = _mm_srai_epi16(y_values, PRECISION);
        cr_values = _mm_srai_epi16(cr_values, PRECISION);
        cb_values = _mm_srai_epi16(cb_values, PRECISION);

        // Constants for RGB conversion
        const __m128i const_45 = _mm_set1_epi16(45);
        const __m128i const_113 = _mm_set1_epi16(113);
        const __m128i const_11 = _mm_set1_epi16(11);
        const __m128i const_23 = _mm_set1_epi16(23);

        // Calculate R, G, and B values
        __m128i R = _mm_add_epi16(y_values, _mm_srai_epi16(_mm_mullo_epi16(const_45, cr_values), 5));
        __m128i B = _mm_add_epi16(y_values, _mm_srai_epi16(_mm_mullo_epi16(const_113, cb_values), 6));
        __m128i G = _mm_sub_epi16(y_values, _mm_srai_epi16(_mm_add_epi16(_mm_mullo_epi16(const_11, cb_values), _mm_mullo_epi16(const_23, cr_values)), 5));
        __m128i A = _mm_set1_epi16(UINT8_MAX);

        // Add offset and store as uint8_t values
        const __m128i offset = _mm_set1_epi16(128);
        R=_mm_add_epi16(R, offset);
        G=_mm_add_epi16(G, offset);
        B=_mm_add_epi16(B, offset);

        const __m128i r_u8 = _mm_packus_epi16(R,R);
        const __m128i g_u8 = _mm_packus_epi16(G,G);
        const __m128i b_u8 = _mm_packus_epi16(B,B);
        const __m128i a_u8 = _mm_packus_epi16(A,A);

        const __m128i rb_u8=_mm_unpacklo_epi8(r_u8,b_u8);
        const __m128i ga_u8=_mm_unpacklo_epi8(g_u8,a_u8);

        const __m128i o1=_mm_unpacklo_epi8(rb_u8,ga_u8);
        const __m128i o2=_mm_unpackhi_epi8(rb_u8,ga_u8);

        // Store as uint8_t values
        uint8_t* output_ptr = &image_data_data[i * 4];
        _mm_storeu_si128((__m128i*)output_ptr, o1);
        output_ptr = &image_data_data[(i+4) * 4];
        _mm_storeu_si128((__m128i*)output_ptr, o2);

    }
}

#endif

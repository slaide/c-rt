#include <mmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef VK_USE_PLATFORM_XCB_KHR
    #include<jemalloc/jemalloc.h>
#endif

#ifdef VK_USE_PLATFORM_METAL_EXT
#include <arm_neon.h>
#elif VK_USE_PLATFORM_XCB_KHR
#include <emmintrin.h>
#include <xmmintrin.h>
#include <immintrin.h> // avx2
#endif

#include <time.h>

#include "app/app.h"
#include "app/error.h"
#include "app/huffman.h"

#define println(...) {\
    printf("%s : %d | ",__FILE__,__LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
}

typedef void*(*pthread_callback)(void*);

#define MAX_NSEC 999999999

static double current_time(){
    struct timespec current_time;
    int time_get_result=clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (time_get_result != 0) {
        fprintf(stderr, "failed to get start time because %d\n",time_get_result);
        exit(-66);
    }

    double ret=(double)current_time.tv_sec;
    ret+=((double)current_time.tv_nsec)/(double)(MAX_NSEC+1);
    return ret;
}

[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline uint32_t max_u32(const uint32_t a,const uint32_t b){
    return (a>b)?a:b;
}
[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline int32_t max_i32(const int32_t a,const int32_t b){
    return (a>b)?a:b;
}
#define max(a,b) (_Generic((a), int32_t: max_i32, uint32_t: max_u32, float:fmaxf, double:fmax)(a,b))

[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline uint32_t min_u32(const uint32_t a,const uint32_t b){
    return (a<b)?a:b;
}
[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline int32_t min_i32(const int32_t a,const int32_t b){
    return (a<b)?a:b;
}
#define min(a,b) (_Generic((a), int32_t: min_i32, uint32_t: min_u32, float:fminf, double:fmin)(a,b))

[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot,maybe_unused]]
static inline float clamp_f32(const float v_min,const float v_max,const float v){
    return fmaxf(v_min, fminf(v_max, v));
}
[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,maybe_unused]]
static inline int32_t clamp_i32(const int32_t v_min,const int32_t v_max, const int32_t v){
    return max(v_min, min(v_max, v));
}
#define clamp(v_min,v_max,v) (_Generic((v), float: clamp_f32, int32_t: clamp_i32)(v_min,v_max,v))

[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline int32_t twos_complement_32(const uint32_t magnitude, const int32_t value){
    int32_t threshold=1<<(magnitude-1);
    if (value<threshold){
        int32_t ret=value+1;
        ret-=1<<magnitude;
        return ret;
    }

    return value;
}
[[gnu::pure,gnu::const,clang::always_inline,gnu::flatten,gnu::hot]]
static inline int16_t twos_complement_16(const uint16_t magnitude, const int16_t value){
    int16_t threshold=(int16_t)(1<<(magnitude-1));
    if (value<threshold){
        int16_t ret=value+1;
        ret-=1<<magnitude;
        return ret;
    }

    return value;
}
#define twos_complement(a,b) (_Generic(a, uint32_t:twos_complement_32,uint16_t:twos_complement_16)((a),(b)))

typedef enum JpegSegmentType{
    JPEG_SEGMENT_SOI=0xFFD8,
    JPEG_SEGMENT_EOI=0xFFD9,

    SOF0=0xFFC0,
    SOF1=0xFFC1,
    SOF2=0xFFC2,
    SOF3=0xFFC3,
    SOF5=0xFFC5,
    SOF6=0xFFC6,
    SOF7=0xFFC7,
    SOF8=0xFFC8,
    SOF9=0xFFC9,
    SOF10=0xFFCA,
    SOF11=0xFFCB,
    SOF13=0xFFCD,
    SOF14=0xFFCE,
    SOF15=0xFFCF,

    /// define huffman tables
    DHT=0xFFC4,
    /// define arithmetic coding conditions
    DAC=0xFFCC,

    /// start of scan
    SOS=0xFFDA,

    /// define quantization tables
    DQT=0xFFDB,
    /// define number of lines
    DNL=0xFFDC,
    /// define restart inverval
    DRI=0xFFDD,
    /// define hierachical progression
    DHP=0xFFDE,
    /// expand reference components
    EXP=0xFFDF,
    /// comment
    COM=0xFFFE,

    APP0=0xFFE0,
    APP1=0xFFE1,
    APP2=0xFFE2,
    APP3=0xFFE3,
    APP4=0xFFE4,
    APP5=0xFFE5,
    APP6=0xFFE6,
    APP7=0xFFE7,
    APP8=0xFFE8,
    APP9=0xFFE9,
    APP10=0xFFEA,
    APP11=0xFFEB,
    APP12=0xFFEC,
    APP13=0xFFED,
    APP14=0xFFEE,
    APP15=0xFFEF
}JpegSegmentType;

const char* Image_jpeg_segment_type_name(JpegSegmentType segment_type){
    #define CASE(CASE_NAME) case CASE_NAME: return #CASE_NAME;

    switch (segment_type) {
        CASE(JPEG_SEGMENT_SOI)
        CASE(JPEG_SEGMENT_EOI)

        CASE(SOF0)
        CASE(SOF1)
        CASE(SOF2)
        CASE(SOF3)
        CASE(SOF5)
        CASE(SOF6)
        CASE(SOF7)
        CASE(SOF8)
        CASE(SOF9)
        CASE(SOF10)
        CASE(SOF11)
        CASE(SOF13)
        CASE(SOF14)
        CASE(SOF15)
        CASE(DHT)
        CASE(DAC)
        CASE(SOS)
        CASE(DQT)
        CASE(DNL)
        CASE(DRI)
        CASE(DHP)
        CASE(EXP)
        CASE(COM)
        CASE(APP0)
        CASE(APP1)
        CASE(APP2)
        CASE(APP3)
        CASE(APP4)
        CASE(APP5)
        CASE(APP6)
        CASE(APP7)
        CASE(APP8)
        CASE(APP9)
        CASE(APP10)
        CASE(APP11)
        CASE(APP12)
        CASE(APP13)
        CASE(APP14)
        CASE(APP15)
    }

    return NULL;
}

typedef int16_t MCU_EL;
//#define FIXED_PRECISION_ARITHMETIC
#ifdef FIXED_PRECISION_ARITHMETIC
    #define PRECISION 8
    typedef int32_t OUT_EL; // 16bits are kinda enough, but some images then peak on individual pixels (i.e. random pixels are white)
#else
    typedef float OUT_EL;
#endif

typedef OUT_EL QUANT;
typedef QUANT QuantizationTable[64];

static const uint8_t ZIGZAG[64]={
    0,  1,  5,  6,  14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63,
};
[[maybe_unused]]
static const int UNZIGZAG[64]={
        0,  1,  8,  16, 9,  2,  3,  10,
        17, 24, 32, 25, 18, 11, 4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6,  7,  14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
};

typedef struct ImageComponent{
    uint32_t vert_samples;
    uint32_t horz_samples;

    uint8_t component_id;
    uint8_t vert_sample_factor;
    uint8_t horz_sample_factor;
    uint8_t quant_table_specifier;

    uint32_t num_scans;
    uint32_t num_blocks_in_scan;

    uint32_t total_num_blocks;

    /**
     * @brief decompressed scans, where each scan has its own memory
     * 
     */
    MCU_EL** scan_memory;

    /**
     * @brief 
     * 
     */
    OUT_EL* out_block_downsampled;

    uint32_t* conversion_indices;
}ImageComponent;

[[gnu::flatten,gnu::hot]]
static inline void decode_dc(
    MCU_EL* const restrict block_mem [[gnu::aligned(64)]],

    const HuffmanCodingTable* const restrict dc_table,
    MCU_EL* const restrict diff_dc,

    BitStream* const restrict stream,

    const uint8_t successive_approximation_bit_low
){
    uint8_t dc_magnitude=HuffmanCodingTable_lookup(dc_table, stream);

    const MCU_EL lookahead_dc_value_bits=(MCU_EL)BitStream_get_bits(stream, 12);

    if (dc_magnitude>0) {
        const MCU_EL dc_value_bits=lookahead_dc_value_bits>>(12-dc_magnitude);
        BitStream_advance_unsafe(stream, dc_magnitude);
        
        MCU_EL dc_value=(MCU_EL)twos_complement((uint16_t)dc_magnitude, dc_value_bits);

        *diff_dc+=dc_value;
    }

    block_mem[0]=(MCU_EL)(*diff_dc<<successive_approximation_bit_low);
}
/**
 * @brief decode block with successive approximation high at zero
 * 
 * @param block_mem 
 * @param ac_table 
 * @param spectral_selection_start 
 * @param spectral_selection_end 
 * @param bit_stream 
 * @param successive_approximation_bit_low 
 * @param eob_run 
 */
[[clang::always_inline,gnu::flatten,gnu::hot]]
static inline void decode_block_ac(
    MCU_EL* const restrict block_mem [[gnu::aligned(64)]],

    const HuffmanCodingTable* const restrict ac_table,

    const uint8_t spectral_selection_start,
    const uint8_t spectral_selection_end,

    BitStream* const restrict stream,

    const uint8_t successive_approximation_bit_low,

    uint64_t* const restrict eob_run
){
    for(
        register int spec_sel=spectral_selection_start;
        spec_sel<=spectral_selection_end;
    ){
        const uint8_t ac_bits=HuffmanCodingTable_lookup(ac_table, stream);

        if (ac_bits==0) {
            break;
        }

        const uint8_t num_zeros=ac_bits>>4;
        const uint8_t ac_magnitude=ac_bits&0xF;

        if (ac_magnitude==0) {
            if (num_zeros==15) {
                spec_sel+=16;
                continue;
            }else{
                *eob_run=get_mask_u32(num_zeros);
                *eob_run+=BitStream_get_bits_advance(stream, num_zeros);

                break;
            }
        }

        spec_sel+=num_zeros;
        if (spec_sel>spectral_selection_end) {
            break;
        }

        const MCU_EL ac_value_bits=(MCU_EL)BitStream_get_bits_advance(stream,ac_magnitude);

        const MCU_EL ac_value=(MCU_EL)twos_complement((uint16_t)ac_magnitude,ac_value_bits);

        block_mem[spec_sel++]=(MCU_EL)(ac_value<<successive_approximation_bit_low);
    }
}

[[gnu::flatten,gnu::hot]]
static inline uint8_t refine_block(
    MCU_EL* const restrict block_mem [[gnu::aligned(64)]],

    BitStream* const restrict bit_stream,

    const uint8_t range_start,
    const uint8_t range_end,

    uint8_t num_zeros,
    const MCU_EL bit
){
    for(uint8_t index= range_start;index<=range_end;index++){
        if(block_mem[index]==0){
            if(num_zeros==0){
                return index;
            }

            num_zeros -= 1;
        }else{
            uint64_t next_bit=BitStream_get_bits_advance(bit_stream,1);
            if(next_bit==1 && (block_mem[index]&bit) == 0){
                if(block_mem[index]>0){
                    block_mem[index] += bit;
                }else{
                    block_mem[index] -= bit;
                }
            }
        }
    }

    return range_end;
}
/**
 * @brief decode block with successive approximation high set
 * 
 * @param block_mem 
 * @param ac_table 
 * @param spectral_selection_start 
 * @param spectral_selection_end 
 * @param bit_stream 
 * @param successive_approximation_bit_low 
 * @param successive_approximation_bit_high 
 * @param eob_run 
 */
[[gnu::flatten,gnu::hot]]
static inline void decode_block_with_sbh(
    MCU_EL* const restrict block_mem [[gnu::aligned(64)]],

    const HuffmanCodingTable* const restrict ac_table,

    const uint8_t spectral_selection_start,
    const uint8_t spectral_selection_end,

    BitStream* const restrict bit_stream,

    const MCU_EL succ_approx_bit_shifted,

    uint64_t* const restrict eob_run
){
    uint8_t next_pixel_index=spectral_selection_start;
    for(;next_pixel_index <= spectral_selection_end;){
        const uint8_t ac_bits=HuffmanCodingTable_lookup(ac_table, bit_stream);

        // 4 most significant bits are number of zero-value bytes inserted before actual value (may be zero)
        uint8_t num_zeros=ac_bits>>4;
        // no matter number of '0' bytes inserted, last 4 bits in ac value denote (additional) pixel value
        const uint32_t ac_magnitude=ac_bits&0xF;

        MCU_EL value=0;

        if(ac_magnitude==0){
            switch(num_zeros){
                case 15:
                    break; // num_zeros is 15, value is zero => 16 zeros written already
                default:
                    {
                        uint32_t eob_run_bits=(uint32_t)BitStream_get_bits_advance(bit_stream,num_zeros);
                        *eob_run=get_mask_u32(num_zeros) + eob_run_bits;
                    }
                    num_zeros=64;
                    break;
                case 0:
                    *eob_run=0;
                    num_zeros=64;
            }
        }else{
            if(BitStream_get_bits_advance(bit_stream,1)==1){
                value = succ_approx_bit_shifted;
            }else{
                value = -succ_approx_bit_shifted;
            }
        }

        next_pixel_index=refine_block(
            block_mem,
            bit_stream, 

            next_pixel_index, 
            spectral_selection_end, 

            num_zeros, 
            succ_approx_bit_shifted
        );

        if(value != 0){
            block_mem[next_pixel_index]=value;
        }
        
        next_pixel_index+=1;
    }
}

/**
 * @brief used internally for idct mask generation
 * 
 * @param u 
 * @return float 
 */
[[gnu::pure,gnu::const,gnu::hot]]
static inline float coeff(const uint32_t u){
    if(u==0){
        return 1.0f/(float)M_SQRT2;
    }else{
        return 1.0f;
    }
}

typedef struct IDCTMaskSet {
    OUT_EL idct_element_masks[64][64];
} IDCTMaskSet;
static inline void IDCTMaskSet_generate(
    IDCTMaskSet* const restrict mask_set
){
    for(uint32_t mask_index = 0;mask_index<64;mask_index++){
        for(uint32_t ix = 0;ix<8;ix++){
            for(uint32_t iy = 0;iy<8;iy++){
                uint32_t mask_u=mask_index%8;
                uint32_t mask_v=mask_index/8;

                float x_cos_arg = ((2.0f * (float)(iy) + 1.0f) * (float)(mask_u) * (float)M_PI) / 16.0f;
                float y_cos_arg = ((2.0f * (float)(ix) + 1.0f) * (float)(mask_v) * (float)M_PI) / 16.0f;

                float x_val = cosf(x_cos_arg) * coeff(mask_u);
                float y_val = cosf(y_cos_arg) * coeff(mask_v);
                // the divide by 4 comes from the spec, from the algorithm to reverse the application of the IDCT
                #ifdef FIXED_PRECISION_ARITHMETIC
                OUT_EL value = (OUT_EL)(((x_val * y_val)/4)*(1<<PRECISION));
                #else
                OUT_EL value = (x_val * y_val)/4;
                #endif

                uint32_t mask_pixel_index=8*ix+iy;
                mask_set->idct_element_masks[ZIGZAG[mask_index]][mask_pixel_index]=value;
            }
        }
    }
}

typedef struct JpegParser{
    double start_time;

    ImageData* image_data;

    uint64_t file_size;
    uint8_t* file_contents;
    uint32_t current_byte_position;

    QuantizationTable quant_tables[4];
    HuffmanCodingTable ac_coding_tables[4];
    HuffmanCodingTable dc_coding_tables[4];

    uint8_t max_component_vert_sample_factor;
    uint8_t max_component_horz_sample_factor;

    ImageComponent image_components[3];

    uint32_t P,X,Y,Nf;
    uint32_t real_X,real_Y;

    IDCTMaskSet idct_mask_set;

    uint32_t component_label;
    uint32_t color_space;
}JpegParser;

void JpegParser_init_empty(JpegParser* restrict parser){
    parser->file_size=0;
    parser->file_contents=NULL;
    parser->current_byte_position=0;

    for(int i=0;i<4;i++){
        for(int j=0;j<64;j++)
            parser->quant_tables[i][j]=0;

        parser->ac_coding_tables[i].lookup_table=NULL;
        parser->ac_coding_tables[i].max_code_length_bits=0;

        parser->dc_coding_tables[i].lookup_table=NULL;
        parser->dc_coding_tables[i].max_code_length_bits=0;
    };

    parser->max_component_horz_sample_factor=0;
    parser->max_component_vert_sample_factor=0;

    for(int i=0;i<3;i++){    
        parser->image_components[i].component_id=0;
        parser->image_components[i].horz_sample_factor=0;
        parser->image_components[i].vert_sample_factor=0;
        parser->image_components[i].horz_samples=0;
        parser->image_components[i].vert_samples=0;
        parser->image_components[i].quant_table_specifier=0;

        parser->image_components[i].scan_memory=NULL;
        parser->image_components[i].out_block_downsampled=NULL;
    }

    parser->P=0;
    parser->X=0;
    parser->Y=0;
    parser->Nf=0;
    parser->real_X=0;
    parser->real_Y=0;

    IDCTMaskSet_generate(&parser->idct_mask_set);

    parser->start_time=current_time();

    parser->component_label=0;
    parser->color_space=0;
}

[[gnu::flatten,gnu::nonnull(1)]]
static void JpegParser_process_channel(
    const JpegParser* const restrict parser,
    const uint8_t c,
    const uint32_t scan_id_start,
    const uint32_t scan_id_end
){
    QuantizationTable component_quant_table;
    memcpy(component_quant_table,parser->quant_tables[parser->image_components[c].quant_table_specifier],sizeof(component_quant_table));

    // -- reverse idct and quantization table application

    OUT_EL* const restrict out_block_downsampled=parser->image_components[c].out_block_downsampled;

    const uint32_t num_blocks_in_scan=parser->image_components[c].num_blocks_in_scan;

    OUT_EL idct_mask_set[64][64];
    memcpy(idct_mask_set,parser->idct_mask_set.idct_element_masks,sizeof(idct_mask_set));

    const OUT_EL idct_m0_v0=idct_mask_set[0][0];

    for (uint32_t scan_id=scan_id_start; scan_id<scan_id_end; scan_id++) {
        const MCU_EL* const restrict scan_mem=parser->image_components[c].scan_memory[scan_id];

        for (uint32_t block_id=0; block_id<num_blocks_in_scan; block_id++) {

            const MCU_EL* const restrict in_block=scan_mem+block_id*64;

            const uint32_t block_pixel_range_start=(scan_id*num_blocks_in_scan+block_id);

            OUT_EL out_block[64];

            // use first idct mask index to initialize storage
            {
                const OUT_EL cosine_mask_strength=(OUT_EL)(in_block[0]*component_quant_table[0]);

                const OUT_EL idct_m0_value=idct_m0_v0*cosine_mask_strength;
                
                for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                    out_block[pixel_index]=idct_m0_value;
                }
            }

            for(uint32_t cosine_index = 1;cosine_index<64;cosine_index++){
                const uint32_t corrected_cosine_index=cosine_index;
                const MCU_EL pre_quantized_mask_strength=in_block[corrected_cosine_index];
                if(pre_quantized_mask_strength == 0) {
                    continue;
                }

                const OUT_EL cosine_mask_strength=pre_quantized_mask_strength*component_quant_table[cosine_index];
                const OUT_EL* const restrict idct_mask=idct_mask_set[cosine_index];
                
                for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                    out_block[pixel_index]+=idct_mask[pixel_index]*cosine_mask_strength;
                }
            }

            memcpy(out_block_downsampled+block_pixel_range_start*64,out_block,sizeof(OUT_EL)*64);
        }
    }
}
struct JpegParser_process_channel_argset{
    JpegParser* parser;
    uint8_t c;
    uint32_t block_range_start;
    uint32_t block_range_end;
    ImageData* image_data;
};
void* JpegParser_process_channel_pthread(struct JpegParser_process_channel_argset* args){
    JpegParser_process_channel(args->parser,args->c,args->block_range_start,args->block_range_end);
    return NULL;
}

struct SomeData{
    JpegParser* parser;
    uint8_t channel;
    _Atomic uint32_t num_scans_parsed;
    ImageData* image_data;
};
void* ProcessIncomingScans_pthread(struct SomeData* somedata){
    uint32_t scan_id_start=0;
    uint32_t total_num_scans=somedata->parser->image_components[1].num_scans;
    while(scan_id_start<total_num_scans){
        uint32_t scan_id_end=atomic_load(&somedata->num_scans_parsed);

        uint32_t num_scans_to_process=scan_id_end-scan_id_start;
        if(num_scans_to_process){
            const uint8_t c=somedata->channel;

            JpegParser_process_channel(somedata->parser,c,scan_id_start,scan_id_end);

            scan_id_start=scan_id_end;
        }else{
            struct timespec sleeptime={.tv_sec=0,.tv_nsec=100000};
            nanosleep(&sleeptime, NULL);
        }
    }

    return NULL;
}

struct ScanComponent{
    uint8_t vert_sample_factor;
    uint8_t horz_sample_factor;

    uint32_t horz_samples;
    uint32_t vert_samples;

    uint32_t num_scans;
    uint32_t num_blocks_in_scan;
    
    uint8_t component_index_in_image;

    uint32_t num_blocks;

    uint32_t num_horz_blocks;

    HuffmanCodingTable* ac_table;
    HuffmanCodingTable* dc_table;

    MCU_EL** scan_memory;
};

[[gnu::hot,gnu::flatten,gnu::nonnull(3,4,6,7)]]
static inline void process_mcu_baseline(
    const struct ScanComponent* const restrict component,
    const uint32_t mcu_col,
    BitStream* const restrict stream,
    MCU_EL* const restrict diff_dc,
    const uint8_t successive_approximation_bit_low,
    MCU_EL* const restrict mcu_memory,
    uint64_t* const restrict eob_run
){
    const uint32_t vert_sample_factor=component->vert_sample_factor;
    const uint32_t horz_sample_factor=component->horz_sample_factor;

    const HuffmanCodingTable* const ac_table=component->ac_table;
    const HuffmanCodingTable* const dc_table=component->dc_table;

    for (uint32_t vert_sid=0; vert_sid<vert_sample_factor; vert_sid++) {
        for (uint32_t horz_sid=0; horz_sid<horz_sample_factor; horz_sid++) {
            const uint32_t block_col=mcu_col*horz_sample_factor + horz_sid;

            uint32_t component_block_id = block_col + vert_sid * component->num_horz_blocks;
            
            MCU_EL* const block_mem=&mcu_memory[component_block_id*64];

            decode_dc(block_mem, dc_table, diff_dc, stream, successive_approximation_bit_low);

            if(*eob_run>0){
                *eob_run-=1;
                continue;
            }
            
            decode_block_ac(block_mem, ac_table, 1, 63, stream, successive_approximation_bit_low, eob_run);
        
        }
    }
}

[[gnu::flatten,gnu::nonnull(3,4,6,11)]]
static inline void process_mcu_generic(
    const struct ScanComponent* const restrict component,
    uint32_t const mcu_col,
    BitStream* const restrict stream,
    MCU_EL* const restrict diff_dc,
    uint8_t const successive_approximation_bit_low,
    MCU_EL* const restrict mcu_memory,
    bool is_interleaved,
    uint8_t const spectral_selection_start,
    uint8_t const spectral_selection_end,
    uint8_t const successive_approximation_bit_high,
    uint64_t* const restrict eob_run,
    MCU_EL const succ_approx_bit_shifted
){
    const uint32_t vert_sample_factor=component->vert_sample_factor;
    const uint32_t horz_sample_factor=component->horz_sample_factor;

    const HuffmanCodingTable* const ac_table=component->ac_table;
    const HuffmanCodingTable* const dc_table=component->dc_table;

    for (uint32_t vert_sid=0; vert_sid<vert_sample_factor; vert_sid++) {
        for (uint32_t horz_sid=0; horz_sid<horz_sample_factor; horz_sid++) {
            const uint32_t block_col=mcu_col*horz_sample_factor + horz_sid;

            uint32_t component_block_id;
            if (is_interleaved) {
                component_block_id = block_col + vert_sid * component->num_horz_blocks;
            }else {
                component_block_id =
                    mcu_col
                        * horz_sample_factor*vert_sample_factor
                    + horz_sid
                    + vert_sid
                        * horz_sample_factor;
            }
            
            MCU_EL* const block_mem=&mcu_memory[component_block_id*64];

            if (successive_approximation_bit_high==0){
                uint8_t scan_start=spectral_selection_start;

                // decode dc
                if (spectral_selection_start==0) {
                    decode_dc(block_mem, dc_table, diff_dc, stream, successive_approximation_bit_low);

                    scan_start=1;
                }

                if(spectral_selection_end==0){
                    continue;
                }

                if(*eob_run>0){
                    *eob_run-=1;
                    continue;
                }
                
                // decode ac's
                const uint8_t sse=spectral_selection_end;
                register uint8_t spec_sel=scan_start;
                for (; spec_sel<=sse;) {
                    const uint8_t ac_bits=HuffmanCodingTable_lookup(ac_table, stream);

                    if (ac_bits==0) {
                        break;
                    }

                    const uint8_t num_zeros=ac_bits>>4;
                    const uint8_t ac_magnitude=ac_bits&0xF;

                    if (ac_magnitude==0) {
                        if (num_zeros==15) {
                            spec_sel+=16;
                            continue;
                        }else {
                            *eob_run=get_mask_u32(num_zeros);
                            *eob_run+=BitStream_get_bits_advance(stream, num_zeros);

                            break;
                        }
                    }

                    spec_sel+=num_zeros;
                    if (spec_sel>sse) {
                        break;
                    }

                    const MCU_EL ac_value_bits=(MCU_EL)BitStream_get_bits_advance(stream,ac_magnitude);

                    const MCU_EL ac_value=(MCU_EL)twos_complement((uint16_t)ac_magnitude,ac_value_bits);

                    block_mem[spec_sel++]=(MCU_EL)(ac_value<<successive_approximation_bit_low);
                }
            }else{
                if(spectral_selection_start == 0){
                    const uint64_t test_bit=BitStream_get_bits_advance(stream, 1);
                    if(test_bit){
                        block_mem[0] += succ_approx_bit_shifted;
                    }

                    continue;
                }

                if(*eob_run>0){
                    *eob_run-=1;

                    discard refine_block(
                        block_mem,
                        stream, 

                        spectral_selection_start, 
                        spectral_selection_end, 

                        64, 
                        succ_approx_bit_shifted
                    );

                    continue;
                }

                if(spectral_selection_end == 0)
                    continue;

                decode_block_with_sbh(
                    block_mem,
                    ac_table, 
                    spectral_selection_start, 
                    spectral_selection_end, 
                    stream, 
                    succ_approx_bit_shifted,
                    eob_run
                );
            }
        }
    }
}

[[gnu::nonnull(1,2)]]
void JpegParser_parse_file(
    JpegParser* const restrict parser,
    ImageData* const restrict image_data,
    const bool parallel
){
    #define GET_NB ((uint32_t)((parser->file_contents)[parser->current_byte_position++]))

    #define GET_U8(VARIABLE) VARIABLE=GET_NB;
    #define GET_U16(VARIABLE) VARIABLE=((GET_NB<<8)| GET_NB);

    #define HB_U8(VARIABLE) ((VARIABLE&0xF0)>>4)
    #define LB_U8(VARIABLE) (VARIABLE&0xF)

    struct SomeData async_scan_info[3]={
        {
            .parser=parser,
            .channel=0,
            .num_scans_parsed=0,
            .image_data=image_data
        },
        {
            .parser=parser,
            .channel=1,
            .num_scans_parsed=0,
            .image_data=image_data
        },
        {
            .parser=parser,
            .channel=2,
            .num_scans_parsed=0,
            .image_data=image_data
        }
    };
    pthread_t async_scan_processors[3];

    struct ScanComponent scan_components[3];

    // +1 for each component to allow component with index 0 to be actively counted
    static const uint32_t CHANNEL_COMPLETE=2016+64; // sum(0..<64) + 64
    uint32_t channel_completeness[3]={0,0,0};

    // these fields are only used within a sos segment, but the storage can be re-used for later sos segments. since each sos segment reads some part of the file, subsequent sos segments cannot require more storage than the first one, i.e. storage requirement does not increase over time either
    uint32_t stuffed_byte_index_capacity=1024;
    uint32_t* stuffed_byte_indices=malloc(sizeof(uint32_t)*stuffed_byte_index_capacity);
    uint8_t* const de_zeroed_file_contents=malloc(parser->file_size);

    enum EncodingMethod{
        Baseline,
        Progressive,

        UNDEFINED
    };

    enum EncodingMethod encoding_method=UNDEFINED;

    bool parsing_done=false;
    while (!parsing_done) {
        uint32_t next_header;
        GET_U16(next_header);

        uint32_t segment_size;
        
        switch (next_header) {
            case JPEG_SEGMENT_SOI:
                break;
            case JPEG_SEGMENT_EOI:
                parsing_done=true;
                break;

            case COM:
                {
                    GET_U16(segment_size);
                    const uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    const uint32_t comment_length=segment_size-2;

                    char* const comment_str=malloc(comment_length+1);
                    comment_str[comment_length]=0;

                    memcpy(comment_str,&parser->file_contents[parser->current_byte_position],comment_length);

                    println("found jpeg file comment: \"%s\"",comment_str);

                    free(comment_str);

                    parser->current_byte_position=segment_end_position;
                }
                break;

            case APP0:
            case APP1:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=parser->current_byte_position+segment_size-2;
                    parser->current_byte_position=segment_end_position;
                }
                break;

            case DQT:
                {
                    GET_U16(segment_size);
                    const uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        const uint8_t destination_and_precision=(uint8_t)GET_NB;
                        const uint8_t destination=LB_U8(destination_and_precision);
                        const uint8_t precision=HB_U8(destination_and_precision);
                        if (precision!=0) {
                            fprintf(stderr, "jpeg quant table precision is not 0 - it is %d\n",precision);
                            exit(-45);
                        }

                        uint8_t table_entries[64];
                        memcpy(table_entries,&parser->file_contents[parser->current_byte_position],64);
                        parser->current_byte_position+=64;

                        segment_bytes_read+=65;

                        for (int i=0; i<64; i++) {
                            parser->quant_tables[destination][i]=table_entries[i];
                        }

                    }

                    parser->current_byte_position=segment_end_position;
                }
                break;

            case DHT:
                {
                    GET_U16(segment_size);
                    const uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        uint8_t table_index_and_class=(uint8_t)GET_NB;

                        const uint8_t table_index=LB_U8(table_index_and_class);
                        const uint8_t table_class=HB_U8(table_index_and_class);

                        HuffmanCodingTable* target_table=NULL;
                        switch (table_class) {
                            case  0:
                                target_table=&parser->dc_coding_tables[table_index];
                                break;
                            case  1:
                                target_table=&parser->ac_coding_tables[table_index];
                                break;
                            default:
                                exit(FATAL_UNEXPECTED_ERROR);
                        }

                        uint32_t total_num_values=0;
                        uint8_t num_values_of_length[16];

                        memcpy(num_values_of_length,&parser->file_contents[parser->current_byte_position],16);
                        parser->current_byte_position+=16;

                        for (int i=0; i<16; i++) {
                            total_num_values+=num_values_of_length[i];
                        }

                        segment_bytes_read+=17;

                        uint8_t values[260];
                        memcpy(values,&parser->file_contents[parser->current_byte_position],total_num_values);
                        parser->current_byte_position+=total_num_values;

                        uint8_t value_code_lengths[260];

                        uint32_t value_index=0;
                        for (uint8_t code_length=0; code_length<16; code_length++) {
                            memset(&value_code_lengths[value_index],code_length+1,num_values_of_length[code_length]);
                            value_index+=num_values_of_length[code_length];
                        }

                        segment_bytes_read+=value_index;

                        // destroy previous table, if there was one
                        HuffmanCodingTable_destroy(target_table);

                        HuffmanCodingTable_new(
                            target_table,
                            num_values_of_length,
                            total_num_values,
                            value_code_lengths,
                            values
                        );
                    }

                    parser->current_byte_position=segment_end_position;
                }
                break;

            case SOF0: // baseline
                if(encoding_method==UNDEFINED)
                    encoding_method=Baseline;
            case SOF2: // progressive
                if(encoding_method==UNDEFINED)
                    encoding_method=Progressive;

                {
                    GET_U16(segment_size);
                    const uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    GET_U8(parser->P);
                    GET_U16(parser->real_Y);
                    GET_U16(parser->real_X);
                    GET_U8(parser->Nf);

                    if (parser->P!=8) {
                        fprintf(stderr,"image precision is not 8 - is %d instead\n",parser->P);
                        exit(-46);
                    }

                    // parse basic per-component metadata
                    for (uint32_t i=0; i<parser->Nf; i++) {
                        parser->image_components[i].component_id=(uint8_t)GET_NB;

                        const uint32_t sample_factors=GET_NB;

                        parser->image_components[i].vert_sample_factor=HB_U8(sample_factors);
                        parser->image_components[i].horz_sample_factor=LB_U8(sample_factors);

                        parser->image_components[i].quant_table_specifier=(uint8_t)GET_NB;

                        if (parser->image_components[i].vert_sample_factor>parser->max_component_vert_sample_factor) {
                            parser->max_component_vert_sample_factor=parser->image_components[i].vert_sample_factor;
                        }
                        if (parser->image_components[i].horz_sample_factor>parser->max_component_horz_sample_factor) {
                            parser->max_component_horz_sample_factor=parser->image_components[i].horz_sample_factor;
                        }
                    }

                    parser->X=ROUND_UP(parser->real_X,8);
                    parser->Y=ROUND_UP(parser->real_Y,8);

                    image_data->height=parser->Y;
                    image_data->width=parser->X;

                    // calculate per-component metadata and allocate scan memory
                    for (uint32_t i=0; i<parser->Nf; i++) {
                        parser->image_components[i].vert_samples=(ROUND_UP(parser->Y,8*parser->max_component_vert_sample_factor))*parser->image_components[i].vert_sample_factor/parser->max_component_vert_sample_factor;
                        parser->image_components[i].horz_samples=(ROUND_UP(parser->X,8*parser->max_component_horz_sample_factor))*parser->image_components[i].horz_sample_factor/parser->max_component_horz_sample_factor;

                        parser->X=max(parser->X,parser->image_components[i].horz_samples);
                        parser->Y=max(parser->Y,parser->image_components[i].vert_samples);

                        const uint32_t component_data_size=parser->image_components[i].vert_samples*parser->image_components[i].horz_samples;

                        const uint32_t component_num_scans=parser->image_components[i].vert_samples/parser->image_components[i].vert_sample_factor/8;
                        const uint32_t component_num_scan_elements=parser->image_components[i].horz_samples*parser->image_components[i].vert_sample_factor*8;

                        parser->image_components[i].scan_memory=malloc(sizeof(MCU_EL*)*component_num_scans);

                        uint32_t per_scan_memory_size=ROUND_UP(component_num_scan_elements*sizeof(MCU_EL),4096);
                        MCU_EL* const total_scan_memory=calloc(component_num_scans,per_scan_memory_size);
                        for (uint32_t s=0; s<component_num_scans; s++) {
                            parser->image_components[i].scan_memory[s]=total_scan_memory+s*per_scan_memory_size/sizeof(MCU_EL);
                            //parser->image_components[i].scan_memory[s]=calloc(1,component_num_scan_elements*sizeof(MCU_EL));
                        }

                        parser->image_components[i].num_scans=component_num_scans;
                        parser->image_components[i].num_blocks_in_scan=component_num_scan_elements/64;

                        parser->image_components[i].out_block_downsampled=aligned_alloc(64,ROUND_UP(sizeof(OUT_EL)*(component_data_size+16),64));

                        parser->image_components[i].total_num_blocks=parser->image_components[i].vert_samples*parser->image_components[i].horz_samples/64;

                        parser->component_label|=((uint32_t)parser->image_components[i].horz_sample_factor)<<(((parser->Nf-i)*2-1)*4);
                        parser->component_label|=((uint32_t)parser->image_components[i].vert_sample_factor)<<(((parser->Nf-i)*2-2)*4);

                        parser->color_space|=(uint32_t)(parser->image_components[i].component_id<<(4*(parser->Nf-1-i)));
                    }
                   
                    const uint32_t num_pixels_per_scan=parser->max_component_vert_sample_factor*8*parser->X;

                    // pre-calculate indices to re-order pixel data from block-orientation to row-column orientation
                    for (uint32_t i=0; i<parser->Nf; i++) {
                        parser->image_components[i].conversion_indices=malloc(sizeof(uint32_t)*num_pixels_per_scan);
                        uint32_t* const conversion_indices=parser->image_components[i].conversion_indices;

                        uint32_t ci=0;
                        for(uint32_t img_y=0;img_y<parser->max_component_vert_sample_factor*8;img_y++){
                            for (uint32_t img_x=0;img_x<parser->X;img_x++) {
                                const uint32_t adjusted_img_y=img_y*parser->image_components[i].vert_sample_factor/parser->max_component_vert_sample_factor;
                                const uint32_t adjusted_img_x=img_x*parser->image_components[i].horz_sample_factor/parser->max_component_horz_sample_factor;

                                const uint32_t index=
                                     (adjusted_img_y/8)*parser->image_components[i].horz_samples*8
                                    +(adjusted_img_x/8)*64
                                    +(adjusted_img_y%8)*8
                                    +adjusted_img_x%8;

                                conversion_indices[ci++]=index;
                            }
                        }

                        if(ci!=num_pixels_per_scan){
                            println("this is a bug. %d != %d",ci,num_pixels_per_scan);
                            exit(FATAL_UNEXPECTED_ERROR);
                        }
                    }

                    const uint32_t total_num_pixels_in_image=parser->X*parser->Y;

                    image_data->data=malloc(ROUND_UP(sizeof(uint8_t)*total_num_pixels_in_image*4,64));

                    parser->current_byte_position=segment_end_position;
                }
                println("ending sof at %f",current_time()-parser->start_time);
                break;

            case SOS:
                println("starting sos at %f",current_time()-parser->start_time);
                {
                    GET_U16(segment_size);

                    uint8_t num_scan_components=(uint8_t)GET_NB;

                    bool is_interleaved=num_scan_components != 1;

                    uint8_t scan_component_id[3];
                    uint8_t scan_component_ac_table_index[3];
                    uint8_t scan_component_dc_table_index[3];

                    for (uint32_t i=0; i<num_scan_components; i++) {
                        scan_component_id[i]=(uint8_t)GET_NB;

                        uint8_t table_indices=(uint8_t)GET_NB;

                        scan_component_dc_table_index[i]=HB_U8(table_indices);
                        scan_component_ac_table_index[i]=LB_U8(table_indices);
                    }

                    const uint8_t spectral_selection_start=(uint8_t)GET_NB;
                    const uint8_t spectral_selection_end=(uint8_t)GET_NB;

                    const uint8_t successive_approximation_bits=(uint8_t)GET_NB;
                    const uint8_t successive_approximation_bit_low=LB_U8(successive_approximation_bits);
                    const uint8_t successive_approximation_bit_high=HB_U8(successive_approximation_bits);

                    MCU_EL differential_dc[3]={0,0,0};

                    uint32_t stuffed_byte_index_count=0;

                    uint32_t out_index=0;
                    for (uint32_t i=0; i<(parser->file_size-parser->current_byte_position-1); i++) {
                        const uint8_t current_byte=parser->file_contents[parser->current_byte_position+i];
                        const uint8_t next_byte=parser->file_contents[parser->current_byte_position+i+1];

                        de_zeroed_file_contents[out_index]=current_byte;

                        if ((current_byte==0xFF) && (next_byte==0)) {
                            stuffed_byte_indices[stuffed_byte_index_count++]=out_index;

                            if (stuffed_byte_index_count==stuffed_byte_index_capacity) {
                                stuffed_byte_index_capacity*=2;
                                stuffed_byte_indices=realloc(stuffed_byte_indices, 4*stuffed_byte_index_capacity);
                            }

                            i++;
                        }
                        out_index++;
                    }

                    BitStream _bit_stream;
                    BitStream* const restrict stream=&_bit_stream;
                    BitStream_new(stream, de_zeroed_file_contents);

                    const uint32_t mcu_cols=parser->image_components[0].horz_samples/parser->image_components[0].horz_sample_factor/8;
                    const uint32_t mcu_rows=parser->image_components[0].vert_samples/parser->image_components[0].vert_sample_factor/8;

                    uint8_t scan_component_vert_sample_factor[3];
                    uint8_t scan_component_horz_sample_factor[3];
                    uint8_t scan_component_index_in_image[3]={UINT8_MAX,UINT8_MAX,UINT8_MAX};

                    for (uint8_t scan_component_index=0; scan_component_index<num_scan_components; scan_component_index++) {
                        for (uint8_t i=0; i<parser->Nf; i++) {
                            if (parser->image_components[i].component_id==scan_component_id[scan_component_index]) {
                                scan_component_vert_sample_factor[scan_component_index]=parser->image_components[i].vert_sample_factor;
                                scan_component_horz_sample_factor[scan_component_index]=parser->image_components[i].horz_sample_factor;

                                scan_component_index_in_image[scan_component_index]=i;
                                break;
                            }
                        }

                        if (scan_component_index_in_image[scan_component_index]==UINT8_MAX) {
                            fprintf(stderr,"did not find image component?!\n");
                            exit(-102);
                        }
                    }

                    for (int c=0; c<num_scan_components; c++) {
                        scan_components[c].vert_sample_factor=scan_component_vert_sample_factor[c];
                        scan_components[c].horz_sample_factor=scan_component_horz_sample_factor[c];

                        uint8_t component_index_in_image=scan_component_index_in_image[c];
                        scan_components[c].component_index_in_image=component_index_in_image;

                        scan_components[c].num_blocks=parser->image_components[component_index_in_image].horz_samples/8*parser->image_components[component_index_in_image].vert_samples/8;

                        scan_components[c].dc_table=&parser->dc_coding_tables[scan_component_dc_table_index[c]];
                        scan_components[c].ac_table=&parser->ac_coding_tables[scan_component_ac_table_index[c]];

                        scan_components[c].scan_memory=parser->image_components[component_index_in_image].scan_memory;

                        scan_components[c].num_scans=parser->image_components[component_index_in_image].num_scans;
                        scan_components[c].num_blocks_in_scan=parser->image_components[component_index_in_image].num_blocks_in_scan;

                        scan_components[c].horz_samples=parser->image_components[component_index_in_image].horz_samples;
                        scan_components[c].vert_samples=parser->image_components[component_index_in_image].vert_samples;

                        scan_components[c].num_horz_blocks=scan_components[c].horz_samples/8;
                    }

                    //println("scan info: %d succ low %d high %d start %d end %d",num_scan_components,successive_approximation_bit_low,successive_approximation_bit_high,spectral_selection_start,spectral_selection_end);

                    if(parallel && (successive_approximation_bit_low==0)){
                        for (uint32_t c=0; c<num_scan_components; c++) {
                            const uint32_t t=scan_components[c].component_index_in_image;
                            for (uint32_t i=spectral_selection_start; i<=spectral_selection_end; i++) {
                                channel_completeness[t]+=i+1;
                            }
                            if (channel_completeness[t]==CHANNEL_COMPLETE){
                                pthread_create(&async_scan_processors[t], NULL, (pthread_callback)ProcessIncomingScans_pthread, &async_scan_info[t]);
                            }
                        }
                    }

                    // needed when successive_approximation_bit_high>0
                    const MCU_EL succ_approx_bit_shifted=(MCU_EL)(1<<successive_approximation_bit_low);

                    uint64_t eob_run=0;

                    for (uint32_t mcu_row=0;mcu_row<mcu_rows;mcu_row++) {
                        MCU_EL* const restrict scan_memories[3]={
                            scan_components[0].scan_memory[mcu_row],
                            scan_components[1].scan_memory[mcu_row],
                            scan_components[2].scan_memory[mcu_row],
                        };

                        if(encoding_method==Baseline){
                            if(successive_approximation_bit_high!=0){
                                fprintf(stderr,"bug in %s : %d.\n",__FILE__,__LINE__);
                                exit(FATAL_UNEXPECTED_ERROR);
                            }

                            for (uint32_t mcu_col=0;mcu_col<mcu_cols;mcu_col++) {
                                for (uint32_t c=0; c<num_scan_components; c++) {
                                    process_mcu_baseline(
                                        &scan_components[c],
                                        mcu_col,
                                        stream,
                                        &differential_dc[c],
                                        successive_approximation_bit_low,
                                        scan_memories[c],
                                        &eob_run
                                    );
                                }
                            }
                        }else{
                            for (uint32_t mcu_col=0;mcu_col<mcu_cols;mcu_col++) {
                                for (uint32_t c=0; c<num_scan_components; c++) {
                                    process_mcu_generic(
                                        &scan_components[c],
                                        mcu_col,
                                        stream,
                                        &differential_dc[c],
                                        successive_approximation_bit_low,
                                        scan_memories[c],
                                        is_interleaved,
                                        spectral_selection_start,
                                        spectral_selection_end,
                                        successive_approximation_bit_high,
                                        &eob_run,
                                        succ_approx_bit_shifted
                                    );
                                }
                            }
                        }

                        if(parallel&&(successive_approximation_bit_low==0))
                            for(int t=0;t<num_scan_components;t++){
                                const uint8_t index=scan_components[t].component_index_in_image;
                                if (channel_completeness[index]==CHANNEL_COMPLETE)
                                    atomic_fetch_add(&async_scan_info[index].num_scans_parsed, 1);
                            }
                    }

                    const uint32_t bytes_read_from_stream=(uint32_t)(stream->next_data_index-stream->buffer_bits_filled/8);

                    uint32_t stuffed_byte_count_skipped=0;
                    for (uint32_t i=0; i<stuffed_byte_index_count; i++) {
                        if (bytes_read_from_stream>=stuffed_byte_indices[i]) {
                            stuffed_byte_count_skipped+=1;
                        }else {
                            break;
                        }
                    }
                    parser->current_byte_position+=bytes_read_from_stream+stuffed_byte_count_skipped;
                }

                break;

            default:
                fprintf(stderr,"unhandled segment %s ( %X ) \n",Image_jpeg_segment_type_name(next_header),next_header);
                exit(-40);
        }
    }

    free(stuffed_byte_indices);
    free(de_zeroed_file_contents);

    println("parsing done at %f",current_time()-parser->start_time);

    if(parallel)
        for(uint8_t t=0;t<3;t++)
            pthread_join(async_scan_processors[t], NULL);

    if (!parallel) {
        for(uint8_t c=0;c<3;c++){
            JpegParser_process_channel(parser,c,0,parser->image_components[c].num_scans);
        }
    }

    println("processing done at %f",current_time()-parser->start_time);
}

#ifdef FIXED_PRECISION_ARITHMETIC
[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb(
    const JpegParser* const restrict parser,
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

    for (uint32_t i=0; i<pixels_in_scan; i++) {
        // -- re-order from block-orientation to final image orientation

        #ifdef FIXED_PRECISION_ARITHMETIC
            const OUT_EL Y=y[image_components[0].conversion_indices[i]]>>PRECISION;
            const OUT_EL Cr=(cr[image_components[1].conversion_indices[i]]-128)>>PRECISION;
            const OUT_EL Cb=(cb[image_components[2].conversion_indices[i]]-128)>>PRECISION;

            // -- convert ycbcr to rgb

            const OUT_EL R = Y + ((            45 * Cr ) >> 5 );
            const OUT_EL B = Y + (( 113 * Cb           ) >> 6 );
            const OUT_EL G = Y - ((  11 * Cb + 23 * Cr ) >> 5 );

            // -- deinterlace and convert to uint8

            image_data_data[i* 4 + 0] = (uint8_t)clamp(0,255,R+128);
            image_data_data[i* 4 + 1] = (uint8_t)clamp(0,255,G+128);
            image_data_data[i* 4 + 2] = (uint8_t)clamp(0,255,B+128);
            image_data_data[i* 4 + 3] = UINT8_MAX;
        #else
            #ifdef VK_USE_PLATFORM_METAL_EXT
                const float32x4_t y_simd=vld1q_f32(&y[image_components[0].conversion_indices[i]]);
                const float32x4_t cr_simd=vld1q_f32(&cr[image_components[1].conversion_indices[i]]);
                const float32x4_t cb_simd=vld1q_f32(&cb[image_components[2].conversion_indices[i]]);

                // -- convert ycbcr to rgb

                register float32x4_t r_simd=y_simd+1.402f*cr_simd;
                register float32x4_t b_simd=y_simd+1.772f*cb_simd;
                register float32x4_t g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

                register float32x4_t v_simd;

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

                i+=3;

            #elif defined(VK_USE_PLATFORM_XCB_KHR)
                __m128 y_simd=_mm_loadu_ps(&y[image_components[0].conversion_indices[i]]);
                __m128 cr_simd=_mm_loadu_ps(&cr[image_components[1].conversion_indices[i]]);
                cr_simd=_mm_shuffle_epi32(cr_simd,16+64);
                __m128 cb_simd=_mm_loadu_ps(&cb[image_components[2].conversion_indices[i]]);
                cb_simd=_mm_shuffle_epi32(cb_simd,16+64);

                // -- convert ycbcr to rgb

                register __m128 r_simd=y_simd+1.402f*cr_simd;
                register __m128 b_simd=y_simd+1.772f*cb_simd;
                register __m128 g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

                register __m128 v_simd;

                v_simd=_mm_set1_ps(128.0);
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

                i+=3;

            #else
                const OUT_EL Y=y[image_components[0].conversion_indices[i]];
                const OUT_EL Cr=cr[image_components[1].conversion_indices[i]];
                const OUT_EL Cb=cb[image_components[2].conversion_indices[i]];

                // -- convert ycbcr to rgb

                const OUT_EL R = Y +                1.402f * Cr;
                const OUT_EL B = Y +  1.772f * Cb;
                const OUT_EL G = Y - (0.343f * Cb + 0.718f * Cr );

                // -- deinterlace and convert to uint8

                image_data_data[i* 4 + 0] = (uint8_t)clamp_f32(0.0f,255.0f,R+128.0f);
                image_data_data[i* 4 + 1] = (uint8_t)clamp_f32(0.0f,255.0f,G+128.0f);
                image_data_data[i* 4 + 2] = (uint8_t)clamp_f32(0.0f,255.0f,B+128.0f);
                image_data_data[i* 4 + 3] = UINT8_MAX;
            #endif
        #endif

    }
}

#else // not(defined(FIXED_PRECISION_ARITHMETIC))

#ifdef VK_USE_PLATFORM_METAL_EXT

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb_neon(
    const JpegParser* const restrict parser,
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

        // TODO : bug . the 4 values starting at image_components[0 or 1 or 2].conversion_indices[i] may not be strictly increasing by 1
        const float32x4_t y_simd=vld1q_f32(&y[image_components[0].conversion_indices[i]]);
        const float32x4_t cr_simd=vld1q_f32(&cr[image_components[1].conversion_indices[i]]);
        const float32x4_t cb_simd=vld1q_f32(&cb[image_components[2].conversion_indices[i]]);

        // -- convert ycbcr to rgb

        register float32x4_t r_simd=y_simd+1.402f*cr_simd;
        register float32x4_t b_simd=y_simd+1.772f*cb_simd;
        register float32x4_t g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

        register float32x4_t v_simd;

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

#elifdef VK_USE_PLATFORM_XCB_KHR

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
void scan_ycbcr_to_rgb_sse(
    const JpegParser* const restrict parser,
    const uint32_t mcu_row
){
    const ImageComponent image_components[3]={
        parser->image_components[0],
        parser->image_components[1],
        parser->image_components[2]
    };

    uint32_t pixels_in_scan=image_components[0].horz_samples*8*image_components[0].vert_sample_factor;
    uint32_t scan_offset=mcu_row*pixels_in_scan;

    uint8_t* const restrict image_data_data=parser->image_data->data+scan_offset*4;

    const uint32_t rescale_factor[3]={
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const restrict y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const restrict cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const restrict cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i+=4) {
        // -- re-order from block-orientation to final image orientation

        __m128 y_simd=_mm_loadu_ps(&y[image_components[0].conversion_indices[i]]);
        __m128 cr_simd=_mm_loadu_ps(&cr[image_components[1].conversion_indices[i]]);
        cr_simd=_mm_shuffle_epi32(cr_simd,(1<<4)+(1<<6));
        __m128 cb_simd=_mm_loadu_ps(&cb[image_components[2].conversion_indices[i]]);
        cb_simd=_mm_shuffle_epi32(cb_simd,(1<<4)+(1<<6));

        // -- convert ycbcr to rgb

        register __m128 r_simd=y_simd+1.402f*cr_simd;
        register __m128 b_simd=y_simd+1.772f*cb_simd;
        register __m128 g_simd=y_simd-(0.343f * cb_simd + 0.718f * cr_simd );

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

#endif

[[gnu::hot,gnu::flatten,gnu::nonnull(1)]]
static inline void scan_ycbcr_to_rgb(
    const JpegParser* const restrict parser,
    const uint32_t mcu_row
){
    const ImageComponent image_components[3]={
        parser->image_components[0],
        parser->image_components[1],
        parser->image_components[2]
    };

    uint32_t pixels_in_scan=image_components[0].horz_samples*8*image_components[0].vert_sample_factor;
    uint32_t scan_offset=mcu_row*pixels_in_scan;

    uint8_t* const restrict image_data_data=parser->image_data->data+scan_offset*4;

    const uint32_t rescale_factor[3]={
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[0].horz_sample_factor*image_components[0].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[1].horz_sample_factor*image_components[1].vert_sample_factor),
        parser->max_component_horz_sample_factor*parser->max_component_vert_sample_factor/(image_components[2].horz_sample_factor*image_components[2].vert_sample_factor)
    };

    const OUT_EL* const restrict y[[gnu::aligned(16)]]=image_components[0].out_block_downsampled+scan_offset/rescale_factor[0];
    const OUT_EL* const restrict cr[[gnu::aligned(16)]]=image_components[1].out_block_downsampled+scan_offset/rescale_factor[1];
    const OUT_EL* const restrict cb[[gnu::aligned(16)]]=image_components[2].out_block_downsampled+scan_offset/rescale_factor[2];

    for (uint32_t i=0; i<pixels_in_scan; i++) {
        // -- re-order from block-orientation to final image orientation

        const OUT_EL Y=y[image_components[0].conversion_indices[i]];
        const OUT_EL Cr=cr[image_components[1].conversion_indices[i]];
        const OUT_EL Cb=cb[image_components[2].conversion_indices[i]];

        // -- convert ycbcr to rgb

        const OUT_EL R = Y +                1.402f * Cr;
        const OUT_EL B = Y +  1.772f * Cb;
        const OUT_EL G = Y - (0.343f * Cb + 0.718f * Cr );

        // -- deinterlace and convert to uint8

        image_data_data[i* 4 + 0] = (uint8_t)clamp_f32(0.0f,255.0f,R+128.0f);
        image_data_data[i* 4 + 1] = (uint8_t)clamp_f32(0.0f,255.0f,G+128.0f);
        image_data_data[i* 4 + 2] = (uint8_t)clamp_f32(0.0f,255.0f,B+128.0f);
        image_data_data[i* 4 + 3] = UINT8_MAX;
    }
}
#endif // defined(FIXED_PRECISION_ARITHMETIC)

[[gnu::flatten,gnu::hot,gnu::nonnull(1)]]
void JpegParser_convert_colorspace(
    JpegParser* const restrict parser,
    const uint32_t scan_index_start,
    const uint32_t scan_index_end
){
    #ifndef FIXED_PRECISION_ARITHMETIC
    if (parser->component_label==0x221111){
        #ifdef VK_USE_PLATFORM_METAL_EXT
            for (uint32_t s=scan_index_start; s<scan_index_end; s++)
                scan_ycbcr_to_rgb_neon(parser,s);
            return;
        #elifdef VK_USE_PLATFORM_XCB_KHR
            for (uint32_t s=scan_index_start; s<scan_index_end; s++)
                scan_ycbcr_to_rgb_sse(parser,s);
            return;
        #endif
    }
    #endif

    for (uint32_t s=scan_index_start; s<scan_index_end; s++) {
        scan_ycbcr_to_rgb(
            parser,
            s
        );
    }
}

struct JpegParser_convert_colorspace_argset{
    JpegParser* parser;

    uint32_t scan_index_start;
    uint32_t scan_index_end;
};
void* JpegParser_convert_colorspace_pthread(struct JpegParser_convert_colorspace_argset* args){
    JpegParser_convert_colorspace(args->parser,args->scan_index_start,args->scan_index_end);
    return NULL;
}

ImageParseResult Image_read_jpeg(
    const char* const filepath,
    ImageData* const restrict image_data
){
    println("starting decode");
    FILE* const file=fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "file '%s' not found\n",filepath);
        return IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
    }

    JpegParser parser;
    JpegParser_init_empty(&parser);
    parser.image_data=image_data;

    discard fseek(file,0,SEEK_END);
    long ftell_res=ftell(file);
    if(ftell_res<0){
        fprintf(stderr,"could not get file size\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }
    parser.file_size=(uint64_t)ftell_res;
    rewind(file);

    parser.file_contents=aligned_alloc(64,ROUND_UP(parser.file_size,64));
    discard fread(parser.file_contents, 1, parser.file_size, file);
    
    fclose(file);

    const uint32_t num_threads=1;

    JpegParser_parse_file(&parser, image_data, num_threads>1);

    // -- convert idct magnitude values to channel pixel values
    // then upsample channels to final resolution
    // and convert ycbcr to rgb

    image_data->interleaved=true;
    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;

    switch(parser.color_space){
        case 0x123:
            {
                if(num_threads>1){
                    struct JpegParser_convert_colorspace_argset* const thread_args=malloc(num_threads*sizeof(struct JpegParser_convert_colorspace_argset));
                    pthread_t* const threads=malloc(num_threads*sizeof(pthread_t));

                    const uint32_t num_scans_per_thread=parser.image_components[0].num_scans/num_threads;
                    for(uint32_t i=0;i<num_threads;i++){
                        thread_args[i].parser=&parser;
                        thread_args[i].scan_index_start=i*num_scans_per_thread;
                        thread_args[i].scan_index_end=(i+1)*num_scans_per_thread;

                        if(i==(num_threads-1)){
                            thread_args[i].scan_index_end=parser.image_components[0].num_scans;
                        }
                    }

                    for(uint32_t i=0;i<num_threads;i++){
                        if(pthread_create(&threads[i], NULL, (pthread_callback)JpegParser_convert_colorspace_pthread, &thread_args[i])!=0){
                            fprintf(stderr,"failed to launch pthread\n");
                            exit(-107);
                        }
                    }

                    for(uint32_t i=0;i<num_threads;i++){
                        if(pthread_join(threads[i],NULL)!=0){
                            fprintf(stderr,"failed to join pthread\n");
                            exit(-108);
                        }
                    }

                    free(thread_args);
                    free(threads);
                }else{
                    JpegParser_convert_colorspace(&parser,0,parser.image_components[0].num_scans);
                }
            }
            break;

        default:
            fprintf(stderr,"color space %3X other than YCbCr (component IDs 1,2,3) currently unimplemented",parser.color_space);
            exit(-65);
    }

    println("colorspace done at %f",current_time()-parser.start_time);

    // -- crop to real size

    if(parser.X!=parser.real_X || parser.Y!=parser.real_Y){
        uint8_t* const old_data=image_data->data;
        uint8_t* const real_data=aligned_alloc(64,ROUND_UP(parser.real_X*parser.real_Y*4,64));

        for (uint32_t y=0; y<parser.real_Y; y++) {
            for(uint32_t x=0; x<parser.real_X; x++) {
                real_data[(y*parser.real_X+x)*4+0]=old_data[(y*parser.X+x)*4+0];
                real_data[(y*parser.real_X+x)*4+1]=old_data[(y*parser.X+x)*4+1];
                real_data[(y*parser.real_X+x)*4+2]=old_data[(y*parser.X+x)*4+2];
                real_data[(y*parser.real_X+x)*4+3]=old_data[(y*parser.X+x)*4+3];
            }
        }

        free(old_data);

        image_data->height=parser.real_Y;
        image_data->width=parser.real_X;
        image_data->data=real_data;

        println("cropped to final size at %f",current_time()-parser.start_time);
    }

    // -- parsing done. free all resources

    free(parser.file_contents);

    for(int i=0;i<4;i++){
        HuffmanCodingTable_destroy(&parser.ac_coding_tables[i]);
        HuffmanCodingTable_destroy(&parser.dc_coding_tables[i]);
    }

    for(int c=0;c<3;c++){
        free(parser.image_components[c].conversion_indices);
        free(parser.image_components[c].out_block_downsampled);
        //for(uint32_t s=0;s<parser.image_components[c].num_scans;s++){
        for(uint32_t s=0;s<1;s++){
            free(parser.image_components[c].scan_memory[s]);
        }
        free(parser.image_components[c].scan_memory);
    }

    return IMAGE_PARSE_RESULT_OK;
}

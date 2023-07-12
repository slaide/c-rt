#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#include <time.h>

#include "app/app.h"
#include "app/error.h"
#include "app/huffman.h"

typedef void*(*pthread_callback)(void*);

#define MAX_NSEC 999999999

double current_time(){
    struct timespec current_time;
    int time_get_result=clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (time_get_result != 0) {
        fprintf(stderr, "failed to get start time because %d\n",time_get_result);
        exit(-66);
    }

    double ret=current_time.tv_sec;
    ret+=((double)current_time.tv_nsec)/(double)(MAX_NSEC+1);
    return ret;
}

float clamp_f32(float v_min,float v_max, float v){
    return fmaxf(v_min, fminf(v_max, v));
}

int32_t twos_complement(uint32_t magnitude, int32_t value){
    int32_t threshold=1<<(magnitude-1);
    if (value<threshold){
        int32_t ret=value+1;
        ret+=(-1)<<magnitude;
        return ret;
    }

    return value;
}

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

typedef int QuantizationTable[64];

const int ZIGZAG[64]={
    0,  1,  5,  6,  14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63,
};
const int UNZIGZAG[64]={
    0,  1,  8,  16, 9,  2,  3,  10, 
    17, 24, 32, 25, 18, 11, 4,  5, 
    12, 19, 26, 33, 40, 48, 41, 34, 
    27, 20, 13, 6,  7,  14, 21, 28, 
    35, 42, 49, 56, 57, 50, 43, 36, 
    29, 22, 15, 23, 30, 37, 44, 51, 
    58, 59, 52, 45, 38, 31, 39, 46, 
    53, 60, 61, 54, 47, 55, 62, 63
};

#define BLOCK_ELEMENT_TYPE int32_t

typedef struct ImageComponent{
    uint32_t vert_samples;
    uint32_t horz_samples;

    uint32_t component_id;
    uint32_t vert_sample_factor;
    uint32_t horz_sample_factor;
    uint32_t quant_table_specifier;

    uint32_t num_scans;
    uint32_t num_blocks_in_scan;

    uint32_t total_num_blocks;

    /**
     * @brief decompressed scans, where each scan has its own memory
     * 
     */
    BLOCK_ELEMENT_TYPE** scan_memory;

    /**
     * @brief 
     * 
     */
    float* out_block_downsampled;
}ImageComponent;

/**
 * @brief decode block with successive approximation high at zero
 * 
 * @param block_mem 
 * @param dc_table 
 * @param diff_dc 
 * @param ac_table 
 * @param spectral_selection_start 
 * @param spectral_selection_end 
 * @param bit_stream 
 * @param successive_approximation_bit_low 
 * @param eob_run 
 */
static inline void decode_block(
    BLOCK_ELEMENT_TYPE block_mem[64],

    HuffmanCodingTable* dc_table,
    int32_t* diff_dc,
    HuffmanCodingTable* ac_table,

    int32_t spectral_selection_start,
    int32_t spectral_selection_end,

    BitStream* bit_stream,

    int32_t successive_approximation_bit_low,

    int32_t* eob_run
){
  if (spectral_selection_start==0) {
        int32_t dc_magnitude=HuffmanCodingTable_lookup(dc_table, bit_stream);
        int32_t dc_value=0;
        if (dc_magnitude>0) {
            dc_value=BitStream_get_bits_advance(bit_stream, dc_magnitude);
            dc_value=twos_complement(dc_magnitude, dc_value);
        }

        *diff_dc+=dc_value;

        block_mem[0]=*diff_dc<<successive_approximation_bit_low;

        spectral_selection_start=1;
    }

    if (spectral_selection_end==0) {
        return;
    }

    if (*eob_run>0) {
        *eob_run-=1;
        return;
    }

    for (int c=spectral_selection_start; c<=spectral_selection_end; c++) {
        int32_t ac_bits=HuffmanCodingTable_lookup(ac_table, bit_stream);

        if (ac_bits==0) {
            break;
        }

        uint32_t num_zeros=ac_bits>>4;
        uint32_t ac_magnitude=ac_bits&0xF;

        if (ac_magnitude==0) {
            if (num_zeros==15) {
                c+=15;//+1 implicitely from loop
                continue;
            }else {
                if (num_zeros>0){
                    uint32_t eob_run_bits=BitStream_get_bits_advance(bit_stream,num_zeros);
                    *eob_run=mask_u32(num_zeros) + eob_run_bits;
                }else{
                    *eob_run=0;
                }

                break;
            }
        }

        c+=num_zeros;
        if (c>spectral_selection_end) {
            break;
        }

        ac_bits=BitStream_get_bits_advance(bit_stream, ac_magnitude);

        int ac_value=twos_complement(ac_magnitude,ac_bits);

        block_mem[UNZIGZAG[c]]=ac_value<<successive_approximation_bit_low;
    }
}
static inline uint8_t refine_block(
    BLOCK_ELEMENT_TYPE block_mem[64],

    BitStream* bit_stream,

    uint8_t range_start,
    uint8_t range_end,

    uint8_t num_zeros,
    BLOCK_ELEMENT_TYPE bit
){
    for(int i= range_start;i<=range_end;i++){
        int index=UNZIGZAG[i];

        if(block_mem[index]==0){
            if(num_zeros==0){
                return i;
            }

            num_zeros -= 1;
        }else{
            int next_bit=BitStream_get_bits_advance(bit_stream,1);
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
static inline void decode_block_with_sbh(
    BLOCK_ELEMENT_TYPE block_mem[64],

    HuffmanCodingTable* ac_table,

    int32_t spectral_selection_start,
    int32_t spectral_selection_end,

    BitStream* bit_stream,

    int32_t successive_approximation_bit_low,
    int32_t successive_approximation_bit_high,

    int32_t* eob_run
){
    discard successive_approximation_bit_high;

    int succ_approx_bit_shifted=1<<successive_approximation_bit_low;
    
    if(spectral_selection_start == 0){
        if(BitStream_get_bits_advance(bit_stream,1)==1){
            block_mem[0] += succ_approx_bit_shifted;
        }
        return;
    }

    if(*eob_run>0){
        *eob_run -= 1;

        discard refine_block(
            block_mem,
            bit_stream, 

            spectral_selection_start, 
            spectral_selection_end, 

            64, 
            succ_approx_bit_shifted
        );

        return;
    }

    // go through all ac values, some of which may be run length encoded

    // if no AC values are encoded in current scan
    if(spectral_selection_end >= 1){
        for(uint8_t next_pixel_index=spectral_selection_start;next_pixel_index <= spectral_selection_end;){
            int ac_bits=HuffmanCodingTable_lookup(ac_table, bit_stream);

            // 4 most significant bits are number of zero-value bytes inserted before actual value (may be zero)
            uint32_t num_zeros=ac_bits>>4;
            // no matter number of '0' bytes inserted, last 4 bits in ac value denote (additional) pixel value
            int ac_magnitude=ac_bits&0xF;

            int value=0;

            switch(ac_magnitude){
                case 0:
                    switch(num_zeros){
                        case 15:
                            break; // num_zeros is 15, value is zero => 16 zeros written already
                        default:
                            *eob_run=0;
                            if(num_zeros>0){
                                uint32_t eob_run_bits=BitStream_get_bits_advance(bit_stream,num_zeros);
                                //guard eob_run_bits >= 1<<(num_zeros-1) else {fatalError("variable length bit pattern \(eob_run_bits) too small to warrant magnitude of \(num_zeros)")}
                                *eob_run=mask_u32(num_zeros) + eob_run_bits;
                            }
                            num_zeros=64;
                    }
                    break;
                case 1:
                    if(BitStream_get_bits_advance(bit_stream,1)==1){
                        value = succ_approx_bit_shifted;
                    }else{
                        value = -succ_approx_bit_shifted;
                    }
                    break;
                default:
                    fprintf(stderr,"error during block parsing\n");
                    exit(-80);
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
                block_mem[UNZIGZAG[next_pixel_index]]=value;
            }
            
            next_pixel_index+=1;
        }
    }
}

/**
 * @brief used internally for idct mask generation
 * 
 * @param u 
 * @return float 
 */
static inline float coeff(int u){
    if(u==0){
        return 1.0/sqrt(2.0);
    }else{
        return 1.0;
    }
}

#define IDCT_MASK_ELEMENT_TYPE float
typedef struct IDCTMaskSet {
    IDCT_MASK_ELEMENT_TYPE idct_element_masks[64][64];
} IDCTMaskSet;
static inline void IDCTMaskSet_generate(IDCTMaskSet* mask_set){
    for(int mask_index = 0;mask_index<64;mask_index++){
        for(int ix = 0;ix<8;ix++){
            for(int iy = 0;iy<8;iy++){
                int mask_u=mask_index%8;
                int mask_v=mask_index/8;

                float x_cos_arg = ((2.0 * (float)(iy) + 1.0) * (float)(mask_u) * M_PI) / 16.0;
                float y_cos_arg = ((2.0 * (float)(ix) + 1.0) * (float)(mask_v) * M_PI) / 16.0;

                float x_val = cos(x_cos_arg) * coeff(mask_u);
                float y_val = cos(y_cos_arg) * coeff(mask_v);
                // the divide by 4 comes from the spec, from the algorithm to reverse the application of the IDCT
                float value = (x_val * y_val)/4;

                int mask_pixel_index=8*ix+iy;
                mask_set->idct_element_masks[mask_index][mask_pixel_index]=value;
            }
        }
    }
}

typedef struct JpegParser{
    uint32_t file_size;
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
}JpegParser;

void JpegParser_init_empty(JpegParser* parser){
    parser->file_size=0;
    parser->file_contents=NULL;
    parser->current_byte_position=0;

    for(int i=0;i<4;i++){
        for(int j=0;j<64;j++)
            parser->quant_tables[i][j]=0;

        parser->ac_coding_tables[i].code_length_lookup_table=NULL;
        parser->ac_coding_tables[i].max_code_length_bits=0;
        parser->ac_coding_tables[i].value_lookup_table=NULL;

        parser->dc_coding_tables[i].code_length_lookup_table=NULL;
        parser->dc_coding_tables[i].max_code_length_bits=0;
        parser->dc_coding_tables[i].value_lookup_table=NULL;
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
}

struct JpegParser_process_channel_argset{
    JpegParser* parser;
    int c;
    IDCTMaskSet* idct_mask_set;
    uint32_t block_range_start;
    uint32_t block_range_end;
};
void JpegParser_process_channel(JpegParser* parser,int c,IDCTMaskSet* idct_mask_set,uint32_t block_range_start,uint32_t block_range_end);
void* JpegParser_process_channel_pthread(struct JpegParser_process_channel_argset* args){
    JpegParser_process_channel(args->parser,args->c,args->idct_mask_set,args->block_range_start,args->block_range_end);
    return NULL;
}
void JpegParser_process_channel(JpegParser* parser,int c,IDCTMaskSet* idct_mask_set,uint32_t scan_id_start,uint32_t scan_id_end){
    QuantizationTable* component_quant_table=&parser->quant_tables[parser->image_components[c].quant_table_specifier];

    // -- reverse idct and quantization table application

    float* out_block_downsampled=parser->image_components[c].out_block_downsampled;
    //float* out_block_downsampled_reordered=parser->image_components[c].out_block_downsampled_reordered;

    uint32_t num_blocks_in_scan=parser->image_components[c].num_blocks_in_scan;

    for (uint32_t scan_id=scan_id_start; scan_id<scan_id_end; scan_id++) {
        for(uint32_t block_id=0;block_id<num_blocks_in_scan;block_id++){
            int32_t* in_block=&parser->image_components[c].scan_memory[scan_id][block_id*64];

            uint32_t _block_id=(scan_id*num_blocks_in_scan+block_id);
            float* out_block=&out_block_downsampled[_block_id*64];

            // use first idct mask index to initialize storage
            {
                float cosine_mask_strength=in_block[0]*(*component_quant_table)[0];
                float* idct_mask=idct_mask_set->idct_element_masks[0];

                float idct_mask_0_value=idct_mask[0]*cosine_mask_strength;
                
                for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                    out_block[pixel_index]=idct_mask_0_value;
                }
            }

            for(uint32_t cosine_index = 1;cosine_index<64;cosine_index++){
                if(in_block[cosine_index] == 0) {
                    continue;
                }

                float cosine_mask_strength=in_block[cosine_index]*(*component_quant_table)[cosine_index];
                float* idct_mask=idct_mask_set->idct_element_masks[cosine_index];
                
                for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                    out_block[pixel_index]+=idct_mask[pixel_index]*cosine_mask_strength;
                }
            }
        }
    }
}

struct SomeData{
    JpegParser* parser;
    int channel;
    _Atomic uint32_t num_scans_parsed;
    IDCTMaskSet* idct_mask_set;
};
void* ProcessIncomingScans_pthread(struct SomeData* somedata){
    uint32_t scan_id_start=0;
    uint32_t total_num_scans=somedata->parser->image_components[1].num_scans;
    while(scan_id_start<total_num_scans){
        uint32_t scan_id_end=atomic_load(&somedata->num_scans_parsed);

        uint32_t num_scans_to_process=scan_id_end-scan_id_start;
        if(num_scans_to_process){
            int c=somedata->channel;

            JpegParser_process_channel(somedata->parser,c,somedata->idct_mask_set,scan_id_start,scan_id_end);

            scan_id_start=scan_id_end;
        }else{
            struct timespec sleeptime={.tv_sec=0,.tv_nsec=100000};
            nanosleep(&sleeptime, NULL);
        }
    }

    return NULL;
}

static inline uint32_t max_u32(uint32_t a,uint32_t b){
    return (a>b)?a:b;
}
#define max(a,b) (_Generic((a), uint32_t: max_u32, float:fmaxf, double:fmax)(a,b))

#define println(...) {\
    printf("%s : %d | ",__FILE__,__LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
}

struct ScanComponent{
    uint32_t vert_sample_factor;
    uint32_t horz_sample_factor;

    uint32_t horz_samples;
    uint32_t vert_samples;

    uint32_t num_scans;
    uint32_t num_blocks_in_scan;
    
    uint32_t component_index_in_image;

    uint32_t num_blocks;

    uint32_t num_horz_blocks;

    HuffmanCodingTable* ac_table;
    HuffmanCodingTable* dc_table;

    BLOCK_ELEMENT_TYPE** scan_memory;
};

void JpegParser_parse_file(JpegParser* parser,ImageData* image_data,bool parallel){

    #define GET_NB ((uint32_t)((parser->file_contents)[parser->current_byte_position++]))

    #define GET_U8(VARIABLE) VARIABLE=GET_NB;
    #define GET_U16(VARIABLE) VARIABLE=((GET_NB<<8)| GET_NB);

    #define HB_U8(VARIABLE) ((VARIABLE&0xF0)>>4)
    #define LB_U8(VARIABLE) (VARIABLE&0xF)

    IDCTMaskSet idct_mask_set;
    IDCTMaskSet_generate(&idct_mask_set);

    struct SomeData async_scan_info[3]={
        {
            .parser=parser,
            .channel=0,
            .num_scans_parsed=0,
            .idct_mask_set=&idct_mask_set
        },
        {
            .parser=parser,
            .channel=1,
            .num_scans_parsed=0,
            .idct_mask_set=&idct_mask_set
        },
        {
            .parser=parser,
            .channel=2,
            .num_scans_parsed=0,
            .idct_mask_set=&idct_mask_set
        }
    };
    pthread_t async_scan_processors[3];

    struct ScanComponent scan_components[3];

    // +1 for each component to allow component with index 0 to be actively counted
    #define CHANNEL_COMPLETE 2016+64 // sum(0..<64) + 64
    uint32_t channel_completeness[3]={0,0,0};

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
                    uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    uint32_t comment_length=segment_size-2;

                    char* comment_str=malloc(comment_length+1);
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
                    uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        int destination_and_precision=GET_NB;
                        int destination=LB_U8(destination_and_precision);
                        int precision=HB_U8(destination_and_precision);
                        if (precision!=0) {
                            fprintf(stderr, "jpeg quant table precision is not 0 - it is %d\n",precision);
                            exit(-45);
                        }

                        uint8_t table_entries[64];
                        memcpy(table_entries,&parser->file_contents[parser->current_byte_position],64);
                        parser->current_byte_position+=64;

                        segment_bytes_read+=65;

                        for (int i=0; i<64; i++) {
                            parser->quant_tables[destination][i]=table_entries[ZIGZAG[i]];
                        }

                    }

                    parser->current_byte_position=segment_end_position;
                }
                break;

            case DHT:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        int table_index_and_class=GET_NB;

                        int table_index=LB_U8(table_index_and_class);
                        int table_class=HB_U8(table_index_and_class);

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

                        int32_t total_num_values=0;
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

            case SOF0:
            case SOF2:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=parser->current_byte_position+segment_size-2;

                    GET_U8(parser->P);
                    GET_U16(parser->real_Y);
                    GET_U16(parser->real_X);
                    GET_U8(parser->Nf);

                    if (parser->P!=8) {
                        fprintf(stderr,"image precision is not 8 - is %d instead\n",parser->P);
                        exit(-46);
                    }

                    for (uint32_t i=0; i<parser->Nf; i++) {
                        parser->image_components[i].component_id=GET_NB;

                        uint32_t sample_factors=GET_NB;

                        parser->image_components[i].vert_sample_factor=HB_U8(sample_factors);
                        parser->image_components[i].horz_sample_factor=LB_U8(sample_factors);

                        parser->image_components[i].quant_table_specifier=GET_NB;

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

                    for (uint32_t i=0; i<parser->Nf; i++) {
                        parser->image_components[i].vert_samples=(ROUND_UP(parser->Y,8*parser->max_component_vert_sample_factor))*parser->image_components[i].vert_sample_factor/parser->max_component_vert_sample_factor;
                        parser->image_components[i].horz_samples=(ROUND_UP(parser->X,8*parser->max_component_horz_sample_factor))*parser->image_components[i].horz_sample_factor/parser->max_component_horz_sample_factor;

                        parser->X=max(parser->X,parser->image_components[i].horz_samples);
                        parser->Y=max(parser->Y,parser->image_components[i].vert_samples);

                        uint32_t component_data_size=parser->image_components[i].vert_samples*parser->image_components[i].horz_samples;

                        uint32_t component_num_scans=parser->image_components[i].vert_samples/parser->image_components[i].vert_sample_factor/8;
                        uint32_t component_num_scan_elements=parser->image_components[i].horz_samples*parser->image_components[i].vert_sample_factor*8;
                        parser->image_components[i].scan_memory=malloc(sizeof(BLOCK_ELEMENT_TYPE*)*component_num_scans);
                        for (uint32_t s=0; s<component_num_scans; s++) {
                            // this needs to be zero initialised for scans where successive approximation high is not zero
                            parser->image_components[i].scan_memory[s]=calloc(component_num_scan_elements,sizeof(BLOCK_ELEMENT_TYPE));
                        }

                        parser->image_components[i].num_scans=component_num_scans;
                        parser->image_components[i].num_blocks_in_scan=component_num_scan_elements/64;

                        parser->image_components[i].out_block_downsampled=malloc(sizeof(float)*component_data_size);

                        parser->image_components[i].total_num_blocks=parser->image_components[i].vert_samples*parser->image_components[i].horz_samples/64;
                    }

                    parser->current_byte_position=segment_end_position;
                }
                break;

            case SOS:
                {
                    GET_U16(segment_size);

                    uint8_t num_scan_components=GET_NB;

                    bool is_interleaved=num_scan_components != 1;

                    uint8_t scan_component_id[3];
                    uint8_t scan_component_ac_table_index[3];
                    uint8_t scan_component_dc_table_index[3];

                    for (uint32_t i=0; i<num_scan_components; i++) {
                        scan_component_id[i]=GET_NB;

                        uint8_t table_indices=GET_NB;

                        scan_component_dc_table_index[i]=HB_U8(table_indices);
                        scan_component_ac_table_index[i]=LB_U8(table_indices);
                    }

                    uint32_t spectral_selection_start=GET_NB;
                    uint32_t spectral_selection_end=GET_NB;

                    int32_t eob_run=0;

                    int32_t successive_approximation_bits=GET_NB;
                    int32_t successive_approximation_bit_low=LB_U8(successive_approximation_bits);
                    int32_t successive_approximation_bit_high=HB_U8(successive_approximation_bits);

                    int32_t differential_dc[3]={0,0,0};

                    uint32_t stuffed_byte_index_count=0;
                    uint32_t stuffed_byte_index_capacity=1024;
                    uint32_t* stuffed_byte_indices=malloc(sizeof(uint32_t)*stuffed_byte_index_capacity);

                    uint8_t* de_zeroed_file_contents=malloc(parser->file_size);
                    uint32_t out_index=0;
                    for (uint32_t i=0; i<(parser->file_size-parser->current_byte_position-1); i++) {
                        uint8_t current_byte=parser->file_contents[parser->current_byte_position+i];
                        uint8_t next_byte=parser->file_contents[parser->current_byte_position+i+1];

                        de_zeroed_file_contents[out_index]=current_byte;

                        if ((current_byte==0xFF) && (next_byte==0)) {
                            stuffed_byte_indices[stuffed_byte_index_count++]=out_index;

                            if (stuffed_byte_index_count==stuffed_byte_index_capacity) {
                                stuffed_byte_index_capacity*=2;
                                stuffed_byte_indices=realloc(stuffed_byte_indices, 4*stuffed_byte_index_capacity);
                            }

                            i++;
                        }
                        out_index+=1;
                    }

                    BitStream bit_stream;
                    BitStream_new(&bit_stream, de_zeroed_file_contents);

                    uint32_t mcu_cols=parser->image_components[0].horz_samples/parser->image_components[0].horz_sample_factor/8;
                    uint32_t mcu_rows=parser->image_components[0].vert_samples/parser->image_components[0].vert_sample_factor/8;

                    uint32_t scan_component_vert_sample_factor[3];
                    uint32_t scan_component_horz_sample_factor[3];
                    uint32_t scan_component_index_in_image[3];

                    for (uint32_t scan_component_index=0; scan_component_index<num_scan_components; scan_component_index++) {
                        for (uint32_t i=0; i<parser->Nf; i++) {
                            if (parser->image_components[i].component_id==scan_component_id[scan_component_index]) {
                                scan_component_vert_sample_factor[scan_component_index]=parser->image_components[i].vert_sample_factor;
                                scan_component_horz_sample_factor[scan_component_index]=parser->image_components[i].horz_sample_factor;

                                scan_component_index_in_image[scan_component_index]=i;
                                break;
                            }
                        }

                        if (scan_component_index_in_image[scan_component_index]==INT32_MAX) {
                            fprintf(stderr,"did not find image component?!\n");
                            exit(-102);
                        }
                    }

                    for (int c=0; c<num_scan_components; c++) {
                        scan_components[c].vert_sample_factor=scan_component_vert_sample_factor[c];
                        scan_components[c].horz_sample_factor=scan_component_horz_sample_factor[c];

                        uint32_t component_index_in_image=scan_component_index_in_image[c];
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
                            uint32_t t=scan_components[c].component_index_in_image;
                            for (uint32_t i=spectral_selection_start; i<=spectral_selection_end; i++) {
                                channel_completeness[t]+=i+1;
                            }
                            if (channel_completeness[t]==CHANNEL_COMPLETE){
                                pthread_create(&async_scan_processors[t], NULL, (pthread_callback)ProcessIncomingScans_pthread, &async_scan_info[t]);
                            }
                        }
                    }

                    for (uint32_t mcu_row=0;mcu_row<mcu_rows;mcu_row++) {
                        for (uint32_t mcu_col=0;mcu_col<mcu_cols;mcu_col++) {
                            for (int c=0; c<num_scan_components; c++) {
                                for (uint32_t vert_sid=0; vert_sid<scan_components[c].vert_sample_factor; vert_sid++) {
                                    for (uint32_t horz_sid=0; horz_sid<scan_components[c].horz_sample_factor; horz_sid++) {
                                        uint32_t block_col=mcu_col*scan_components[c].horz_sample_factor + horz_sid;
                                        //uint32_t block_row=mcu_row*scan_components[c].vert_sample_factor + vert_sid;

                                        uint32_t component_block_id;
                                        if (is_interleaved) {
                                            component_block_id = block_col + vert_sid * scan_components[c].num_horz_blocks;
                                        }else {
                                            component_block_id =
                                                mcu_col
                                                    * scan_components[c].horz_sample_factor*scan_components[c].vert_sample_factor
                                                + horz_sid
                                                + vert_sid
                                                    * scan_components[c].horz_sample_factor;
                                        }
                                        
                                        int32_t* block_mem=&scan_components[c].scan_memory[mcu_row][component_block_id*64];

                                        if (successive_approximation_bit_high==0) {
                                            decode_block(
                                                block_mem, 
                                                scan_components[c].dc_table, 
                                                &differential_dc[c],
                                                scan_components[c].ac_table, 
                                                spectral_selection_start, 
                                                spectral_selection_end, 
                                                &bit_stream, 
                                                successive_approximation_bit_low, 
                                                &eob_run
                                            );
                                        }else{
                                            decode_block_with_sbh(
                                                block_mem,
                                                scan_components[c].ac_table, 
                                                spectral_selection_start, 
                                                spectral_selection_end, 
                                                &bit_stream, 
                                                successive_approximation_bit_low, 
                                                successive_approximation_bit_high, 
                                                &eob_run
                                            );
                                        }
                                    }
                                }
                            }
                        }

                        if(parallel&&(successive_approximation_bit_low==0))
                            for(int t=0;t<num_scan_components;t++){
                                uint8_t index=scan_components[t].component_index_in_image;
                                if (channel_completeness[index]==CHANNEL_COMPLETE)
                                    atomic_fetch_add(&async_scan_info[index].num_scans_parsed, 1);
                            }
                    }

                    uint32_t bytes_read_from_stream=bit_stream.next_data_index;

                    bytes_read_from_stream-=bit_stream.buffer_bits_filled/8;

                    uint32_t stuffed_byte_count_skipped=0;
                    for (uint32_t i=0; i<stuffed_byte_index_count; i++) {
                        if (bytes_read_from_stream>=stuffed_byte_indices[i]) {
                            stuffed_byte_count_skipped+=1;
                        }else {
                            break;
                        }
                    }
                    parser->current_byte_position+=bytes_read_from_stream+stuffed_byte_count_skipped;

                    free(de_zeroed_file_contents);
                    free(stuffed_byte_indices);
                }

                break;

            default:
                fprintf(stderr,"unhandled segment %s ( %X ) \n",Image_jpeg_segment_type_name(next_header),next_header);
                exit(-40);
        }
    }

    println("parsing done at %f",current_time());

    if(parallel)
        for(int t=0;t<3;t++)
            pthread_join(async_scan_processors[t], NULL);

    if (!parallel) {
        for(int c=0;c<3;c++){
            JpegParser_process_channel(parser,c,&idct_mask_set,0,parser->image_components[c].num_scans);
        }
    }

    println("processing done at %f",current_time());
}

struct JpegParser_convert_colorspace_argset{
    JpegParser* parser;
    ImageData* image_data;
    uint32_t scan_index_start;
    uint32_t scan_index_end;
};
void JpegParser_convert_colorspace(JpegParser* parser,ImageData* image_data,uint32_t scan_index_start,uint32_t scan_index_end);
void* JpegParser_convert_colorspace_pthread(struct JpegParser_convert_colorspace_argset* args){
    JpegParser_convert_colorspace(args->parser,args->image_data,args->scan_index_start,args->scan_index_end);
    return NULL;
}
void JpegParser_convert_colorspace(JpegParser* parser,ImageData* image_data,uint32_t scan_index_start,uint32_t scan_index_end){
    float* y=parser->image_components[0].out_block_downsampled;
    float* cr=parser->image_components[1].out_block_downsampled;
    float* cb=parser->image_components[2].out_block_downsampled;

    uint32_t row_scale[3]={
        parser->max_component_vert_sample_factor/parser->image_components[0].vert_sample_factor,
        parser->max_component_vert_sample_factor/parser->image_components[1].vert_sample_factor,
        parser->max_component_vert_sample_factor/parser->image_components[2].vert_sample_factor
    };
    uint32_t col_scale[3]={
        parser->max_component_horz_sample_factor/parser->image_components[0].horz_sample_factor,
        parser->max_component_horz_sample_factor/parser->image_components[1].horz_sample_factor,
        parser->max_component_horz_sample_factor/parser->image_components[2].horz_sample_factor
    };

    uint32_t pixels_per_scan[3]={
        parser->image_components[0].num_blocks_in_scan*64,
        parser->image_components[1].num_blocks_in_scan*64,
        parser->image_components[2].num_blocks_in_scan*64,
    };

    uint32_t num_mcu_cols=parser->image_components[1].num_blocks_in_scan;

    for (uint32_t s=scan_index_start; s<scan_index_end; s++) {
        uint32_t scan_offset=s*pixels_per_scan[0];
        for (uint32_t mcu_col=0; mcu_col<num_mcu_cols; mcu_col++) {
            uint32_t mcu_offset=mcu_col*parser->max_component_horz_sample_factor*8;
            for(uint32_t mcu_row_sample=0;mcu_row_sample<parser->max_component_vert_sample_factor;mcu_row_sample++){
                uint32_t mcu_row_offset=mcu_row_sample*8*parser->X;
                for(uint32_t mcu_col_sample=0;mcu_col_sample<parser->max_component_horz_sample_factor;mcu_col_sample++){
                    uint32_t mcu_col_offset=mcu_col_sample*8;

                    uint32_t block_cols[3]={
                        mcu_col*parser->image_components[0].horz_sample_factor + mcu_col_sample/col_scale[0],
                        mcu_col*parser->image_components[1].horz_sample_factor + mcu_col_sample/col_scale[1],
                        mcu_col*parser->image_components[2].horz_sample_factor + mcu_col_sample/col_scale[2]
                    };
                    uint32_t block_rows[3]={
                        s*parser->image_components[0].vert_sample_factor + mcu_row_sample/row_scale[0],
                        s*parser->image_components[1].vert_sample_factor + mcu_row_sample/row_scale[1],
                        s*parser->image_components[2].vert_sample_factor + mcu_row_sample/row_scale[2]
                    };
                    uint32_t block_offsets[3]={
                        (block_cols[0] + block_rows[0] * parser->image_components[0].horz_samples/8)*64,
                        (block_cols[1] + block_rows[1] * parser->image_components[1].horz_samples/8)*64,
                        (block_cols[2] + block_rows[2] * parser->image_components[2].horz_samples/8)*64
                    };

                    for(uint32_t y_i=0;y_i<8;y_i++){
                        uint32_t y_offset=y_i*parser->X;
                        for (uint32_t x_i=0; x_i<8; x_i++) {
                            // -- re-order from block-orientation to final image orientation

                            uint32_t i=
                                scan_offset
                                +mcu_offset
                                +mcu_row_offset
                                +mcu_col_offset
                                +y_offset
                                +x_i;

                            uint32_t y_index=block_offsets[0]+y_i*8+x_i;
                            float Y=y[y_index];

                            uint32_t cr_index=block_offsets[1]+y_i*8+x_i;
                            float Cr=cr[cr_index];
                            
                            uint32_t cb_index=block_offsets[2]+y_i*8+x_i;
                            float Cb=cb[cb_index];

                            // -- convert ycbcr to rgb

                            float R = Cr * 1.402 + Y;
                            float B = Cb * 1.772 + Y;
                            float G = (Y - 0.114 * B - 0.299 * R ) / 0.587;

                            // -- deinterlace and convert to uint8

                            image_data->data[(i)* 4 + 0] = (uint8_t)clamp_f32(0.0,255.0,R+128);
                            image_data->data[(i)* 4 + 1] = (uint8_t)clamp_f32(0.0,255.0,G+128);
                            image_data->data[(i)* 4 + 2] = (uint8_t)clamp_f32(0.0,255.0,B+128);
                            image_data->data[(i)* 4 + 3] = UINT8_MAX;
                        }
                    }
                }
            }
        }
    }
}

ImageParseResult Image_read_jpeg(const char* filepath,ImageData* image_data){
    printf("starting jpeg parser at %f\n",current_time());

    FILE* file=fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "file '%s' not found\n",filepath);
        return IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
    }

    JpegParser parser;
    JpegParser_init_empty(&parser);

    discard fseek(file,0,SEEK_END);
    parser.file_size=ftell(file);
    rewind(file);

    parser.file_contents=malloc(parser.file_size);
    discard fread(parser.file_contents, 1, parser.file_size, file);
    
    fclose(file);

    const uint32_t num_threads=1;
    
    JpegParser_parse_file(&parser, image_data, num_threads>1);

    // -- convert idct magnitude values to channel pixel values
    // then upsample channels to final resolution

    IDCTMaskSet idct_mask_set;
    IDCTMaskSet_generate(&idct_mask_set);

    // and convert ycbcr to rgb

    image_data->interleaved=true;
    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;

    int color_space=(parser.image_components[0].component_id<<8*2)
        | (parser.image_components[1].component_id<<8*1)
        | (parser.image_components[2].component_id<<8*0);

    switch(color_space){
        case 0x010203:
            {
                int total_num_pixels_in_image=parser.X*parser.Y;

                image_data->data=(uint8_t*)malloc(sizeof(uint8_t)*total_num_pixels_in_image*4);

                if(num_threads>1){
                    struct JpegParser_convert_colorspace_argset* thread_args=malloc(num_threads*sizeof(struct JpegParser_convert_colorspace_argset));
                    pthread_t* threads=malloc(num_threads*sizeof(pthread_t));

                    int num_scans_thread=parser.image_components[0].num_scans/num_threads;
                    for(uint32_t i=0;i<num_threads;i++){
                        thread_args[i].parser=&parser;
                        thread_args[i].image_data=image_data;
                        thread_args[i].scan_index_start=i*num_scans_thread;
                        thread_args[i].scan_index_end=(i+1)*num_scans_thread;

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

                    printf("converted color space at %f\n",current_time());
                }else{
                    JpegParser_convert_colorspace(&parser,image_data,0,parser.image_components[0].num_scans);
                }
            }
            break;

        default:
            fprintf(stderr,"color space %X other than YCbCr (component IDs 1,2,3) currently unimplemented",color_space);
            exit(-65);
    }

    println("colorspace done at %f",current_time());

    // -- crop to real size

    if(parser.X!=parser.real_X || parser.Y!=parser.real_Y){
        uint8_t* real_data=malloc(parser.real_X*parser.real_Y*4);

        for (uint32_t y=0; y<parser.real_Y; y++) {
            for(uint32_t x=0; x<parser.real_X; x++) {
                real_data[(y*parser.real_X+x)*4+0]=image_data->data[(y*parser.X+x)*4+0];
                real_data[(y*parser.real_X+x)*4+1]=image_data->data[(y*parser.X+x)*4+1];
                real_data[(y*parser.real_X+x)*4+2]=image_data->data[(y*parser.X+x)*4+2];
                real_data[(y*parser.real_X+x)*4+3]=image_data->data[(y*parser.X+x)*4+3];
            }
        }

        free(image_data->data);

        image_data->height=parser.real_Y;
        image_data->width=parser.real_X;
        image_data->data=real_data;

        printf("cropped to final size at %f\n",current_time());
    }

    // -- parsing done. free all resources

    free(parser.file_contents);

    for(int i=0;i<4;i++){
        HuffmanCodingTable_destroy(&parser.ac_coding_tables[i]);
        HuffmanCodingTable_destroy(&parser.dc_coding_tables[i]);
    }

    for(int c=0;c<3;c++){
        free(parser.image_components[c].out_block_downsampled);
        for(uint32_t s=0;s<parser.image_components[c].num_scans;s++){
            free(parser.image_components[c].scan_memory[s]);
        }
        free(parser.image_components[c].scan_memory);
    }

    return IMAGE_PARSE_RESULT_OK;
}

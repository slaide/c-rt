#include "app/app.h"
#include "app/error.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

float min_f32(float a, float b){
    return (a<b)?a:b;
}
float max_f32(float a, float b){
    return (a>b)?a:b;
}
float clamp_f32(float v_min,float v_max, float v){
    return max_f32(v_min, min_f32(v_max, v));
}

void cause_sigseg(){
    int* whatever=NULL;
    int a=*whatever;
    printf("should not print %d\n",a);
}

/**
 * @brief format integer as binary number
 * 
 * @param v 
 * @param s 
 */
void integer_to_str(int v,char s[32]){
    const int num_bits=32;
    for (int i=0; i<num_bits; i++) {
        if ((v>>(num_bits-1-i))&1) {
            s[i]='1';
        }else {
            s[i]='0';
        }
    }
}
/**
 * @brief format integer as binary number
 * 
 * @param v 
 * @param s 
 */
void uint64_to_str(uint64_t v,char s[64]){
    const int num_bits=64;
    for (int i=0; i<num_bits; i++) {
        if ((v>>(num_bits-1-i))&1) {
            s[i]='1';
        }else {
            s[i]='0';
        }
    }
}

typedef  enum JpegSegmentType{
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

uint32_t mask_u32(uint32_t n){
    if (n==0) return  0;
    uint32_t shift_by=32-n;
    uint32_t base=0xffffffff;
    uint32_t ret=base>>shift_by;

    return ret;
}
uint64_t mask_u64(uint64_t n){
    if (n==0) return  0;
    uint64_t shift_by=64-n;
    uint64_t base=0xffffffffffffffff;
    uint64_t ret=base>>shift_by;

    return ret;
}

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

typedef struct ImageComponent{
    int vert_samples;
    int horz_samples;

    int component_id;
    int vert_sample_factor;
    int horz_sample_factor;
    int quant_table_specifier;
}ImageComponent;

typedef int QuantizationTable[64];
typedef struct HuffmanCodingTable{
    uint32_t max_code_length_bits;

    int* value_lookup_table;
    uint32_t* code_length_lookup_table;
}HuffmanCodingTable;

/**
 * @brief reverse a sequence of len bits
 * 
 * @param bits 
 * @param len 
 * @return int 
 */
int reverse_bits(int bits,int len){
    int ret=0;
    for (int i=0; i<len; i++) {
        int nth_bit=(bits&(1<<i))>>i;
        ret|=nth_bit<<(len-1-i);
    }
    return ret;
}

struct LookupLeaf{
    int value;
    uint32_t len;
};
struct ParseLeaf{
    bool present;

    int value;
    uint32_t len;

    uint32_t code;
    uint32_t rcode;
};

int ParseLeaf_compare(const struct ParseLeaf* left, const struct ParseLeaf* right){
    if (left->rcode<right->rcode) {
        return -1;
    }else if(left->rcode>right->rcode) {
        return 1;
    }else {
        return 0;
    }
}

/**
 * @brief create a new huffman coding table at the target location based on the input values
 * 
 * @param table 
 * @param num_values_of_length 
 * @param total_num_values 
 * @param value_code_lengths 
 * @param values 
 */
void HuffmanCodingTable_new(
    HuffmanCodingTable* table,

    int num_values_of_length[16],

    uint32_t total_num_values,
    uint32_t value_code_lengths[260],
    int values[260]
){
    for (int i=0; i<16; i++) {
        if (num_values_of_length[i]>0){
            table->max_code_length_bits=i+1;
        }
    }

    struct ParseLeaf parse_leafs[260];
    for (uint32_t i=0; i<260; i++) {
        if (i<total_num_values) {
            parse_leafs[i].present=true;

            parse_leafs[i].value=values[i];
            parse_leafs[i].len=value_code_lengths[i];

            parse_leafs[i].code=0;
            parse_leafs[i].rcode=0;
        }else {
            parse_leafs[i].present=false;
            
            parse_leafs[i].value=0;
            parse_leafs[i].len=0;

            parse_leafs[i].code=0;
            parse_leafs[i].rcode=0;
        }
    }

    uint32_t bl_count[17];
    memset(bl_count,0,17*4);
    for (uint32_t i=0; i<total_num_values; i++) {
        bl_count[parse_leafs[i].len]+=1;
    }

    uint32_t next_code[17];
    memset(next_code,0,17*4);
    for (uint32_t i=1; i<=table->max_code_length_bits; i++) {
        next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
    }

    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* current_leaf=&parse_leafs[i];
        current_leaf->code=next_code[current_leaf->len] & MASK(current_leaf->len);
        next_code[current_leaf->len]+=1;

        current_leaf->rcode=reverse_bits(current_leaf->code,current_leaf->len);
    }

    qsort(parse_leafs, total_num_values, sizeof(struct ParseLeaf), (int(*)(const void*,const void*))ParseLeaf_compare);

    uint32_t num_possible_leafs=1<<table->max_code_length_bits;
    table->value_lookup_table=malloc(num_possible_leafs*sizeof(int));
    table->code_length_lookup_table=malloc(num_possible_leafs*sizeof(uint32_t));
    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* leaf=&parse_leafs[i];

        uint32_t mask_len=table->max_code_length_bits - leaf->len;
        uint32_t mask=mask_u32(mask_len);

        for (uint32_t j=0; j<=mask; j++) {
            uint32_t leaf_index=(leaf->code<<mask_len)+j;
            
            table->value_lookup_table[leaf_index]=leaf->value;
            table->code_length_lookup_table[leaf_index]=leaf->len;
        }
    }
}
typedef struct BitStream{
    uint8_t* data;
    int next_data_index;

    uint64_t buffer;
    int buffer_bits_filled;
}BitStream;

/**
 * @brief initialise stream (does NOT fill buffer automatically)
 * 
 * @param stream 
 * @param data 
 */
void BitStream_new(BitStream* stream,void* data){
    stream->data=data;
    stream->next_data_index=0;
    stream->buffer=0;
    stream->buffer_bits_filled=0;
}
/**
 * @brief fill internal bit buffer (used for fast lookup)
 * 
 * @param stream 
 */
void BitStream_fill_buffer(BitStream* stream){
    uint64_t num_bytes_missing=7-stream->buffer_bits_filled/8;
    stream->buffer=stream->buffer<<(8*num_bytes_missing);

    for(uint64_t i=0;i<num_bytes_missing;i++){
        uint64_t index=stream->next_data_index+i;
        uint64_t shift_by=(num_bytes_missing-1-i)*8;

        uint64_t next_byte=stream->data[index];
        stream->buffer |=  next_byte << shift_by;
    }
    stream->buffer_bits_filled+=num_bytes_missing*8;
    stream->next_data_index+=num_bytes_missing;
}
/**
 * @brief get next n bits from stream
 * n must not be larger than 57. the internal bit buffer is automatically filled if it was not big enough at function start.
 * this function does NOT advance the internal state, i.e. repeated calls to this function with the same arguments will yield the same result.
 * call BitStream_advance to manually advance the stream.
 * @param stream 
 * @param n_bits 
 * @return int 
 */
uint64_t BitStream_get_bits(BitStream* stream,int n_bits){
    if (n_bits>stream->buffer_bits_filled) {
        BitStream_fill_buffer(stream);
    }

    uint64_t shift_by=stream->buffer_bits_filled-n_bits;
    uint64_t res=stream->buffer>>shift_by;
    return res;
}
/**
 * @brief 
 * 
 * @param stream 
 * @param n_bits 
 */
void BitStream_advance(BitStream* stream,int n_bits){
    if (n_bits>stream->buffer_bits_filled) {
        fprintf(stderr, "bitstream advance by %d bits invalid with %d current buffer length\n",n_bits,stream->buffer_bits_filled);
        exit(-50);
    }
    stream->buffer_bits_filled-=n_bits;
    uint64_t mask=mask_u64(stream->buffer_bits_filled);
    stream->buffer&=mask;
}

uint64_t BitStream_get_bits_advance(BitStream* stream,int n_bits){
    uint64_t res=BitStream_get_bits(stream, n_bits);

    BitStream_advance(stream,n_bits);

    return res;
}

int HuffmanCodingTable_lookup(
    HuffmanCodingTable* table,
    BitStream* bit_stream
){
    uint64_t bits=BitStream_get_bits(bit_stream, table->max_code_length_bits);

    int value=table->value_lookup_table[bits];
    int code_length=table->code_length_lookup_table[bits];
    BitStream_advance(bit_stream, code_length);

    return value;
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

#define BLOCK_ELEMENT_TYPE int32_t

void decode_block(
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

    if (spectral_selection_end>=1) {
        if (*eob_run>0) {
            *eob_run-=1;
            return;
        }
    }

    for (int c=spectral_selection_start; c<=spectral_selection_end; c++) {
        int ac_bits=HuffmanCodingTable_lookup(ac_table, bit_stream);

        if (ac_bits==0) {
            break;
        }

        int num_zeros=ac_bits>>4;
        int ac_magnitude=ac_bits&0xF;

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
uint8_t refine_block(
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
void decode_block_with_sbh(
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

float coeff(int u){
    if(u==0){
        return 1.0/sqrt(2.0);
    }else{
        return 1.0;
    }
}

#define  PI 

#define IDCT_MASK_ELEMENT_TYPE float
typedef struct IDCTMaskSet {
    IDCT_MASK_ELEMENT_TYPE idct_element_masks[64][64];
} IDCTMaskSet;
void IDCTMaskSet_generate(IDCTMaskSet* mask_set){
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

ImageParseResult Image_read_jpeg(const char* filepath,ImageData* image_data){
    FILE* file=fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "file '%s' not found\n",filepath);
        return IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
    }

    discard fseek(file,0,SEEK_END);
    int file_size=ftell(file);
    rewind(file);

    uint8_t* file_contents=malloc(file_size);
    discard fread(file_contents, 1, file_size, file);
    
    fclose(file);

    int current_byte_position=0;

    #define GET_NB ((uint32_t)(((uint8_t*)(file_contents))[current_byte_position++]))

    #define GET_U8(VARIABLE) VARIABLE=GET_NB;
    #define GET_U16(VARIABLE) VARIABLE=GET_NB<<8; VARIABLE|=GET_NB;

    #define HB_U8(VARIABLE) ((VARIABLE&0xF0)>>4)
    #define LB_U8(VARIABLE) (VARIABLE&0xF)

    #define SKIP_SEGMENT GET_U16(segment_size); current_byte_position=current_byte_position+segment_size-2;

    // data that will be parsed later
    QuantizationTable quant_tables[4];
    HuffmanCodingTable ac_coding_tables[4]={
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
    };
    HuffmanCodingTable dc_coding_tables[4]={
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
        {
            .max_code_length_bits=0,
            .value_lookup_table=NULL,
            .code_length_lookup_table=NULL
        },
    };

    int max_component_vert_sample_factor=0;
    int max_component_horz_sample_factor=0;

    BLOCK_ELEMENT_TYPE* component_data[3]={NULL,NULL,NULL};
    ImageComponent image_components[3];

    int P,X,Y,Nf;
    int real_X,real_Y;


    bool parsing_done=false;
    while (!parsing_done) {
        uint32_t next_header;
        GET_U16(next_header);

        uint32_t segment_size;

        // implementing a new segment parser looks like this:
        /*
            case NEW_SEGMENT_TYPE:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;

                    // new code here

                    current_byte_position=segment_end_position;
                }
                break;
        */
        
        switch (next_header) {
            case JPEG_SEGMENT_SOI:
                break;
            case JPEG_SEGMENT_EOI:
                parsing_done=true;
                break;

            case COM:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;

                    // new code here

                    current_byte_position=segment_end_position;
                }
                break;

            case APP0:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;
                    current_byte_position=segment_end_position;
                }
                break;

            case APP1:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;
                    current_byte_position=segment_end_position;
                }
                break;

            case DQT:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        int destination_and_precision=GET_NB;
                        int destination=LB_U8(destination_and_precision);
                        int precision=HB_U8(destination_and_precision);
                        if (precision!=0) {
                            fprintf(stderr, "jpeg quant table precision is not 0 - it is %d\n",precision);
                            exit(-45);
                        }

                        int table_entries[64];
                        for (int i=0;i<64;i++){
                            table_entries[i]=GET_NB;
                        }

                        segment_bytes_read+=65;

                        for (int i=0; i<64; i++) {
                            quant_tables[destination][i]=table_entries[ZIGZAG[i]];
                        }

                    }

                    current_byte_position=segment_end_position;
                }
                break;

            case DHT:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;

                    uint32_t segment_bytes_read=0;
                    while(segment_bytes_read<segment_size-2){
                        int table_index_and_class=GET_NB;

                        int table_index=LB_U8(table_index_and_class);
                        int table_class=HB_U8(table_index_and_class);

                        HuffmanCodingTable* target_table=NULL;
                        switch (table_class) {
                            case  0:
                                target_table=&dc_coding_tables[table_index];
                                break;
                            case  1:
                                target_table=&ac_coding_tables[table_index];
                                break;
                            default:
                                exit(FATAL_UNEXPECTED_ERROR);
                        }

                        int total_num_values=0;
                        int num_values_of_length[16];
                        for (int i=0; i<16; i++) {
                            num_values_of_length[i]=GET_NB;

                            total_num_values+=num_values_of_length[i];
                        }

                        segment_bytes_read+=17;

                        int values[260];
                        memset(values, 0, 260*4);
                        uint32_t value_code_lengths[260];
                        memset(value_code_lengths, 0, 260*4);

                        int value_index=0;
                        for (uint32_t code_length=0; code_length<16; code_length++) {
                            for (int i=0; i<num_values_of_length[code_length]; i++) {
                                values[value_index]=GET_NB;
                                value_code_lengths[value_index]=code_length+1;

                                value_index+=1;
                            }
                        }

                        segment_bytes_read+=value_index;

                        HuffmanCodingTable_new(
                            target_table,
                            num_values_of_length,
                            total_num_values,
                            value_code_lengths,
                            values
                        );
                    }

                    current_byte_position=segment_end_position;
                }
                break;

            case SOF0:
            case SOF2:
                {
                    GET_U16(segment_size);
                    uint32_t segment_end_position=current_byte_position+segment_size-2;

                    GET_U8(P);
                    GET_U16(real_Y);
                    GET_U16(real_X);
                    GET_U8(Nf);

                    if (P!=8) {
                        fprintf(stderr,"image precision is not 8 - is %d instead\n",P);
                        exit(-46);
                    }

                    for (int i=0; i<Nf; i++) {
                        image_components[i].component_id=GET_NB;

                        int sample_factors=GET_NB;

                        image_components[i].vert_sample_factor=HB_U8(sample_factors);
                        image_components[i].horz_sample_factor=LB_U8(sample_factors);

                        image_components[i].quant_table_specifier=GET_NB;

                        if (image_components[i].vert_sample_factor>max_component_vert_sample_factor) {
                            max_component_vert_sample_factor=image_components[i].vert_sample_factor;
                        }
                        if (image_components[i].horz_sample_factor>max_component_horz_sample_factor) {
                            max_component_horz_sample_factor=image_components[i].horz_sample_factor;
                        }
                    }

                    X=ROUND_UP(real_X,8);
                    Y=ROUND_UP(real_Y,8);

                    image_data->height=Y;
                    image_data->width=X;

                    for (int i=0; i<Nf; i++) {
                        image_components[i].vert_samples=(ROUND_UP(Y,8*max_component_vert_sample_factor))*image_components[i].vert_sample_factor/max_component_vert_sample_factor;
                        image_components[i].horz_samples=(ROUND_UP(X,8*max_component_horz_sample_factor))*image_components[i].horz_sample_factor/max_component_horz_sample_factor;

                        int component_data_size=image_components[i].vert_samples*image_components[i].horz_samples;
                        component_data[i]=malloc(component_data_size*sizeof(BLOCK_ELEMENT_TYPE));
                        for(int p=0;p<component_data_size;p++){
                            component_data[i][p]=0.0;
                        }
                    }

                    image_data->data=malloc(image_data->height*image_data->width*4);

                    current_byte_position=segment_end_position;
                }
                break;

            case SOS:
                {
                    GET_U16(segment_size);

                    int num_scan_components=GET_NB;

                    bool is_interleaved=num_scan_components != 1;

                    int scan_component_id[3];
                    int scan_component_ac_table_index[3];
                    int scan_component_dc_table_index[3];

                    for (int i=0; i<num_scan_components; i++) {
                        scan_component_id[i]=GET_NB;

                        int table_indices=GET_NB;

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

                    int32_t dummy_block[64];
                    discard memset(dummy_block, 0, 64*4);

                    int stuffed_byte_index_count=0;
                    int stuffed_byte_index_capacity=1024;
                    int* stuffed_byte_indices=malloc(4*stuffed_byte_index_capacity);

                    uint8_t* de_zeroed_file_contents=malloc(file_size);
                    int out_index=0;
                    for (int i=0; i<file_size-/*1-*/current_byte_position; i++) {
                        uint8_t current_byte=file_contents[current_byte_position+i];
                        uint8_t next_byte=file_contents[current_byte_position+i+1];

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

                    uint32_t num_mcus=image_components[0].vert_samples*image_components[0].horz_samples/(8*8*image_components[0].horz_sample_factor*image_components[0].vert_sample_factor);

                    uint32_t mcu_cols=image_components[0].horz_samples/image_components[0].horz_sample_factor/8;

                    uint32_t scan_component_vert_sample_factor[3];
                    uint32_t scan_component_horz_sample_factor[3];
                    uint32_t scan_component_index_in_image[3];

                    for (int scan_component_index=0; scan_component_index<num_scan_components; scan_component_index++) {
                        for (int i=0; i<Nf; i++) {
                            if (image_components[i].component_id==scan_component_id[scan_component_index]) {
                                scan_component_vert_sample_factor[scan_component_index]=image_components[i].vert_sample_factor;
                                scan_component_horz_sample_factor[scan_component_index]=image_components[i].horz_sample_factor;

                                scan_component_index_in_image[scan_component_index]=i;
                                break;
                            }
                        }
                        if (scan_component_index_in_image[scan_component_index]==INT32_MAX) {
                            fprintf(stderr,"did not find image component?!\n");
                            exit(-102);
                        }
                    }

                    for (uint32_t mcu_id=0;mcu_id<num_mcus;mcu_id++) {
                        uint32_t mcu_row=mcu_id/mcu_cols;
                        uint32_t mcu_col=mcu_id%mcu_cols;

                        for (int scan_component_index=0; scan_component_index<num_scan_components; scan_component_index++) {
                            uint32_t component_vert_sample_factor=scan_component_vert_sample_factor[scan_component_index];
                            uint32_t component_horz_sample_factor=scan_component_horz_sample_factor[scan_component_index];
                            uint32_t component_index_in_image=scan_component_index_in_image[scan_component_index];

                            uint32_t num_component_blocks=image_components[component_index_in_image].horz_samples/8*image_components[component_index_in_image].vert_samples/8;

                            for (uint32_t vert_sid=0; vert_sid<component_vert_sample_factor; vert_sid++) {
                                for (uint32_t horz_sid=0; horz_sid<component_horz_sample_factor; horz_sid++) {
                                    uint32_t block_col=mcu_col*image_components[component_index_in_image].horz_sample_factor + horz_sid;
                                    uint32_t block_row=mcu_row*image_components[component_index_in_image].vert_sample_factor + vert_sid;

                                    uint32_t component_block_id;
                                    if (is_interleaved) {
                                        component_block_id = block_col + block_row * mcu_cols*image_components[component_index_in_image].horz_sample_factor;
                                    }else {
                                        component_block_id = mcu_id
                                            * image_components[component_index_in_image].horz_sample_factor*image_components[component_index_in_image].vert_sample_factor
                                            + horz_sid
                                            + vert_sid
                                            * image_components[component_index_in_image].horz_sample_factor;
                                    }
                                    
                                    int32_t* block_mem;
                                    bool use_dummy_block=component_block_id>=num_component_blocks;
                                    if (use_dummy_block) {
                                        block_mem=dummy_block;
                                    }else {
                                        block_mem=&component_data[component_index_in_image][component_block_id*64];
                                    }

                                    if (successive_approximation_bit_high==0) {
                                        decode_block(
                                            block_mem, 
                                            &dc_coding_tables[scan_component_dc_table_index[scan_component_index]], 
                                            &differential_dc[scan_component_index],
                                            &ac_coding_tables[scan_component_ac_table_index[scan_component_index]], 
                                            spectral_selection_start, 
                                            spectral_selection_end, 
                                            &bit_stream, 
                                            successive_approximation_bit_low, 
                                            &eob_run
                                        );
                                    }else {
                                        decode_block_with_sbh(
                                            block_mem,
                                            &ac_coding_tables[scan_component_ac_table_index[scan_component_index]], 
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

                    int bytes_read_from_stream=bit_stream.next_data_index;

                    bytes_read_from_stream-=bit_stream.buffer_bits_filled/8;

                    int stuffed_byte_count_skipped=0;
                    for (int i=0; i<stuffed_byte_index_count; i++) {
                        if (bytes_read_from_stream>=stuffed_byte_indices[i]) {
                            stuffed_byte_count_skipped+=1;
                        }else {
                            break;
                        }
                    }
                    current_byte_position+=bytes_read_from_stream+stuffed_byte_count_skipped;

                    free(stuffed_byte_indices);
                }

                break;

            default:
                fprintf(stderr,"unhandled segment %s ( %X ) \n",Image_jpeg_segment_type_name(next_header),next_header);
                exit(-40);
        }
    }

    // -- convert idct magnitude values to channel pixel values
    // then upsample channels to final resolution

    IDCTMaskSet idct_mask_set;
    IDCTMaskSet_generate(&idct_mask_set);

    int max_vert_samples=0;
    int max_horz_samples=0;

    for (int c=0; c<3; c++) {
        if (max_vert_samples<image_components[c].vert_sample_factor) {
            max_vert_samples=image_components[c].vert_sample_factor;
        }
        if (max_horz_samples<image_components[c].horz_sample_factor) {
            max_horz_samples=image_components[c].horz_sample_factor;
        }
    }

    float* final_components[3]={NULL,NULL,NULL};
    for (int c=0; c<3; c++) {
        int downsampled_cols=image_components[c].horz_samples;
        int downsampled_rows=image_components[c].vert_samples;

        int component_total_blocks=downsampled_rows*downsampled_cols/64;

        int out_block_size=downsampled_cols*downsampled_rows;

        // -- reverse idct and quantization table application

        float* out_block_downsampled=malloc(sizeof(float)*out_block_size);

        for (int i=0; i<out_block_size; i++) {
            out_block_downsampled[i]=0.0;
        }

        for(int block_id=0;block_id<component_total_blocks;block_id++){
            int32_t* in_block=&component_data[c][block_id*64];
            float* out_block=&out_block_downsampled[block_id*64];

            for(int cosine_index = 0;cosine_index<64;cosine_index++){
                // minor optimization
                if(in_block[cosine_index] == 0) {
                    continue;
                }

                float cosine_mask_strength=in_block[cosine_index]*quant_tables[image_components[c].quant_table_specifier][cosine_index];
                
                for(int pixel_index = 0;pixel_index<64;pixel_index++){
                    float cosine_mask_pixel=idct_mask_set.idct_element_masks[cosine_index][pixel_index];
                    out_block[pixel_index]+=cosine_mask_pixel*cosine_mask_strength;
                }
            }
        }

        // -- convert blocks to row-column oriented image data

        int blocks_per_line=downsampled_cols/8;

        float* out_block_downsampled_reordered=malloc(sizeof(float)*out_block_size);
        for(int block_id = 0;block_id<component_total_blocks;block_id++){
            float* block=&out_block_downsampled[block_id*64];

            // if image divided into blocks, this is the line index of the current block
            int block_line=block_id/blocks_per_line;
            // if image divided into blocks, this is the column index of the current block
            int block_column=block_id%blocks_per_line;

            int block_base_pixel_offset=block_line*64*blocks_per_line + block_column*8;

            for(int line_in_block=0;line_in_block<8;line_in_block++){
                int out_pixel_base_index=block_base_pixel_offset + line_in_block * downsampled_cols;

                for(int col_in_block = 0;col_in_block<8;col_in_block++){
                    int pixel_index_in_block=line_in_block * 8 + col_in_block;
                    int value=block[pixel_index_in_block];

                    out_block_downsampled_reordered[out_pixel_base_index+col_in_block]=value;
                }
            }
        }
        free(out_block_downsampled);

        // -- resample block data to match final image size

        float* out_block;
        // if downsampled image has output size (because it was not actually downsampled), there is no need to upsample anything
        if(out_block_size==Y*X){
            out_block=out_block_downsampled_reordered;
        }else{
            int block_element_count=Y*X;
            out_block=malloc(sizeof(float)*block_element_count);
            
            // this works in both directions:
            //   if image component has fewer samples than max sample count, this upsamples the component
            //   if image component has more samples (because of spec A.2.4 which may require padding), the blocks used for padding are skipped
            switch (
                (max_vert_samples<<3*8)
                | (image_components[c].vert_sample_factor<<2*8)
                | (max_horz_samples<<8)
                | (image_components[c].horz_sample_factor)
            ){
                // fast path for common component sample combination (2 samples for Y, 1 component for Cb and 1 for Cr)
                case (0x02010201):
                    for(int out_row =0;out_row<(Y/2);out_row++){
                        for(int out_col =0;out_col<(X/2);out_col++){
                            float pixel=out_block_downsampled_reordered[out_row*downsampled_cols+out_col];

                            out_block[   out_row * 2       * X + out_col * 2     ]=pixel;
                            out_block[   out_row * 2       * X + out_col * 2 + 1 ]=pixel;
                            out_block[ ( out_row * 2 + 1 ) * X + out_col * 2     ]=pixel;
                            out_block[ ( out_row * 2 + 1 ) * X + out_col * 2 + 1 ]=pixel;
                        }
                    }
                    break;

                case (0x01010101):
                    for(int out_row =0;out_row<Y;out_row++){
                        for(int out_col =0;out_col<X;out_col++){
                            float pixel=out_block_downsampled_reordered[out_row*downsampled_cols+out_col];

                            out_block[ out_row * X + out_col ]=pixel;
                        }
                    }
                    break;
                    
                default:
                    for(int out_row = 0;out_row<Y;out_row++){
                        int downsampled_row=out_row*image_components[c].vert_sample_factor/max_vert_samples;
                        for(int out_col = 0;out_col<X;out_col++){
                            int downsampled_col=out_col*image_components[c].horz_sample_factor/max_horz_samples;

                            int downsampled_pixel_index=downsampled_row*downsampled_cols+downsampled_col;

                            float pixel=out_block_downsampled_reordered[downsampled_pixel_index];
                            out_block[out_row*X+out_col]=pixel;
                        }
                    }
            }

            free(out_block_downsampled_reordered);
        }
        final_components[c]=out_block;
    }

    // and convert ycbcr to rgb

    image_data->interleaved=true;
    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;

    int color_space=(image_components[0].component_id<<8*2)
        | (image_components[1].component_id<<8*1)
        | (image_components[2].component_id<<8*0);
    switch(color_space){
        case 0x010203:
            {
                int total_num_pixels_in_image=X*Y;

                image_data->data=(uint8_t*)malloc(sizeof(uint8_t)*total_num_pixels_in_image*4);

                float* y=final_components[0];
                float* cb=final_components[2];
                float* cr=final_components[1];

                float* r=final_components[0];
                float* g=final_components[2];
                float* b=final_components[1];

                // -- convert ycbcr to rgb

                for(int i = 0;i<total_num_pixels_in_image;i++){
                    float Y=y[i];
                    float Cb=cb[i];
                    float Cr=cr[i];

                    float R = Cr * 1.402 + Y;
                    float B = Cb * 1.772 + Y;
                    float G = (Y - 0.114 * B - 0.299 * R ) / 0.587;

                    r[i] = R;
                    g[i] = G;
                    b[i] = B;
                }

                // -- deinterlace and convert to uint8

                for(int i = 0;i<total_num_pixels_in_image;i++){
                    int base_index=i*4;

                    float R=r[i]+128;
                    float G=g[i]+128;
                    float B=b[i]+128;

                    image_data->data[base_index + 0] = (uint8_t)clamp_f32(0.0,255.0,R);
                    image_data->data[base_index + 1] = (uint8_t)clamp_f32(0.0,255.0,G);
                    image_data->data[base_index + 2] = (uint8_t)clamp_f32(0.0,255.0,B);
                    image_data->data[base_index + 3] = UINT8_MAX;
                }

                // -- crop to real size

                if(X!=real_X || Y!=real_Y){
                    uint8_t* real_data=malloc(real_X*real_Y*4);

                    for (int y=0; y<real_Y; y++) {
                        for(int x=0; x<real_X; x++) {
                            real_data[(y*real_X+x)*4+0]=image_data->data[(y*X+x)*4+0];
                            real_data[(y*real_X+x)*4+1]=image_data->data[(y*X+x)*4+1];
                            real_data[(y*real_X+x)*4+2]=image_data->data[(y*X+x)*4+2];
                            real_data[(y*real_X+x)*4+3]=image_data->data[(y*X+x)*4+3];
                        }
                    }

                    free(image_data->data);

                    image_data->height=real_Y;
                    image_data->width=real_X;
                    image_data->data=real_data;
                }
            }
            break;

        default:
            fprintf(stderr,"color space %X other than YCbCr (component IDs 1,2,3) currently unimplemented",color_space);
            exit(-65);
    }

    return IMAGE_PARSE_RESULT_OK;
}

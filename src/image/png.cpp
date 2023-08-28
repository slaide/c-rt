#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <thread>
#include <stdatomic.h>

#include <time.h>

#include "app/app.hpp"
#include "app/bitstream.hpp"
#include "app/error.hpp"
#include "app/huffman.hpp"
#include "app/image.hpp"

typedef huffman::CodingTable<uint16_t, bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false> DistanceTable;
typedef huffman::CodingTable<uint16_t, bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false> LiteralTable;
typedef huffman::CodingTable<uint8_t, bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false> CodeLengthTable;
typedef DistanceTable::BitStream_ BitStream;

static const uint32_t PNG_BITSTREAM_COMPRESSION_MAX_WINDOW_SIZE=32768;
static const uint32_t MAX_CHUNK_SIZE=0x8FFFFFFF;

#define CHUNK_TYPE_FROM_NAME(C0,C1,C2,C3) ((C3<<24)|(C2<<16)|(C1<<8)|(C0))
enum ChunkType{
    CHUNK_TYPE_IHDR=CHUNK_TYPE_FROM_NAME('I','H','D','R'),
    CHUNK_TYPE_IDAT=CHUNK_TYPE_FROM_NAME('I','D','A','T'),
    CHUNK_TYPE_PLTE=CHUNK_TYPE_FROM_NAME('P','L','T','E'),
    CHUNK_TYPE_IEND=CHUNK_TYPE_FROM_NAME('I','E','N','D'),

    //other common chunk types: sRGB, iCCP, cHRM, gAMA, iTXt, tEXt, zTXt, bKGD, pHYs, sBIT, hIST, tIME
};

uint32_t byteswap(uint32_t v, uint8_t num_bytes){
    union B4{
        uint8_t bytes[4];
        uint32_t v;
    };
    union B4 arg={.v=v};
    union B4 ret={.v=0};

    switch(num_bytes){
        case 4:
            ret.bytes[3]=arg.bytes[0];
            ret.bytes[2]=arg.bytes[1];
            ret.bytes[1]=arg.bytes[2];
            ret.bytes[0]=arg.bytes[3];
            break;
        case 2:
            ret.bytes[1]=arg.bytes[0];
            ret.bytes[0]=arg.bytes[1];
            break;
        default:
            fprintf(stderr,"cannot swap %d bytes\n",num_bytes);
            exit(FATAL_UNEXPECTED_ERROR);
    }
    return ret.v;
}

enum PNGColorType{
    PNG_COLOR_TYPE_GREYSCALE=0,
    PNG_COLOR_TYPE_RGB=2,
    PNG_COLOR_TYPE_PALETTE=3,
    /// greyscale + alpha
    PNG_COLOR_TYPE_GREYSCALEALPHA=4,
    PNG_COLOR_TYPE_RGBA=6,
};
const char* PNGColorType_name(uint8_t color_type){
    switch(color_type){
        case PNG_COLOR_TYPE_GREYSCALE: return "GREYSCALE";
        case PNG_COLOR_TYPE_RGB: return "RGB";
        case PNG_COLOR_TYPE_PALETTE: return "PALETTE";
        case PNG_COLOR_TYPE_GREYSCALEALPHA: return "GREYSCALEALPHA";
        case PNG_COLOR_TYPE_RGBA: return "RGBA";
        default: return NULL;
    }
}
/// specified in IHDR
enum PNGCompressionMethod{
    /// zlib/deflate format
    PNG_COMPRESSION_METHOD_ZLIB=0,
};
/// specified in zlib stream header
enum PNGCompressionMethodCode{
    PNG_COMPRESSION_METHOD_CODE_ZLIB=8,
};
enum PNGFilterMethod{
    /// the only defined set of scan filters
    /// contains per-scan filters: none, sub, up, average, paeth
    PNG_FILTER_METHOD_ADAPTIVE=0
};
struct [[gnu::packed]] IHDR{
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
};
enum PNGScanlineFilter{
    PNG_SCANLINE_FILTER_NONE=0,
    PNG_SCANLINE_FILTER_SUB=1,
    PNG_SCANLINE_FILTER_UP=2,
    PNG_SCANLINE_FILTER_AVERAGE=3,
    PNG_SCANLINE_FILTER_PAETH=4,
};
enum PNGInterlace{
    PNG_INTERLACE_NONE=0,
    PNG_INTERLACE_ADAM7=1,
};

[[maybe_unused]]
static const uint8_t DEFLATE_EXTRA_BITS[]={
    0,0,0,0, 0,0,0,0,// 257...264
    1,1,1,1,// 265...268
    2,2,2,2,// 269...272
    3,3,3,3,// 273...276
    4,4,4,4,// 277...280
    5,5,5,5,// 281...284
    0 // 285
};
[[maybe_unused]]
static const uint16_t DEFLATE_BASE_LENGTH_OFFSET[]={
    3,   4,      5,        6,      7,  8,  9,  10, // 257...264
    11,  11+2,   11+2*2,   11+2*3, // 265...268
    19,  19+4,   19+4*2,   19+4*3, // 269...272
    35,  35+8,   35+8*2,   35+8*3, // 273...276
    67,  67+16,  67+16*2,  67+16*3, // 277...280
    131, 131+32, 131+32*2, 131+32*3, // 281...284
    258 // 285
};
[[maybe_unused]]
static const uint8_t DEFLATE_BACKWARD_EXTRA_BIT[]={
    0,  0,  0,  0,
    1,  1,
    2,  2,
    3,  3,
    4,  4,
    5,  5,
    6,  6,
    7,  7,
    8,  8,
    9,  9,
    10, 10,
    11, 11,
    12, 12,
    13, 13
};
[[maybe_unused]]
static const uint16_t DEFLATE_BACKWARD_LENGTH_OFFSET[]={
    1,     2,    3,    4,
    5,     7,
    9,     13,
    17,    25,
    33,    49,
    65,    97,
    129,   193,
    257,   385,
    513,   769,
    1025,  1537,
    2049,  3073,
    4097,  6145,
    8193,  12289,
    16385, 24577
};

#define NUM_CODE_LENGTH_CODES 19
/// this table in turn is also hufmann encoded, so:
/// per spec, there are 19 code length codes (0-15 denote code length of this many bits, 16-19 are special)
/// the sequence in which the number of bits used for each code length code appear in this table is specified to the following:
static const uint8_t CODE_LENGTH_CODE_CHARACTERS[NUM_CODE_LENGTH_CODES]={16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

class PngParser{
    public:
        /// bytes per pixel
        uint32_t bpp;
        uint32_t scanline_width;

        uint8_t* output_buffer;
        uint8_t* defiltered_output_buffer;
        uint8_t* in_line;
        uint8_t* out_line;
        uint8_t* in_line_prev;
        uint8_t* out_line_prev;

        [[gnu::hot,gnu::flatten]]
        inline uint8_t raw(uint32_t index)const noexcept{
            return this->in_line[index];
        }
        [[gnu::hot,gnu::flatten]]
        inline uint8_t raw_rev(uint32_t index)const noexcept{
            return this->out_line[index];
        }
        [[gnu::hot,gnu::flatten]]
        inline uint8_t previous(uint32_t index)const noexcept{
            if(index<this->bpp)
                return 0;
            return this->in_line[index-this->bpp];
        }
        [[gnu::hot,gnu::flatten]]
        inline uint8_t previous_rev(uint32_t index)const noexcept{
            if(index<this->bpp)
                return 0;
            return this->out_line[index-this->bpp];
        }

        [[gnu::hot,gnu::flatten]]
        inline uint8_t above(uint32_t index)const noexcept{
            if(this->in_line_prev==NULL)
                return 0;
            return this->in_line_prev[index];
        }
        [[gnu::hot,gnu::flatten]]
        inline uint8_t above_rev(uint32_t index)const noexcept{
            if(this->out_line_prev==NULL)
                return 0;
            return this->out_line_prev[index];
        }

        [[gnu::hot,gnu::flatten]]
        inline uint8_t previousAbove(uint32_t index)const noexcept{
            if(this->in_line_prev==NULL)
                return 0;
            if(index<this->bpp)
                return 0;
            return this->in_line_prev[index-this->bpp];
        }
        [[gnu::hot,gnu::flatten]]
        inline uint8_t previousAbove_rev(uint32_t index)const noexcept{
            if(this->out_line_prev==NULL)
                return 0;
            if(index<this->bpp)
                return 0;
            return this->out_line_prev[index-this->bpp];
        }

        template<typename T>
        [[gnu::hot,gnu::flatten]]
        inline static T abs_sub(T a,T b)noexcept{
            T diff=a-b;
            if(diff<0)
                return -diff;
            return diff;
        }
        [[gnu::hot,gnu::flatten]]
        inline static uint16_t abs_sub(uint16_t a,uint16_t b)noexcept{
            int32_t diff=(int32_t)a-(int32_t)b;
            if(diff<0)
                return (uint16_t)-diff;
            return (uint16_t)diff;
        }

        [[gnu::hot,gnu::flatten]]
        inline uint8_t paethPredictor_rev(uint32_t index)const noexcept{
            uint16_t a = this->previous_rev(index); // left
            uint16_t b = this->above_rev(index); // above
            uint16_t c = this->previousAbove_rev(index); // upper left

            // from lodepng::paethPredictor (which i dont quite understand)
            auto pa=abs_sub(b,c);
            auto pb=abs_sub(a,c);
            auto pc=abs_sub(a+b,c+c);

            if(pb < pa){
                a=b;
                pa=pb;
            }

            auto pr_c=pc < pa;
            if(pr_c){
                return (uint8_t)c;
            } else {
                return (uint8_t)a;
            }
        }

        [[gnu::hot,gnu::flatten]]
        inline void process_scanline()noexcept{
            uint8_t scanline_filter_byte=in_line[0];
            in_line++;

            PNGScanlineFilter scanline_filter=(PNGScanlineFilter)scanline_filter_byte;
            switch(scanline_filter){
                case PNG_SCANLINE_FILTER_NONE:
                    // println("filter: none");
                    for(uint32_t index=0;index<this->scanline_width-1;index++){
                        out_line[index]=this->raw(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_SUB:
                    // println("filter: sub");
                    for(uint32_t index=0;index<this->scanline_width-1;index++){
                        out_line[index]=this->raw(index) + this->previous_rev(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_UP:
                    // println("filter: up");
                    for(uint32_t index=0;index<this->scanline_width-1;index++){
                        out_line[index]=this->raw(index) + this->above_rev(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_AVERAGE:
                    // println("filter: average");
                    for(uint32_t index=0;index<this->scanline_width-1;index++){
                        out_line[index]=this->raw(index) + (this->previous_rev(index)+this->above_rev(index))/2;
                    }
                    break;
                case PNG_SCANLINE_FILTER_PAETH:
                    // println("filter: paeth");
                    for(uint32_t index=0;index<this->scanline_width-1;index++){
                        uint8_t res=this->raw(index) + this->paethPredictor_rev(index);

                        out_line[index]=res;
                    }
                    break;
            }
        }
};

/// spec at http://www.libpng.org/pub/png/spec/1.2/PNG-Compression.html
ImageParseResult Image_read_png(
    const char* const filepath,
    ImageData* const  image_data
){
    double start_time=current_time();

    FILE* const file=fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "file '%s' not found\n",filepath);
        return IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
    }

    ImageData_initEmpty(image_data);

    discard fseek(file,0,SEEK_END);
    const long ftell_res=ftell(file);
    if(ftell_res<0){
        fprintf(stderr,"could not get file size\n");
        return IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN;
    }
    const uint64_t file_size=static_cast<uint64_t>(ftell_res);
    rewind(file);

    uint8_t* const file_contents=static_cast<uint8_t*>(aligned_alloc(64,ROUND_UP(file_size,64)));
    discard fread(file_contents, 1, file_size, file);
    
    fclose(file);

    static const uint8_t PNG_SIGNATURE[8]={ 137, 80, 78, 71, 13, 10, 26, 10};
    if(memcmp(PNG_SIGNATURE, file_contents, 8)!=0){
        fprintf(stderr,"png signature invalid\n");
        return IMAGE_PARSE_RESULT_SIGNATURE_INVALID;
    }

    struct IHDR ihdr_data;

    uint64_t byte_index=8;

    // accumulated IDAT contents
    uint64_t data_size=0;
    uint8_t *data_buffer=NULL;

    bool parsing_done=false;
    while(byte_index<file_size && !parsing_done){
        uint32_t bytes_in_chunk;
        memcpy(&bytes_in_chunk,file_contents+byte_index,4);
        bytes_in_chunk=byteswap(bytes_in_chunk,4);
        byte_index+=4;

        if(bytes_in_chunk>MAX_CHUNK_SIZE){
            fprintf(stderr,"png chunk too big. standard only allows up to 2^31 bytes\n");
            return IMAGE_PARSE_RESULT_PNG_CHUNK_SIZE_EXCEEDED;
        }

        uint32_t chunk_type;
        memcpy(&chunk_type,file_contents+byte_index,4);
        byte_index+=4;

        switch(chunk_type){
            case CHUNK_TYPE_IHDR:
                {
                    memcpy(&ihdr_data,file_contents+byte_index,13);
                    ihdr_data.width=byteswap(ihdr_data.width,4);
                    ihdr_data.height=byteswap(ihdr_data.height,4);

                    image_data->height=ihdr_data.height;
                    image_data->width=ihdr_data.width;
                    image_data->interleaved=true;

                    switch(PNGColorType(ihdr_data.color_type)){
                        case PNG_COLOR_TYPE_RGBA:
                            switch(ihdr_data.bit_depth){
                                case 8:
                                    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;
                                    break;
                                default:
                                    bail(FATAL_UNEXPECTED_ERROR,"TODO unknown bit depth %d",ihdr_data.bit_depth);
                            }
                            break;
                        case PNG_COLOR_TYPE_RGB:
                        case PNG_COLOR_TYPE_GREYSCALE:
                        case PNG_COLOR_TYPE_GREYSCALEALPHA:
                        case PNG_COLOR_TYPE_PALETTE:
                            bail(FATAL_UNEXPECTED_ERROR,"TODO pixel format %s",PNGColorType_name(ihdr_data.color_type));
                        default:
                            bail(FATAL_UNEXPECTED_ERROR,"unknown pixel format");
                    }
                }
                break;
            case CHUNK_TYPE_IDAT:
                if(!data_buffer){
                    data_buffer=(uint8_t*)malloc(bytes_in_chunk);
                }else{
                    data_buffer=(uint8_t*)realloc(data_buffer,data_size+bytes_in_chunk);
                }
                memcpy(data_buffer+data_size,file_contents+byte_index,bytes_in_chunk);

                data_size+=bytes_in_chunk;
                break;
            case CHUNK_TYPE_IEND:
                parsing_done=true;
                break;
            default:
                {
                    uint8_t chunk_name[5];
                    chunk_name[4]=0;
                    memcpy(chunk_name,&chunk_type,4);
                    bool chunk_type_significant=chunk_name[0]&0x80;
                    printf("unknown chunk type %s (%ssignificant)\n",chunk_name,chunk_type_significant?"":"not ");
                }
        }

        byte_index+=bytes_in_chunk;

        uint32_t chunk_crc;
        memcpy(&chunk_crc,file_contents+byte_index,4);
        chunk_crc=byteswap(chunk_crc,4);
        byte_index+=4;
    }

    println("done with basic file parsing after %.3fs",current_time()-start_time);

    // the data spread across the IDAT chunks is combined into a single bitstream, defined by RFC 1950 (e.g. https://datatracker.ietf.org/doc/html/rfc1950)

    BitStream _bit_stream;
    BitStream* const stream=&_bit_stream;
    BitStream::BitStream_new(stream, data_buffer, data_size);

    /// combined cm+cinfo flag across 2 bytes is used to verify data integrity
    const uint64_t cmf_flag=byteswap((uint32_t)stream->get_bits(16),2);
    if(cmf_flag%31!=0){
        fprintf(stderr,"png cmf integrity check failed: cmf is %" PRIu64 "\n",cmf_flag);
        exit(FATAL_UNEXPECTED_ERROR);
    }

    /// compression method flag
    const uint64_t cm_flag=stream->get_bits_advance(4);
    if(cm_flag!=PNG_COMPRESSION_METHOD_CODE_ZLIB){
        fprintf(stderr,"invalid png bitstream compression method\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }
    /// (encoded) compression info flag
    const uint64_t cinfo_flag=stream->get_bits_advance(4);

    const uint32_t window_size=1<<(cinfo_flag+8);
    if(window_size>PNG_BITSTREAM_COMPRESSION_MAX_WINDOW_SIZE){
        fprintf(stderr,"invalid png bitstream compression window size %d\n",window_size);
        exit(FATAL_UNEXPECTED_ERROR);
    }

    /// is set so that the cmf integrity check above can succeed
    const uint64_t fcheck_flag=stream->get_bits_advance(5);
    discard fcheck_flag;
    const uint64_t fdict_flag=stream->get_bits_advance(1);
    /// compression level (not relevant for decompression)
    const uint64_t flevel_flag=stream->get_bits_advance(2);
    discard flevel_flag;

    if(fdict_flag){
        fprintf(stderr,"TODO unimplemented: using preset dictionary\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }

    LiteralTable literal_alphabet;
    DistanceTable distance_alphabet;

    uint8_t *const output_buffer=(uint8_t*)malloc((ihdr_data.height+1)*ihdr_data.width*4);
    int out_offset=0;
    int block_id=0;
    discard block_id;

    // remaining bitstream is formatted according to RFC 1951 (deflate/zlib) (e.g. https://datatracker.ietf.org/doc/html/rfc1951)
    bool keep_parsing=true;
    while(keep_parsing){
        const uint64_t bfinal=stream->get_bits_advance(1);

        const uint64_t btype=stream->get_bits_advance(2);
        switch(btype){
            case 0:
                {
                    bail(FATAL_UNEXPECTED_ERROR,"TODO : uncompressed deflate block");
                    
                    printf("using no compression %" PRIu64 "\n",stream->buffer_bits_filled);

                    const uint8_t bits_to_next_byte_boundary=stream->buffer_bits_filled%8;
                    keep_parsing=false;
                    if(bits_to_next_byte_boundary>0)
                        stream->advance_unsafe(bits_to_next_byte_boundary);

                    /// num bytes in this block
                    const uint32_t len=byteswap((uint32_t)stream->get_bits_advance(16),2);
                    /// 1's complement of len
                    const uint32_t nlen=byteswap((uint32_t)stream->get_bits_advance(16),2);

                    if((len|nlen)!=UINT16_MAX){
                        fprintf(stderr,"uncompressed png block length integrity check failed\n");
                        exit(FATAL_UNEXPECTED_ERROR);
                    }

                    // TODO copy LEN bytes of data to output

                    printf("got %d bytes of uncompressed data\n",len);
                    stream->skip(len*8);
                }
                break;
            case 1:
                bail(FATAL_UNEXPECTED_ERROR,"TODO : compression with fixed huffman codes");
                break;
            case 2:
                {                    
                    const uint64_t num_literal_codes=257+stream->get_bits_advance(5);
                    if(num_literal_codes>286){
                        fprintf(stderr,"too many huffman codes (literals) %" PRIu64 "\n",num_literal_codes);
                        exit(FATAL_UNEXPECTED_ERROR);
                    }
                    const uint64_t num_distance_codes=1+stream->get_bits_advance(5);
                    if(num_distance_codes>30){
                        fprintf(stderr,"too many huffman codes (distance) %" PRIu64 "\n",num_distance_codes);
                        exit(FATAL_UNEXPECTED_ERROR);
                    }

                    // the number of elements in this table can be 4-19. the code length codes not present in the table are specified to not occur (i.e. zero bits)
                    const uint8_t num_huffman_codes=4+(uint8_t)stream->get_bits_advance(4);

                    // parse code sizes for each entry in the huffman code table

                    // parse all code lengths in one go
                    uint8_t code_length_codes[NUM_CODE_LENGTH_CODES];
                    memset(code_length_codes,0,sizeof(code_length_codes));
                    for(int code_size_index=0;code_size_index<num_huffman_codes;code_size_index++){
                        uint8_t new_code_length_code=(uint8_t)stream->get_bits_advance(3);
                        code_length_codes[CODE_LENGTH_CODE_CHARACTERS[code_size_index]]=new_code_length_code;
                    }

                    uint8_t values[NUM_CODE_LENGTH_CODES];
                    memset(values,0,sizeof(values));
                    for(uint8_t i=0;i<NUM_CODE_LENGTH_CODES;i++){
                        values[i]=i;
                    }

                    CodeLengthTable code_length_code_alphabet;
                    CodeLengthTable::CodingTable_new(
                        &code_length_code_alphabet, 
                        NUM_CODE_LENGTH_CODES,
                        code_length_codes, 
                        values
                    );

                    // then read literal and distance alphabet code lengths in one pass, since they use the same alphabet
                    uint8_t literal_plus_distance_code_lengths[288+33];
                    for(uint64_t i=0;i<num_literal_codes+num_distance_codes;){
                        const auto value=code_length_code_alphabet.lookup(stream);
                        switch (value) {
                            case 16:
                                {
                                    //Copy the previous code length 3 - 6 times.
                                    //The next 2 bits indicate repeat length
                                    //        (0 = 3, ... , 3 = 6)
                                    const uint64_t num_reps=3+stream->get_bits_advance(2);
                                    for(uint64_t rep=0;rep<num_reps;rep++){
                                        literal_plus_distance_code_lengths[i+rep]=literal_plus_distance_code_lengths[i-1];
                                    }
                                    i+=num_reps;
                                }
                                break;
                            case 17:
                                {
                                    //Repeat a code length of 0 for 3 - 10 times.
                                    //   (3 bits of length)
                                    const auto num_reps=3+stream->get_bits_advance(3);
                                    for(uint64_t rep=0;rep<num_reps;rep++){
                                        literal_plus_distance_code_lengths[i+rep]=0;
                                    }
                                    i+=num_reps;
                                }
                                break;
                            case 18:
                                {
                                    // Repeat a code length of 0 for 11 - 138 times
                                    //   (7 bits of length)
                                    const auto num_reps=11+stream->get_bits_advance(7);
                                    for(uint64_t rep=0;rep<num_reps;rep++){
                                        literal_plus_distance_code_lengths[i+rep]=0;
                                    }
                                    i+=num_reps;
                                }
                                break;
                            default:
                                if (value>15) {
                                    bail(FATAL_UNEXPECTED_ERROR,"unexpected value %d\n",value);
                                }
                                literal_plus_distance_code_lengths[i]=(uint8_t)value;
                                i++;
                        }
                    }

                    code_length_code_alphabet.destroy();

                    // split combined alphabet
                    uint8_t literal_code_lengths[288];
                    for(uint64_t i=0;i<num_literal_codes;i++){
                        literal_code_lengths[i]=literal_plus_distance_code_lengths[i];
                    }
                    for(uint64_t i=num_literal_codes;i<288;i++){
                        literal_code_lengths[i]=0;
                    }

                    uint8_t distance_code_lengths[33];
                    for(uint64_t i=0;i<num_distance_codes;i++){
                        distance_code_lengths[i]=literal_plus_distance_code_lengths[i+num_literal_codes];
                    }
                    for(uint64_t i=num_distance_codes;i<33;i++){
                        distance_code_lengths[i]=0;
                    }

                    // construct literal alphabet from compressed alphabet lengths
                    LiteralTable::VALUE_ literal_alphabet_values[288];
                    for(LiteralTable::VALUE_ i=0;i<288;i++)
                        literal_alphabet_values[i]=i;

                    LiteralTable::CodingTable_new(
                        &literal_alphabet, 
                        (int)num_literal_codes,
                        literal_code_lengths, 
                        literal_alphabet_values
                    );

                    // construct distance alphabet from compressed alphabet lengths
                    DistanceTable::VALUE_ distance_alphabet_values[33];
                    for(DistanceTable::VALUE_ i=0;i<33;i++)
                        distance_alphabet_values[i]=i;

                    DistanceTable::CodingTable_new(
                        &distance_alphabet, 
                        (int)num_distance_codes,
                        distance_code_lengths, 
                        distance_alphabet_values
                    );

                }

                break;
            case 3:
                bail(FATAL_UNEXPECTED_ERROR,"reserved");
                break;
            default:
                exit(FATAL_UNEXPECTED_ERROR);
        }

        bool block_done=false;
        while(!block_done){
            const auto literal_value=literal_alphabet.lookup(stream);
            if (literal_value<=255) {
                output_buffer[out_offset++]=uint8_t(literal_value);
            }else if (literal_value==256) {
                break;
            // 256 < literal_value < 286
            }else{
                const auto table_offset=literal_value-257;
                auto length=DEFLATE_BASE_LENGTH_OFFSET[table_offset];
                const auto extra_bits=DEFLATE_EXTRA_BITS[table_offset];
                if (extra_bits>0) {
                    length+=(uint16_t)stream->get_bits_advance(extra_bits);
                }
                if (length>258)
                    bail(FATAL_UNEXPECTED_ERROR,"length too large %d\n",length);

                const auto backward_distance_symbol=distance_alphabet.lookup(stream);
                const auto backward_extra_bits=DEFLATE_BACKWARD_EXTRA_BIT[backward_distance_symbol];
                auto backward_distance=DEFLATE_BACKWARD_LENGTH_OFFSET[backward_distance_symbol];
                if(backward_extra_bits>0){
                    auto backward_extra_distance=(uint16_t)stream->get_bits_advance(backward_extra_bits);
                    backward_distance+=backward_extra_distance;
                }

                for(int l=0;l<length;l++){
                    const auto base_offset=out_offset+l;
                    output_buffer[base_offset]=output_buffer[base_offset-backward_distance];
                }
                out_offset+=length;
            }
        }

        literal_alphabet.destroy();
        distance_alphabet.destroy();

        if(bfinal){
            break;
        }
        block_id++;
    }

    println("done with DEFLATE after %.3fs",current_time()-start_time);

    const uint32_t bytes_per_pixel=4;
    const uint32_t scanline_width=1+ihdr_data.width*bytes_per_pixel;
    const uint32_t num_scanlines=ihdr_data.height;

    uint8_t* const defiltered_output_buffer=(uint8_t*)malloc(ihdr_data.height*ihdr_data.width*bytes_per_pixel);

    PngParser parser;
    parser.scanline_width=scanline_width;
    parser.bpp=bytes_per_pixel;
    parser.defiltered_output_buffer=defiltered_output_buffer;
    parser.output_buffer=output_buffer;

    for(uint32_t scanline_index=0;scanline_index<num_scanlines;scanline_index++){
        if(scanline_index>0){
            parser.in_line_prev=output_buffer+(scanline_index-1)*scanline_width;
            parser.out_line_prev=defiltered_output_buffer+(scanline_index-1)*(ihdr_data.width*bytes_per_pixel);
        }else{
            parser.in_line_prev=NULL;
            parser.out_line_prev=NULL;
        }

        parser.in_line=output_buffer+scanline_index*scanline_width;
        parser.out_line=defiltered_output_buffer+scanline_index*(ihdr_data.width*bytes_per_pixel);

        parser.process_scanline();
    }

    println("done with scanline processing after %.3fs",current_time()-start_time);

    for(uint32_t pix=0;pix<ihdr_data.height*ihdr_data.width;pix++){
        const uint8_t red=defiltered_output_buffer[pix*4+0];
        const uint8_t gre=defiltered_output_buffer[pix*4+1];
        const uint8_t blu=defiltered_output_buffer[pix*4+2];
        const uint8_t alp=defiltered_output_buffer[pix*4+3];

        defiltered_output_buffer[pix*4+0]=blu;
        defiltered_output_buffer[pix*4+1]=gre;
        defiltered_output_buffer[pix*4+2]=red;
        defiltered_output_buffer[pix*4+3]=alp;
    }

    println("done with BGRA -> RGBA  after %.3fs",current_time()-start_time);

    free(file_contents);
    free(data_buffer);
    free(output_buffer);

    image_data->data=defiltered_output_buffer;

    return IMAGE_PARSE_RESULT_OK;
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef VK_USE_PLATFORM_METAL_EXT
    #include <arm_neon.h>
#elif VK_USE_PLATFORM_XCB_KHR
    #include <x86intrin.h>
#endif

#include <time.h>

#include "app/app.h"
#include "app/error.h"
#include "app/huffman.h"

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

/// spec at http://www.libpng.org/pub/png/spec/1.2/PNG-Compression.html
ImageParseResult Image_read_png(
    const char* const filepath,
    ImageData* const restrict image_data
){
    FILE* const file=fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "file '%s' not found\n",filepath);
        return IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
    }

    ImageData_initEmpty(image_data);

    discard fseek(file,0,SEEK_END);
    long ftell_res=ftell(file);
    if(ftell_res<0){
        fprintf(stderr,"could not get file size\n");
        return IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN;
    }
    const uint64_t file_size=(uint64_t)ftell_res;
    rewind(file);

    uint8_t* file_contents=aligned_alloc(64,ROUND_UP(file_size,64));
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
                    printf("image width %d height %d\n",ihdr_data.width,ihdr_data.height);
                    printf("color type %s\n",PNGColorType_name(ihdr_data.color_type));
                    printf("bit depth: %d\n",ihdr_data.bit_depth);
                }
                break;
            case CHUNK_TYPE_IDAT:
                println("TODO : implement png parser");

                if(!data_buffer){
                    data_buffer=malloc(bytes_in_chunk);
                }else{
                    data_buffer=realloc(data_buffer,data_size+bytes_in_chunk);
                }
                memcpy(data_buffer+data_size,file_contents+byte_index,bytes_in_chunk);

                data_size+=bytes_in_chunk;
                break;
            case CHUNK_TYPE_IEND:
                parsing_done=true;
                break;
            default:
                {
                    uint8_t chunk_name[5]={[4]=0};
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

    // the data spread across the IDAT chunks is combined into a single bitstream, defined by RFC 1950 (e.g. https://datatracker.ietf.org/doc/html/rfc1950)

    BitStream _bit_stream;
    BitStream* const restrict stream=&_bit_stream;
    BitStream_new(stream, data_buffer, BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false);

    /// combined cm+cinfo flag across 2 bytes is used to verify data integrity
    const uint64_t cmf_flag=byteswap((uint32_t)BitStream_get_bits(stream,16),2);
    if(cmf_flag%31!=0){
        fprintf(stderr,"png cmf integrity check failed: cmf is %"PRIu64"\n",cmf_flag);
        exit(FATAL_UNEXPECTED_ERROR);
    }

    /// compression method flag
    const uint64_t cm_flag=BitStream_get_bits_advance(stream, 4);
    if(cm_flag!=PNG_COMPRESSION_METHOD_CODE_ZLIB){
        fprintf(stderr,"invalid png bitstream compression method\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }
    /// (encoded) compression info flag
    const uint64_t cinfo_flag=BitStream_get_bits_advance(stream, 4);

    const uint32_t window_size=1<<(cinfo_flag+8);
    if(window_size>PNG_BITSTREAM_COMPRESSION_MAX_WINDOW_SIZE){
        fprintf(stderr,"invalid png bitstream compression window size %d\n",window_size);
        exit(FATAL_UNEXPECTED_ERROR);
    }

    /// is set so that the cmf integrity check above can succeed
    uint64_t fcheck_flag=BitStream_get_bits_advance(stream, 5);
    discard fcheck_flag;
    uint64_t fdict_flag=BitStream_get_bits_advance(stream, 1);
    /// compression level (not relevant for decompression)
    uint64_t flevel_flag=BitStream_get_bits_advance(stream, 2);
    discard flevel_flag;

    if(fdict_flag){
        fprintf(stderr,"TODO unimplemented: using preset dictionary\n");
        exit(FATAL_UNEXPECTED_ERROR);
    }

    // remaining bitstream is formatted according to RFC 1951 (deflate/zlib) (e.g. https://datatracker.ietf.org/doc/html/rfc1951)
    bool keep_parsing=true;
    while(keep_parsing){
        uint64_t bfinal=BitStream_get_bits_advance(stream, 1);

        uint64_t btype=BitStream_get_bits_advance(stream, 2);
        switch(btype){
            case 0:
                printf("using no compression %"PRIu64"\n",stream->buffer_bits_filled);

                uint8_t bits_to_next_byte_boundary=stream->buffer_bits_filled%8;
                keep_parsing=false;
                if(bits_to_next_byte_boundary>0)
                    BitStream_advance_unsafe(stream, bits_to_next_byte_boundary);

                /// num bytes in this block
                uint32_t len=byteswap((uint32_t)BitStream_get_bits_advance(stream, 16),2);
                /// one's complement of len
                uint32_t nlen=byteswap((uint32_t)BitStream_get_bits_advance(stream, 16),2);
                discard nlen;

                if((len|nlen)!=UINT16_MAX){
                    fprintf(stderr,"uncompressed png block length integrity check failed\n");
                    exit(FATAL_UNEXPECTED_ERROR);
                }

                // TODO copy LEN bytes of data to output

                printf("got %d bytes of uncompressed data\n",len);
                BitStream_skip(stream,len*8);
                break;
            case 1:
                printf("compression with fixed huffman codes\n");
                keep_parsing=false;
                break;
            case 2:
                printf("compression with dynamic huffman codes\n");
                
                uint64_t num_literal_codes=257+BitStream_get_bits_advance(stream, 5);
                printf("num_literal_codes %"PRIu64"\n",num_literal_codes);
                if(num_literal_codes>286){
                    fprintf(stderr,"too many huffman codes (literals) %"PRIu64"\n",num_literal_codes);
                    exit(FATAL_UNEXPECTED_ERROR);
                }
                uint64_t num_distance_codes=1+BitStream_get_bits_advance(stream, 5);
                printf("num_distance_codes %"PRIu64"\n",num_distance_codes);
                if(num_distance_codes>30){
                    fprintf(stderr,"too many huffman codes (distance) %"PRIu64"\n",num_distance_codes);
                    exit(FATAL_UNEXPECTED_ERROR);
                }

                uint64_t num_huffman_codes=4+BitStream_get_bits_advance(stream, 4);
                printf("num_huffman_codes %"PRIu64"\n",num_huffman_codes);

                // TODO parse codes

                break;
            case 3:
                printf("reserved\n"); exit(FATAL_UNEXPECTED_ERROR);
            default:
                exit(FATAL_UNEXPECTED_ERROR);
        }

        if(bfinal){
            break;
        }
    }

    // TODO parse the file
    exit(FATAL_UNEXPECTED_ERROR);

    return IMAGE_PARSE_RESULT_OK;
}

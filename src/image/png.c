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

static const uint32_t MAX_CHUNK_SIZE=0x8FFFFFFF;

#define CHUNK_TYPE_FROM_NAME(C0,C1,C2,C3) ((C3<<24)|(C2<<16)|(C1<<8)|(C0))
enum ChunkType{
    CHUNK_TYPE_IHDR=CHUNK_TYPE_FROM_NAME('I','H','D','R'),
    CHUNK_TYPE_IDAT=CHUNK_TYPE_FROM_NAME('I','D','A','T'),
    CHUNK_TYPE_PLTE=CHUNK_TYPE_FROM_NAME('P','L','T','E'),
    CHUNK_TYPE_IEND=CHUNK_TYPE_FROM_NAME('I','E','N','D'),

    //other common chunk types: sRGB, iCCP, cHRM, gAMA, iTXt, tEXt, zTXt, bKGD, pHYs, sBIT, hIST, tIME
};

uint32_t byteswap(uint32_t v){
    union B4{
        uint8_t bytes[4];
        uint32_t v;
    };
    union B4 arg={.v=v};
    union B4 ret={.v=0};

    ret.bytes[3]=arg.bytes[0];
    ret.bytes[2]=arg.bytes[1];
    ret.bytes[1]=arg.bytes[2];
    ret.bytes[0]=arg.bytes[3];
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
enum PNGCompressionMethod{
    /// zlib/deflate format
    PNG_COMPRESSION_METHOD_ZLIB=0,
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
        bytes_in_chunk=byteswap(bytes_in_chunk);
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
                    ihdr_data.width=byteswap(ihdr_data.width);
                    ihdr_data.height=byteswap(ihdr_data.height);
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
        chunk_crc=byteswap(chunk_crc);
        byte_index+=4;
    }

    BitStream _bit_stream;
    BitStream* const restrict stream=&_bit_stream;
    BitStreamRtL_new(stream, data_buffer);

    // TODO parse the file
    exit(FATAL_UNEXPECTED_ERROR);

    return IMAGE_PARSE_RESULT_OK;
}

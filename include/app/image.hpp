#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "app/macros.hpp"
#include "app/error.hpp"
#include "app/bitstream.hpp"

typedef enum PixelFormat{
    PIXEL_FORMAT_Ru8Gu8Bu8Au8
}PixelFormat;
struct ImageFileMetadata{
    char* file_comment;
};

typedef struct ImageData{
    uint8_t* data;

    uint32_t height;
    uint32_t width;

    PixelFormat pixel_format;
    bool interleaved;

    struct ImageFileMetadata image_file_metadata;
}ImageData;

/// initialise all fields to their zero-equivalent
void ImageData_initEmpty(struct ImageData* const image_data);
void ImageData_destroy(struct ImageData* const image_data);

typedef enum ImageParseResult{
    IMAGE_PARSE_RESULT_OK,
    IMAGE_PARSE_RESULT_FILE_NOT_FOUND,

    IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN,
    
    IMAGE_PARSE_RESULT_SIGNATURE_INVALID,

    IMAGE_PARSE_RESULT_PNG_CHUNK_SIZE_EXCEEDED,
}ImageParseResult;

class FileParser{
    public:
        uint64_t file_size;
        uint8_t* file_contents;

        uint64_t current_file_content_index;

        ImageData* const image_data;

    FileParser(
        const char* file_path,
        ImageData* image_data
    ):image_data(image_data){
        FILE* const file=fopen(file_path, "rb");
        if (!file) {
            fprintf(stderr, "file '%s' not found\n",file_path);
            throw IMAGE_PARSE_RESULT_FILE_NOT_FOUND;
        }

        ImageData_initEmpty(this->image_data);

        discard fseek(file,0,SEEK_END);
        const long ftell_res=ftell(file);
        if(ftell_res<0){
            fprintf(stderr,"could not get file size\n");
            throw IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN;
        }
        this->file_size=static_cast<uint64_t>(ftell_res);
        rewind(file);

        this->file_contents=static_cast<uint8_t*>(aligned_alloc(64,ROUND_UP(this->file_size,64)));
        discard fread(this->file_contents, 1, this->file_size, file);
        
        fclose(file);

        this->current_file_content_index=0;
    }

    enum TestSignatureResult{
        TEST_SIGNATURE_FAILURE=0,
        TEST_SIGNATURE_SUCCESS=1,
    };

    enum TestSignatureResult test_signature(const uint8_t* signature,const uint64_t signature_len){
        if(memcmp(signature, this->file_contents+this->current_file_content_index, signature_len)!=0){
            return TEST_SIGNATURE_FAILURE;
        }
        return TEST_SIGNATURE_SUCCESS;
    }

    template<bool ADVANCE=true>
    void expect_signature(const uint8_t* signature,const uint64_t signature_len){
        if(this->test_signature(signature, signature_len)==TEST_SIGNATURE_FAILURE){
            throw IMAGE_PARSE_RESULT_SIGNATURE_INVALID;
        }
        if constexpr(ADVANCE)
            this->current_file_content_index+=signature_len;
    }

    template<typename T,bool ADVANCE=true>
    T get_mem(){
        T ret;
        memcpy(&ret,this->file_contents+this->current_file_content_index,sizeof(T));
        if constexpr(ADVANCE)
            this->current_file_content_index+=sizeof(T);
        return ret;
    }

    uint8_t* data_ptr()const noexcept{
        return this->file_contents+this->current_file_content_index;
    }
};

ImageParseResult Image_read_jpeg(const char* filepath,ImageData* image_data);

ImageParseResult Image_read_png(const char* const filepath,ImageData* const image_data);

ImageParseResult Image_read_webp(const char* const filepath,ImageData* const  image_data);

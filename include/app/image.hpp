#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "app/macros.hpp"
#include "app/error.hpp"
#include "app/bitstream.hpp"

#include <vulkan/vulkan.h>

typedef enum PixelFormat{
    PIXEL_FORMAT_Ru8Gu8Bu8Au8
}PixelFormat;
struct ImageFileMetadata{
    char* file_comment;
};

class ImageData{
    public:

        uint8_t* data;

        uint32_t height;
        uint32_t width;

        PixelFormat pixel_format;
        bool interleaved;

        struct ImageFileMetadata image_file_metadata;

        /// initialise all fields to their zero-equivalent
        static void initEmpty(ImageData* const image_data);
        static void destroy(ImageData* const image_data);

        VkFormat vk_img_format()const noexcept;
};

typedef enum ImageParseResult{
    IMAGE_PARSE_RESULT_OK,
    IMAGE_PARSE_RESULT_FILE_NOT_FOUND,

    IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN,
    
    IMAGE_PARSE_RESULT_SIGNATURE_INVALID,

    IMAGE_PARSE_RESULT_PNG_CHUNK_SIZE_EXCEEDED,
}ImageParseResult;

class FileParser{
    public:
        std::size_t file_size;
        uint8_t* file_contents;

        std::size_t current_file_content_index;

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

        ImageData::initEmpty(this->image_data);

        discard fseek(file,0,SEEK_END);
        const long ftell_res=ftell(file);
        if(ftell_res<0){
            fprintf(stderr,"could not get file size\n");
            throw IMAGE_PARSE_RESULT_FILESIZE_UNKNOWN;
        }
        this->file_size=static_cast<std::size_t>(ftell_res);
        rewind(file);

        this->file_contents=new uint8_t[this->file_size];
        discard fread(this->file_contents, 1, this->file_size, file);
        
        fclose(file);

        this->current_file_content_index=0;
    }

    uint8_t* data_ptr()const noexcept{
        return this->file_contents+this->current_file_content_index;
    }

    enum TestSignatureResult{
        TEST_SIGNATURE_FAILURE=0,
        TEST_SIGNATURE_SUCCESS=1,
    };

    enum TestSignatureResult test_signature(const uint8_t* signature,const uint64_t signature_len)const{
        if(memcmp(signature, this->data_ptr(), signature_len)!=0){
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
        memcpy(&ret,this->data_ptr(),sizeof(T));
        if constexpr(ADVANCE)
            this->current_file_content_index+=sizeof(T);
        return ret;
    }
};

ImageParseResult Image_read_jpeg(
    const char* const filepath,
    ImageData* const image_data
);
ImageParseResult Image_read_png(
    const char* const filepath,
    ImageData* const image_data
);

ImageParseResult Image_read_gif(
    const char* const filepath,
    ImageData* const  image_data
);


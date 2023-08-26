#pragma once

#include <stdint.h>

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

ImageParseResult Image_read_jpeg(const char* filepath,ImageData* image_data);

ImageParseResult Image_read_png(const char* const filepath,ImageData* const image_data);

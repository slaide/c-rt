#include "app/app.hpp"
#include "app/bitstream.hpp"
#include "app/error.hpp"
#include "app/image.hpp"
#include <cstdio>

enum class FrameType:uint8_t{
    KeyFrame=0,
    IntraFrame=1,
};
const char* FrameType_name(FrameType frame_type){
    switch(frame_type){
        case FrameType::IntraFrame: return "IntraFrame";
        case FrameType::KeyFrame: return "KeyFrame";
        default:
            return NULL;
    }
}
enum class ScalingFactor:uint8_t{
    /// no upscaling
    None,
    /// upscaled by factor 5/4 (1.25)
    Upscale5o4,
    /// upscaled by factor 5/3 (1.666..)
    Upscale5o3,
    /// upscaled by factor 2
    Upscale2,
};

/// webp header spec at https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
ImageParseResult Image_read_webp(
    const char* const filepath,
    ImageData* const  image_data
){
    FileParser parser{filepath,image_data};

    parser.expect_signature((const uint8_t*)"RIFF", 4);
    const uint32_t file_size=parser.get_mem<uint32_t>();
    println("file is %d bytes",file_size);
    parser.expect_signature((const uint8_t*)"WEBP", 4);

    // VP8 spec at https://datatracker.ietf.org/doc/html/rfc6386
    if(parser.test_signature((const uint8_t*)"VP8 ", 4)==FileParser::TEST_SIGNATURE_SUCCESS){
        println("VP8 content (lossy)");
    }
    // VP8L spec at https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification
    if(parser.test_signature((const uint8_t*)"VP8L", 4)==FileParser::TEST_SIGNATURE_SUCCESS){
        println("VP8L content (lossless)");
    }
    parser.current_file_content_index+=4;
    uint32_t chunk_size=parser.get_mem<uint32_t>();
    println("chunk_size %d",chunk_size);

    typedef bitStream::BitStream<bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false> BitStream;
    BitStream stream;
    BitStream::BitStream_new(&stream, parser.data_ptr(),parser.file_size-parser.current_file_content_index);

    // -- vp8 spec section 9.1

        FrameType frame_type=FrameType(stream.get_bits_advance(1));
        println("got frame type %s",FrameType_name(frame_type));
        auto vp8_version=(uint8_t)stream.get_bits_advance(3);
        println("vp8 version %d",vp8_version);
        auto frame_should_be_displayed=(uint8_t)stream.get_bits_advance(1);
        println("frame should be displayed? %d",frame_should_be_displayed);
        auto first_data_partition_size=stream.get_bits_advance(19);
        println("first_data_partition_size %" PRIu64, first_data_partition_size);

        if(frame_type==FrameType::KeyFrame){
            uint64_t start_code_byte[3]={stream.get_bits_advance(8),stream.get_bits_advance(8),stream.get_bits_advance(8)};
            if(start_code_byte[0]!=0x9d) bail(FATAL_UNEXPECTED_ERROR,"start_code_byte[0]!=0x9d (is %X)",(uint32_t)start_code_byte[0]);
            if(start_code_byte[1]!=0x01) bail(FATAL_UNEXPECTED_ERROR,"start_code_byte[1]!=0x01 (is %X)",(uint32_t)start_code_byte[1]);
            if(start_code_byte[2]!=0x2a) bail(FATAL_UNEXPECTED_ERROR,"start_code_byte[2]!=0x2a (is %X)",(uint32_t)start_code_byte[2]);

            auto horizontal_information=stream.get_bits_advance(16);
            auto vertical_information=stream.get_bits_advance(16);

            ScalingFactor horizontal_scale=static_cast<ScalingFactor>(horizontal_information>>14);
            ScalingFactor vertical_scale=static_cast<ScalingFactor>(vertical_information>>14);

            auto horizontal_size=horizontal_information&0x3FFF;
            auto vertical_size=vertical_information&0x3FFF;

            println("size: horz %d vert %d",(uint32_t)horizontal_size,(uint32_t)vertical_size);
            println("scale: horz %d vert %d",(uint32_t)horizontal_scale,(uint32_t)vertical_scale);
        }

    bail(FATAL_UNEXPECTED_ERROR,"TODO");

    return IMAGE_PARSE_RESULT_OK;
}

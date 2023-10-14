#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <thread>
#include <vector>
#include <ranges>
#include <numeric>
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
using BitStream = DistanceTable::BitStream_;

static constexpr uint32_t PNG_BITSTREAM_COMPRESSION_MAX_WINDOW_SIZE=32768;
static constexpr uint32_t MAX_CHUNK_SIZE=0x8FFFFFFF;

#define CHUNK_TYPE_FROM_NAME(C0,C1,C2,C3) ((C3<<24)|(C2<<16)|(C1<<8)|(C0))
enum ChunkType{
    CHUNK_TYPE_IHDR=CHUNK_TYPE_FROM_NAME('I','H','D','R'),
    CHUNK_TYPE_IDAT=CHUNK_TYPE_FROM_NAME('I','D','A','T'),
    CHUNK_TYPE_PLTE=CHUNK_TYPE_FROM_NAME('P','L','T','E'),
    CHUNK_TYPE_IEND=CHUNK_TYPE_FROM_NAME('I','E','N','D'),

    //other common chunk types: sRGB, iCCP, cHRM, gAMA, iTXt, tEXt, zTXt, bKGD, pHYs, sBIT, hIST, tIME
};

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
static constexpr uint8_t DEFLATE_EXTRA_BITS[]={
    0,0,0,0, 0,0,0,0,// 257...264
    1,1,1,1,// 265...268
    2,2,2,2,// 269...272
    3,3,3,3,// 273...276
    4,4,4,4,// 277...280
    5,5,5,5,// 281...284
    0 // 285
};
[[maybe_unused]]
static constexpr uint16_t DEFLATE_BASE_LENGTH_OFFSET[]={
    3,   4,      5,        6,      7,  8,  9,  10, // 257...264
    11,  11+2,   11+2*2,   11+2*3, // 265...268
    19,  19+4,   19+4*2,   19+4*3, // 269...272
    35,  35+8,   35+8*2,   35+8*3, // 273...276
    67,  67+16,  67+16*2,  67+16*3, // 277...280
    131, 131+32, 131+32*2, 131+32*3, // 281...284
    258 // 285
};
[[maybe_unused]]
static constexpr uint8_t DEFLATE_BACKWARD_EXTRA_BIT[]={
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
static constexpr uint16_t DEFLATE_BACKWARD_LENGTH_OFFSET[]={
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

static constexpr std::size_t NUM_CODE_LENGTH_CODES=19;
/// this table in turn is also hufmann encoded, so:
/// per spec, there are 19 code length codes (0-15 denote code length of this many bits, 16-19 are special)
/// the sequence in which the number of bits used for each code length code appear in this table is specified to the following:
static constexpr uint8_t CODE_LENGTH_CODE_CHARACTERS[NUM_CODE_LENGTH_CODES]={16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/// decode zlib-compressed data
///
/// specified in RFC 1950 (e.g. https://datatracker.ietf.org/doc/html/rfc1950)
class ZLIBDecoder{
    public:
        uint8_t* input_buffer;
        uint64_t input_buffer_size;
        uint8_t* output_buffer;
        uint64_t output_buffer_size;

        ZLIBDecoder(
            uint64_t input_buffer_size,
            uint8_t* input_buffer,
            uint64_t output_buffer_size,
            uint8_t* output_buffer
        ):
            input_buffer(input_buffer),
            input_buffer_size(input_buffer_size),
            output_buffer(output_buffer),
            output_buffer_size(output_buffer_size)
        {}

        void decode(){
            BitStream _stream;
            BitStream* stream=&_stream;
            BitStream::BitStream_new(stream,input_buffer,input_buffer_size);

            /// combined cm+cinfo flag across 2 bytes is used to verify data integrity
            const uint64_t cmf_flag=bitUtil::byteswap((uint32_t)stream->get_bits(16),2);
            if(cmf_flag%31!=0){
                fprintf(stderr,"png cmf integrity check failed: cmf is %" PRIu64 "\n",cmf_flag);
                exit(FATAL_UNEXPECTED_ERROR);
            }

            /// compression method flag
            const uint64_t cm_flag=stream->get_bits_advance(4);
            if(cm_flag!=PNG_COMPRESSION_METHOD_CODE_ZLIB){
                bail(FATAL_UNEXPECTED_ERROR,"invalid png bitstream compression method %d",(int)cm_flag);
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

            if(fdict_flag)
                bail(FATAL_UNEXPECTED_ERROR,"TODO unimplemented: using preset dictionary");

            int out_offset=0;
            int block_id=0;
            discard block_id;

            // remaining bitstream is formatted according to RFC 1951 (deflate) (e.g. https://datatracker.ietf.org/doc/html/rfc1951)
            bool keep_parsing=true;
            while(keep_parsing){
                LiteralTable literal_alphabet;
                DistanceTable distance_alphabet;

                const auto bfinal=stream->get_bits_advance<unsigned>(1);

                const auto btype=stream->get_bits_advance<unsigned>(2);
                switch(btype){
                    // uncompressed block
                    case 0:
                        {
                            bail(FATAL_UNEXPECTED_ERROR,"TODO : uncompressed deflate block");
                            
                            printf("using no compression %" PRIu64 "\n",stream->buffer_bits_filled);

                            const uint8_t bits_to_next_byte_boundary=stream->buffer_bits_filled%8;
                            keep_parsing=false;
                            if(bits_to_next_byte_boundary>0)
                                stream->advance_unsafe(bits_to_next_byte_boundary);

                            /// num bytes in this block
                            const uint32_t len=bitUtil::byteswap((uint32_t)stream->get_bits_advance(16),2);
                            /// 1's complement of len
                            const uint32_t nlen=bitUtil::byteswap((uint32_t)stream->get_bits_advance(16),2);

                            if((len+nlen)!=UINT16_MAX)
                                bail(FATAL_UNEXPECTED_ERROR,"uncompressed png block length integrity check failed\n");

                            // TODO copy LEN bytes of data to output

                            printf("got %d bytes of uncompressed data\n",len);
                            stream->skip(len*8);
                        }
                        break;
                    // compressed using fixed huffman codes
                    case 1:
                        {
                            // construct predefined huffman trees
                            {
                                constexpr std::size_t num_entries=288;

                                std::array<int,num_entries> code_lengths{};
                                std::fill(code_lengths.data()+0,code_lengths.data()+144,8);
                                std::fill(code_lengths.data()+144,code_lengths.data()+256,9);
                                std::fill(code_lengths.data()+256,code_lengths.data()+280,7);
                                std::fill(code_lengths.data()+280,code_lengths.data()+288,8);

                                std::array<uint16_t,num_entries> values{};
                                std::iota(values.begin(),values.end(),0);

                                LiteralTable::CodingTable_new(
                                    literal_alphabet, 
                                    code_lengths.size(),
                                    code_lengths.data(), 
                                    values.data()
                                );
                            }

                            {
                                constexpr std::size_t num_entries=32;

                                std::array<int,num_entries> code_lengths{};
                                std::fill(code_lengths.begin(),code_lengths.end(),5);
                                std::array<uint16_t,num_entries> values{};
                                std::iota(values.begin(),values.end(),0);

                                LiteralTable::CodingTable_new(
                                    distance_alphabet, 
                                    code_lengths.size(),
                                    code_lengths.data(), 
                                    values.data()
                                );
                            }
                        }
                        break;
                    // compressed using dynamic huffman codes
                    case 2:
                        {
                            const std::size_t num_literal_codes=257+stream->get_bits_advance(5);
                            if(num_literal_codes>286)
                                bail(FATAL_UNEXPECTED_ERROR,"too many huffman codes (literals) %" PRIu64 "\n",num_literal_codes);

                            const std::size_t num_distance_codes=1+stream->get_bits_advance(5);
                            if(num_distance_codes>30)
                                bail(FATAL_UNEXPECTED_ERROR,"too many huffman codes (distance) %" PRIu64 "\n",num_distance_codes);

                            // the number of elements in this table can be 4-19. the code length codes not present in the table are specified to not occur (i.e. zero bits)
                            const auto num_huffman_codes=4+stream->get_bits_advance<std::size_t>(4);

                            // parse code sizes for each entry in the huffman code table

                            // parse all code lengths in one go
                            std::array<int,NUM_CODE_LENGTH_CODES> code_length_codes{};
                            for(std::size_t code_size_index : std::views::iota(0u,num_huffman_codes)){
                                int new_code_length_code=stream->get_bits_advance<uint8_t>(3);
                                code_length_codes[CODE_LENGTH_CODE_CHARACTERS[code_size_index]]=new_code_length_code;
                            }

                            /// used as huffman table values
                            std::array<uint8_t,NUM_CODE_LENGTH_CODES> values{};
                            std::iota(values.begin(), values.end(), 0);

                            CodeLengthTable code_length_code_alphabet;
                            CodeLengthTable::CodingTable_new(
                                code_length_code_alphabet, 
                                NUM_CODE_LENGTH_CODES,
                                code_length_codes.data(), 
                                values.data()
                            );

                            // then read literal and distance alphabet code lengths in one pass, since they use the same alphabet
                            std::array<int,288+33> literal_plus_distance_code_lengths{};
                            for(std::size_t i=0;i<num_literal_codes+num_distance_codes;){
                                const auto value=code_length_code_alphabet.lookup(stream);
                                switch (value) {
                                    case 16:
                                        {
                                            //Copy the previous code length 3 - 6 times.
                                            //The next 2 bits indicate repeat length
                                            //        (0 = 3, ... , 3 = 6)
                                            const auto num_reps=3+stream->get_bits_advance<std::size_t>(2);

                                            const auto code_to_copy=literal_plus_distance_code_lengths[i-1];
                                            std::fill(literal_plus_distance_code_lengths.data()+i,literal_plus_distance_code_lengths.data()+i+num_reps,code_to_copy);

                                            i+=num_reps;
                                        }
                                        break;
                                    case 17:
                                        {
                                            //Repeat a code length of 0 for 3 - 10 times.
                                            //   (3 bits of length)
                                            const auto num_reps=3+stream->get_bits_advance<std::size_t>(3);
                                            std::fill(literal_plus_distance_code_lengths.data()+i,literal_plus_distance_code_lengths.data()+i+num_reps,0);
                                            i+=num_reps;
                                        }
                                        break;
                                    case 18:
                                        {
                                            // Repeat a code length of 0 for 11 - 138 times
                                            //   (7 bits of length)
                                            const auto num_reps=11+stream->get_bits_advance<std::size_t>(7);
                                            std::fill(literal_plus_distance_code_lengths.data()+i,literal_plus_distance_code_lengths.data()+i+num_reps,0);
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
                            std::array<int,288> literal_code_lengths{};
                            std::copy(
                                literal_plus_distance_code_lengths.data(),
                                literal_plus_distance_code_lengths.data()+num_literal_codes,
                                literal_code_lengths.data()
                            );

                            std::array<int,33> distance_code_lengths{};
                            std::copy(
                                literal_plus_distance_code_lengths.data()+num_literal_codes,
                                literal_plus_distance_code_lengths.data()+num_literal_codes+num_distance_codes,
                                distance_code_lengths.data()
                            );

                            // construct literal alphabet from compressed alphabet lengths
                            std::array<LiteralTable::VALUE_,288> literal_alphabet_values{};
                            std::iota(literal_alphabet_values.begin(),literal_alphabet_values.end(),0);

                            LiteralTable::CodingTable_new(
                                literal_alphabet, 
                                num_literal_codes,
                                literal_code_lengths.data(), 
                                literal_alphabet_values.data()
                            );

                            // construct distance alphabet from compressed alphabet lengths
                            std::array<DistanceTable::VALUE_,33> distance_alphabet_values{};
                            std::iota(distance_alphabet_values.begin(),distance_alphabet_values.end(),0);

                            DistanceTable::CodingTable_new(
                                distance_alphabet, 
                                num_distance_codes,
                                distance_code_lengths.data(), 
                                distance_alphabet_values.data()
                            );
                        }

                        break;
                    case 3:
                        bail(FATAL_UNEXPECTED_ERROR,"reserved");
                        break;
                    default:
                        exit(FATAL_UNEXPECTED_ERROR);
                }

                for(;;){
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
                        if (extra_bits>0)
                            length+=stream->get_bits_advance<uint16_t>(extra_bits);

                        if (length>258)
                            bail(FATAL_UNEXPECTED_ERROR,"invalid length %d>258\n",length);

                        const auto backward_distance_symbol=distance_alphabet.lookup(stream);
                        const auto backward_extra_bits=DEFLATE_BACKWARD_EXTRA_BIT[backward_distance_symbol];
                        auto backward_distance=DEFLATE_BACKWARD_LENGTH_OFFSET[backward_distance_symbol];
                        if(backward_extra_bits>0){
                            auto backward_extra_distance=stream->get_bits_advance<uint16_t>(backward_extra_bits);
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
        }
};

class PngParser:public FileParser{
    public:
        struct IHDR ihdr_data;

        /// bytes per pixel
        uint32_t bpp=0;
        uint32_t scanline_width=0;

        uint8_t* output_buffer=nullptr;
        uint8_t* defiltered_output_buffer=nullptr;
        uint8_t* in_line=nullptr;
        uint8_t* out_line=nullptr;
        uint8_t* in_line_prev=nullptr;
        uint8_t* out_line_prev=nullptr;

        PngParser(const char* file_path,ImageData*const image_data):FileParser(file_path, image_data){}

        void destroy(){
            delete[] this->file_contents;
            delete[] this->output_buffer;
        }

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
            const uint8_t scanline_filter_byte=this->in_line[0];
            this->in_line++;

            PNGScanlineFilter scanline_filter=(PNGScanlineFilter)scanline_filter_byte;
            switch(scanline_filter){
                case PNG_SCANLINE_FILTER_NONE:
                    // println("filter: none");
                    for(uint32_t index : std::views::iota(0u,this->scanline_width-1)){
                        this->out_line[index]=this->raw(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_SUB:
                    // println("filter: sub");
                    for(uint32_t index : std::views::iota(0u,this->scanline_width-1)){
                        this->out_line[index]=this->raw(index) + this->previous_rev(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_UP:
                    // println("filter: up");
                    for(uint32_t index : std::views::iota(0u,this->scanline_width-1)){
                        this->out_line[index]=this->raw(index) + this->above_rev(index);
                    }
                    break;
                case PNG_SCANLINE_FILTER_AVERAGE:
                    // println("filter: average");
                    for(uint32_t index : std::views::iota(0u,this->scanline_width-1)){
                        this->out_line[index]=this->raw(index) + static_cast<uint8_t>((this->previous_rev(index)+this->above_rev(index))/2);
                    }
                    break;
                case PNG_SCANLINE_FILTER_PAETH:
                    // println("filter: paeth");
                    for(uint32_t index : std::views::iota(0u,this->scanline_width-1)){
                        uint8_t res=this->raw(index) + this->paethPredictor_rev(index);

                        this->out_line[index]=res;
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

    PngParser parser{filepath,image_data};

    const char* PNG_SIGNATURE="\x89PNG\r\n\x1a\n";
    parser.expect_signature((const uint8_t*)(PNG_SIGNATURE), 8);

    // accumulated IDAT contents
    std::size_t data_size=0;
    std::vector<uint8_t> data_buffer;

    bool parsing_done=false;
    while(parser.current_file_content_index<parser.file_size && !parsing_done){
        uint32_t bytes_in_chunk=bitUtil::byteswap(parser.get_mem<uint32_t>(),4);

        if(bytes_in_chunk>MAX_CHUNK_SIZE){
            fprintf(stderr,"png chunk too big. standard only allows up to 2^31 bytes\n");
            return IMAGE_PARSE_RESULT_PNG_CHUNK_SIZE_EXCEEDED;
        }

        uint32_t chunk_type=parser.get_mem<uint32_t>();

        switch(chunk_type){
            case CHUNK_TYPE_IHDR:
                {
                    parser.ihdr_data=parser.get_mem<struct IHDR,false>();
                    parser.ihdr_data.width=bitUtil::byteswap(parser.ihdr_data.width,4);
                    parser.ihdr_data.height=bitUtil::byteswap(parser.ihdr_data.height,4);

                    image_data->height=parser.ihdr_data.height;
                    image_data->width=parser.ihdr_data.width;
                    image_data->interleaved=true;

                    switch(PNGColorType(parser.ihdr_data.color_type)){
                        case PNG_COLOR_TYPE_RGBA:
                            switch(parser.ihdr_data.bit_depth){
                                case 8:
                                    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;
                                    break;
                                default:
                                    bail(FATAL_UNEXPECTED_ERROR,"TODO unknown bit depth %d",parser.ihdr_data.bit_depth);
                            }
                            break;
                        case PNG_COLOR_TYPE_RGB:
                        case PNG_COLOR_TYPE_GREYSCALE:
                        case PNG_COLOR_TYPE_GREYSCALEALPHA:
                        case PNG_COLOR_TYPE_PALETTE:
                            bail(FATAL_UNEXPECTED_ERROR,"TODO pixel format %s",PNGColorType_name(parser.ihdr_data.color_type));
                        default:
                            bail(FATAL_UNEXPECTED_ERROR,"unknown pixel format");
                    }
                }
                break;
            case CHUNK_TYPE_IDAT:
                data_buffer.resize(data_buffer.size()+bytes_in_chunk);

                std::copy(
                    parser.data_ptr(),
                    parser.data_ptr()+bytes_in_chunk,
                    data_buffer.data()+data_size
                );
                data_size+=bytes_in_chunk;

                break;
            case CHUNK_TYPE_IEND:
                parsing_done=true;
                break;
            default:
                {
                    std::array<uint8_t,5> chunk_name{};
                    memcpy(chunk_name.data(),&chunk_type,4);
                    bool chunk_type_significant=chunk_name[0]&0x80;
                    printf("unknown chunk type %s (%ssignificant)\n",chunk_name.data(),chunk_type_significant?"":"not ");
                }
        }
        parser.current_file_content_index+=bytes_in_chunk;

        uint32_t chunk_crc=bitUtil::byteswap(parser.get_mem<uint32_t>(),4);
        discard chunk_crc;
    }

    println("done with basic file parsing after %.3fms",(current_time()-start_time)*1000);

    std::size_t output_buffer_size=(parser.ihdr_data.height+1)*parser.ihdr_data.width*4;
    uint8_t* const output_buffer=new uint8_t[output_buffer_size];

    // the data spread across the IDAT chunks is combined into a single bitstream, defined by RFC 1950 (e.g. https://datatracker.ietf.org/doc/html/rfc1950)

    ZLIBDecoder zlib_decoder{
        data_buffer.size(),
        data_buffer.data(),
        output_buffer_size,
        output_buffer
    };
    zlib_decoder.decode();

    println("done with DEFLATE after %.3fms",(current_time()-start_time)*1000);

    const uint32_t bytes_per_pixel=4;
    const uint32_t scanline_width=1+parser.ihdr_data.width*bytes_per_pixel;
    const uint32_t num_scanlines=parser.ihdr_data.height;

    uint8_t* const defiltered_output_buffer=new uint8_t[parser.ihdr_data.height*parser.ihdr_data.width*bytes_per_pixel];

    parser.scanline_width=scanline_width;
    parser.bpp=bytes_per_pixel;
    parser.defiltered_output_buffer=defiltered_output_buffer;
    parser.output_buffer=output_buffer;

    parser.in_line_prev=NULL;
    parser.out_line_prev=NULL;

    for(uint32_t scanline_index : std::views::iota(0u,num_scanlines)){
        parser.in_line =&output_buffer[scanline_index*scanline_width];
        parser.out_line=&defiltered_output_buffer[scanline_index*parser.ihdr_data.width*bytes_per_pixel];

        parser.process_scanline();

        parser.in_line_prev=parser.in_line;
        parser.out_line_prev=parser.out_line;
    }

    println("done with scanline processing after %.3fms",(current_time()-start_time)*1000);

    parser.destroy();

    image_data->data=defiltered_output_buffer;

    return IMAGE_PARSE_RESULT_OK;
}

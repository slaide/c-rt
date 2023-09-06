#include <cstdint>
#include <cstring>

#include "app/bitstream.hpp"
#include "app/macros.hpp"
#include "app/error.hpp"
#include "app/image.hpp"

enum class GIFVersion{
    V87a,
    V89a,
};

enum class GIFBlockSignature:uint8_t{
    /// section 20
    ImageDescriptor=0x2c,
    /// see GIFBlockLabel
    Extension=0x21,
    /// this block indicates the end of the GIF data stream
    Trailer=0x3b,
};
/// specifically for extensions
enum class GIFBlockLabel:uint8_t{
    /// section 23
    GraphicControl=0xf9,
    /// section 25
    PlainText=0x01,
    /// section 26
    Application=0xff,
};
/// Indicates the way in which the graphic is to be treated after being displayed.
enum class GIFDisposalMethod{
    /// No disposal specified. The decoder is not required to take any action.
    NoDisposal=0,
    /// Do not dispose. The graphic is to be left in place.
    DoNotDispose=1,
    /// Restore to background color. The area used by the graphic must be restored to the background color.
    RestoreBackground=2,
    /// Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.
    RestoreToPrevious=3,
    /// To be defined.
    RESERVED4=4,
    /// To be defined.
    RESERVED5=5,
    /// To be defined.
    RESERVED6=6,
    /// To be defined.
    RESERVED7=7,
};

/// terminating byte expected at the end of certain data blocks
const uint8_t BLOCK_TERMINATOR[1]={0};

class [[gnu::packed]] GraphicControlPackedField{
    public:
        uint8_t _reserved:3;
        uint8_t disposal_method:3;
        uint8_t user_input_flag:1;
        uint8_t transparent_color_flag:1;
};
class [[gnu::packed]] ImageDescriptorPackedField{
    public:
        uint8_t local_color_table_flag:1;
        uint8_t interlace_flag:1;
        uint8_t sort_flag:1;
        uint8_t _reserved:2;
        uint8_t size_of_local_color_table:3;
};

class ExtensionInformation{
    public:
        class NetscapeLoopingExtension{
            public:
                bool present=false;
                uint16_t num_loops;
        }netscape_looping_extension;
};

template<bitStream::Direction = bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT>
class BitStreamWriter{
    public:
        /// in bits
        uint64_t data_len;
        uint8_t* data;

        uint64_t data_buffer_len;
        uint64_t data_buffer;
        BitStreamWriter(){
            this->data_len=0;
            this->data=nullptr;
            this->data_buffer_len=0;
            this->data_buffer=0;
        }

        template<bool EXACT=false>
        void flush_1_byte(){
            this->data=(uint8_t*)realloc(this->data, this->data_len/8+1);
            this->data[this->data_len/8]=static_cast<uint8_t>(this->data_buffer&0xFF);
            if constexpr(EXACT) {
                if(this->data_buffer_len>8){
                    this->data_len+=8;
                    this->data_buffer_len-=8;
                }else{
                    this->data_len+=this->data_buffer_len;
                    this->data_buffer_len-=this->data_buffer_len;
                }
            }else{
                this->data_len+=8;
                this->data_buffer_len-=8;
            }

            this->data_buffer>>=8;
        }
        void flush_bytes(){
            int num_bytes_in_buffer=static_cast<int>(this->data_buffer_len/8);
            for(int i=0;i<num_bytes_in_buffer;i++){
                this->flush_1_byte();
            }
        }
        void flush_complete(){
            this->flush_bytes();
            if (this->data_buffer_len>0) {
                // flush a full byte, which may underflow the data_buffer_len calculation
                this->flush_1_byte<true>();

                // correct potential data_buffer_len underflow
                this->data_buffer_len=0;
            }
        }
        void write(uint64_t data, uint64_t num_bits){
            uint64_t space_left_in_buffer=64-this->data_buffer_len;
            if (space_left_in_buffer<num_bits) {
                this->flush_bytes();
            }

            this->data_buffer|=data<<this->data_buffer_len;
            this->data_buffer_len+=num_bits;
        }
};

namespace LZW{
    constexpr static const char* const alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    typedef struct DictionaryEntry{
        uint64_t index_in_dictionary;

        int sym_len;
        char* sym;

        uint32_t bit_len;
        uint32_t bits;
    }DictionaryEntry;

    class Encoder{
        public:
            uint32_t code_length;

            template<bool EARLY_INCREASE=false>
            Encoder(
                BitStreamWriter<>& writer,
                const char* const encode_str,
                uint32_t initial_code_length,
                uint64_t initial_dictionary_size
            ):code_length(initial_code_length){
                const char terminator='#';
                int data_len=(int)strlen(encode_str);

                int current_symbol_len=1;
                uint64_t current_dictionary_size=initial_dictionary_size;

                uint32_t next_code=1;
                DictionaryEntry* dictionary=(DictionaryEntry*)malloc(sizeof(DictionaryEntry)*current_dictionary_size);
                for(int i=0;i<26;i++){
                    dictionary[i].index_in_dictionary=static_cast<uint64_t>(i+1);

                    dictionary[i].sym_len=current_symbol_len;
                    dictionary[i].sym=(char*)alphabet+i;

                    dictionary[i].bit_len=this->code_length;
                    dictionary[i].bits=next_code++;
                }

                for(int i=0;i<data_len;){
                    uint64_t largest_dict_match_index=0;

                    for(uint64_t d_i=0;d_i<current_dictionary_size;d_i++){
                        if(largest_dict_match_index!=0 && dictionary[d_i].sym_len<dictionary[largest_dict_match_index].sym_len)
                            continue;

                        if(strncmp(dictionary[d_i].sym,encode_str+i,(uint32_t)dictionary[d_i].sym_len)==0){
                            largest_dict_match_index=d_i;
                        }
                    }

                    if(largest_dict_match_index==0)
                        throw FATAL_UNEXPECTED_ERROR;

                    // insert new value into alphabet

                    uint64_t realloc_size=sizeof(DictionaryEntry)*(current_dictionary_size+1);
                    dictionary=(DictionaryEntry*)realloc(dictionary,realloc_size);

                    DictionaryEntry* new_entry=dictionary+current_dictionary_size;
                    DictionaryEntry* largest_dict_match=dictionary+largest_dict_match_index;

                    bool should_increase_code_size=false;
                    if((next_code+1)>(1<<this->code_length)){
                        should_increase_code_size=true;
                    }

                    if(EARLY_INCREASE&&should_increase_code_size)
                        this->code_length++;

                    new_entry->bit_len=this->code_length;

                    if((!EARLY_INCREASE)&&should_increase_code_size)
                        this->code_length++;

                    new_entry->sym=(char*)encode_str+i;
                    new_entry->sym_len=largest_dict_match->sym_len+1;
                    new_entry->bits=next_code++;
                    new_entry->index_in_dictionary=current_dictionary_size+1;

                    writer.write(largest_dict_match->index_in_dictionary,new_entry->bit_len);
                    println(
                        "wrote value %2" PRIu64 " for sym %3.*s",
                        largest_dict_match->index_in_dictionary,
                        largest_dict_match->sym_len,
                        largest_dict_match->sym
                    );

                    if(encode_str[i+largest_dict_match->sym_len]==terminator)
                        break;
                    
                    i+=new_entry->sym_len-1;
                    current_dictionary_size+=1;
                }
                // write terminator (a number of 0 bits at current code length)
                writer.write(0,this->code_length);
                writer.flush_complete();
                println("done after writing %" PRIu64 " bits",writer.data_len);
            }
    };
    class Decoder{
        public:
            typedef bitStream::BitStream<bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT,false> BitStream;
            
            uint32_t code_length;

            template<bool EARLY_INCREASE=false>
            Decoder(
                BitStream& stream,
                const uint32_t initial_code_length,
                const uint64_t initial_dictionary_size
            ):code_length(initial_code_length){
                auto dictionary_size=initial_dictionary_size;

                DictionaryEntry* dictionary=(DictionaryEntry*)malloc(sizeof(DictionaryEntry)*initial_dictionary_size);
                for(uint64_t i=0;i<initial_dictionary_size;i++){
                    dictionary[i].index_in_dictionary=i+1;
                    dictionary[i].sym=(char*)alphabet+i;
                    dictionary[i].sym_len=1;
                    dictionary[i].bits=(uint32_t)i+1;
                    dictionary[i].bit_len=this->code_length;
                }

                DictionaryEntry prev_match;
                prev_match.sym=nullptr;

                uint64_t somebits;
                do {
                    if ((dictionary_size+2)>(1<<this->code_length)) {
                        this->code_length++;
                    }

                    somebits=stream.get_bits_advance((uint8_t)this->code_length);

                    DictionaryEntry dict_match;
                    for (uint64_t i=0; i<dictionary_size; i++) {
                        if(dictionary[i].bits==somebits){
                            dict_match=dictionary[i];
                        }
                    }

                    if (somebits==0) {
                        break;
                    }
                    println("got val %" PRIu64 " of len %d (cur len %d) for sym %.*s",somebits,dict_match.bit_len,this->code_length,dict_match.sym_len,dict_match.sym);

                    if(prev_match.sym!=nullptr){
                        dictionary=(DictionaryEntry*)realloc(dictionary, sizeof(DictionaryEntry)*(dictionary_size+1));
                        DictionaryEntry* new_entry=dictionary+dictionary_size;

                        new_entry->index_in_dictionary=dictionary_size+1;
                        new_entry->bit_len=this->code_length;
                        new_entry->bits=(uint32_t)dictionary_size+1;
                        new_entry->sym_len=prev_match.sym_len+1;
                        new_entry->sym=(char*)malloc(sizeof(char)*(uint64_t)(new_entry->sym_len));

                        memcpy(
                            (void*)new_entry->sym,
                            prev_match.sym,
                            (uint64_t)prev_match.sym_len
                        );
                        memcpy(
                            (void*)(new_entry->sym+prev_match.sym_len),
                            dict_match.sym,
                            1
                        );

                        //println("created dict %d entry %.*s",new_entry->bits,new_entry->sym_len,new_entry->sym);

                        dictionary_size++;
                    }
                    if (stream.next_data_index>=stream.data_size) {
                        break;
                    }
                    prev_match=dict_match;
                } while(somebits!=0);
            }
    };
}
class GifParser:public FileParser{
    public:
        GIFVersion version;

        uint32_t num_entries_in_global_color_table;
        uint8_t global_color_table[3*265];

        ExtensionInformation extension_information;

        int current_image_index=0;

        GifParser(
            const char* file_path,
            ImageData* image_data
        ):FileParser(file_path,image_data){

        }

        GIFBlockSignature parse_next_block(){
            BitStreamWriter<> writer;
            const char* const somedata="TOBEORNOTTOBEORTOBEORNOT#";
            LZW::Encoder test_enc(writer,somedata,5,26);

            LZW::Decoder::BitStream stream;
            LZW::Decoder::BitStream::BitStream_new(&stream,writer.data,writer.data_len);
            LZW::Decoder(stream,5,26);

            throw "whatever";




            GIFBlockSignature next_block_signature{this->get_mem<uint8_t>()};
            switch(next_block_signature){
                case GIFBlockSignature::Extension:
                    {
                        println("extension block");
                        GIFBlockLabel extension_type{this->get_mem<uint8_t>()};
                        switch(extension_type){
                            case GIFBlockLabel::GraphicControl:
                                println("graphic control");
                                {
                                    uint8_t block_size=this->get_mem<uint8_t>();
                                    if(block_size!=4)
                                        bail(FATAL_UNEXPECTED_ERROR,"unexpected block size %d",block_size);

                                    GraphicControlPackedField packed_fields=this->get_mem<GraphicControlPackedField>();
                                    discard packed_fields;

                                    uint16_t delay_time=this->get_mem<uint16_t>();
                                    discard delay_time;
                                    uint8_t transparent_color_index=this->get_mem<uint8_t>();
                                    discard transparent_color_index;

                                    this->expect_signature(BLOCK_TERMINATOR, 1);
                                }
                                break;
                            case GIFBlockLabel::Application:
                                println("application");
                                {
                                    uint8_t block_size=this->get_mem<uint8_t>();
                                    println("block size %d",block_size);

                                    char application_name[8];
                                    memcpy(application_name,this->data_ptr(),8);
                                    this->current_file_content_index+=8;

                                    char application_identifier[3];
                                    memcpy(application_identifier,this->data_ptr(),3);
                                    this->current_file_content_index+=3;

                                    // found at http://www.vurdalakov.net/misc/gif/netscape-looping-application-extension
                                    // via https://stackoverflow.com/questions/26352546/how-to-decode-the-application-extension-block-of-gif
                                    if(memcmp("NETSCAPE",application_name,8)==0 && memcmp((const uint8_t*)"2.0",application_identifier,3)==0){
                                        println("found 'Netscape Looping Application Extension'");

                                        uint8_t sub_block_size=this->get_mem<uint8_t>();
                                        if(sub_block_size!=3)
                                            bail(FATAL_UNEXPECTED_ERROR,"unexpected sub-block size %d",sub_block_size);
                                        uint8_t sub_block_id=this->get_mem<uint8_t>();
                                        if(sub_block_id!=1)
                                            bail(FATAL_UNEXPECTED_ERROR,"unexpected sub-block id %d",sub_block_id);

                                        /// number of times the gif should loop
                                        uint16_t loop_count=this->get_mem<uint16_t>();
                                        
                                        this->extension_information.netscape_looping_extension.present=true;
                                        this->extension_information.netscape_looping_extension.num_loops=loop_count;
                                    }else{
                                        println("extension '%.8s' (ident. '%.3s')",application_name,application_identifier);

                                        // skip 'Application Data' that is inherently application defined (i.e. we don't know what to with it here)
                                        uint8_t remaining_block_size=block_size-11;
                                        if(remaining_block_size>0){
                                            bail(FATAL_UNEXPECTED_ERROR,"unimplemented (%d>0)",remaining_block_size);
                                            this->current_file_content_index+=remaining_block_size;
                                        }

                                        uint8_t next_byte=this->get_mem<uint8_t,false>();
                                        println("next byte: %x",next_byte);
                                    }
                                    this->expect_signature(BLOCK_TERMINATOR, 1);
                                }
                                break;
                            default:
                                bail(FATAL_UNEXPECTED_ERROR,"unknown gif extension block type %x",(uint8_t)extension_type);
                        }
                    }
                    break;
                case GIFBlockSignature::ImageDescriptor:
                    println("image descriptor");
                    {
                        uint16_t image_left_position=this->get_mem<uint16_t>();
                        uint16_t image_top_position=this->get_mem<uint16_t>();
                        uint16_t image_width=this->get_mem<uint16_t>();
                        uint16_t image_height=this->get_mem<uint16_t>();

                        println("image pos left %d top %d width %d height %d",image_left_position,image_top_position,image_width,image_height);

                        ImageDescriptorPackedField packed_fields=this->get_mem<ImageDescriptorPackedField>();
                        println("descriptor packed fields: interlaced %d sizeoflocalcolortable %d",packed_fields.interlace_flag,packed_fields.size_of_local_color_table);
                        println("packed_fields.size_of_local_color_table=%d",packed_fields.size_of_local_color_table);

                        // then parse 'Table Based Image Data' - see section 22
                        {
                            // image data is compressed using variable-length LZW compression
                            int lzw_minimum_code_size=this->get_mem<uint8_t>();
                            int clear_code=1<<lzw_minimum_code_size;
                            int stop_code=clear_code+1;
                            println("lzw_minimum_code_size %d, stop code %d",lzw_minimum_code_size,stop_code);

                            while(this->get_mem<uint8_t,false>()!=BLOCK_TERMINATOR[0]){
                                uint8_t block_size=this->get_mem<uint8_t>();
                                this->current_image_index+=block_size-1;
                            }
                            discard this->get_mem<uint8_t>();
                        }
                    }
                    this->current_image_index++;
                    break;
                default:
                    bail(FATAL_UNEXPECTED_ERROR,"unknown gif block type %x",(uint8_t)next_block_signature);
            }

            return next_block_signature;
        }
};

/// spec at https://www.w3.org/Graphics/GIF/spec-gif89a.txt
ImageParseResult Image_read_gif(
    const char* const filepath,
    ImageData* const  image_data
){
    GifParser parser{filepath,image_data};

    parser.expect_signature((const uint8_t*)"GIF", 3);

    char png_version_str[3];
    memcpy(png_version_str,parser.data_ptr(),3);
    if(memcmp(png_version_str,"87a",3)==0){
        parser.version=GIFVersion::V87a;
    }else if(memcmp(png_version_str,"89a",3)==0){
        parser.version=GIFVersion::V89a;
    }else{
        bail(FATAL_UNEXPECTED_ERROR,"png file has unknown version %.3s",png_version_str);
    }
    println("png version: %.3s",png_version_str);
    parser.current_file_content_index+=3;

    const uint16_t logical_screen_width=parser.get_mem<uint16_t>();
    const uint16_t logical_screen_height=parser.get_mem<uint16_t>();
    const uint8_t packed_fields=parser.get_mem<uint8_t>();
    /// Index into the Global Color Table for
    ///        the Background Color. The Background Color is the color used for
    ///        those pixels on the screen that are not covered by an image. If the
    ///        Global Color Table Flag is set to (zero), this field should be zero
    ///        and should be ignored.

    const uint8_t background_color_index=parser.get_mem<uint8_t>();
    discard background_color_index;
    ///        Aspect Ratio = (Pixel Aspect Ratio + 15) / 64
    ///
    ///        The Pixel Aspect Ratio is defined to be the quotient of the pixel's
    ///        width over its height.  The value range in this field allows
    ///        specification of the widest pixel of 4:1 to the tallest pixel of
    ///        1:4 in increments of 1/64th.
    ///
    ///        Values :        0 -   No aspect ratio information is given.
    ///                   1..255 -   Value used in the computation.
    const uint8_t pixel_aspect_ratio=parser.get_mem<uint8_t>();

    println("width %d height %d aspect ratio %d",logical_screen_width,logical_screen_height,pixel_aspect_ratio);

    ///          Values :    0 -   No Global Color Table follows, the Background
    ///                          Color Index field is meaningless.
    ///                    1 -   A Global Color Table will immediately follow, the
    ///                          Background Color Index field is meaningful.
    const uint8_t global_color_table_flag=packed_fields&0x80>>7;
    /// Number of bits per primary color available
    ///        to the original image, minus 1.
    const uint8_t color_resolution=packed_fields&0x70>>4;
    ///            Values :    0 -   Not ordered.
    ///                    1 -   Ordered by decreasing importance, most
    ///                          important color first.
    const uint8_t sort_flag=packed_fields&0x08>>3;
    /// If the Global Color Table Flag is
    ///         set to 1, the value in this field is used to calculate the number
    ///         of bytes contained in the Global Color Table. To determine that
    ///         actual size of the color table, raise 2 to [the value of the field
    ///         + 1].
    const uint8_t size_of_global_color_table=packed_fields&0x07;
    discard color_resolution;discard sort_flag;

    println("global color table flag %d",global_color_table_flag);

    if(global_color_table_flag){
        parser.num_entries_in_global_color_table=1<<(size_of_global_color_table+1);
        println("parser.num_entries_in_global_color_table is %d",parser.num_entries_in_global_color_table);
        for(uint32_t i=0;i<parser.num_entries_in_global_color_table;i++){
            parser.global_color_table[i*3+0]=parser.file_contents[parser.current_file_content_index++];
            parser.global_color_table[i*3+1]=parser.file_contents[parser.current_file_content_index++];
            parser.global_color_table[i*3+2]=parser.file_contents[parser.current_file_content_index++];
        }
    }else{
        parser.num_entries_in_global_color_table=0;
    }

    while(parser.parse_next_block()!=GIFBlockSignature::Trailer)
        ;

    bail(FATAL_UNEXPECTED_ERROR,"unimplemented");

    return IMAGE_PARSE_RESULT_OK;
}

#include <cstdint>

#include "app/bitstream.hpp"
#include "app/macros.hpp"
#include "app/error.hpp"
#include "app/image.hpp"

enum class PngVersion{
    V87a,
    V89a,
};

enum class PngBlockSignature:uint8_t{
    /// section 20
    ImageDescriptor=0x2c,
    /// see PngBlockLabel
    Extension=0x21,
    /// this block indicates the end of the GIF data stream
    Trailer=0x3b,
};
/// specifically for extensions
enum class PngBlockLabel:uint8_t{
    /// section 23
    GraphicControl=0xf9,
    /// section 25
    PlainText=0x01,
    /// section 26
    Application=0xff,
};
/// Indicates the way in which the graphic is to be treated after being displayed.
enum class PngDisposalMethod{
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
class GifParser:public FileParser{
    public:
        PngVersion version;

        uint32_t num_entries_in_global_color_table;
        uint8_t global_color_table[3*265];

        ExtensionInformation extension_information;

        int current_image_index=0;

        GifParser(
            const char* file_path,
            ImageData* image_data
        ):FileParser(file_path,image_data){

        }

        class LZWEncoder{
            constexpr static const char* const alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            public:
                int code_length;

                template<bool EARLY_INCREASE=false>
                LZWEncoder(
                    const char* const encode_str,
                    int initial_code_length,
                    uint64_t initial_dictionary_size
                ):code_length(initial_code_length){
                    const char terminator='#';
                    int data_len=(int)strlen(encode_str);

                    int num_bits_output=0;

                    int current_symbol_len=1;
                    uint64_t current_dictionary_size=initial_dictionary_size;
                    typedef struct DictionaryEntry{
                        int sym_len;
                        const char* val;
                        int bit_len;
                        int bits;
                    }DictionaryEntry;
                    int next_code=1;
                    DictionaryEntry* dictionary=(DictionaryEntry*)malloc(sizeof(DictionaryEntry)*current_dictionary_size);
                    for(int i=0;i<26;i++){
                        dictionary[i].sym_len=current_symbol_len;
                        dictionary[i].val=alphabet+i;
                        dictionary[i].bit_len=this->code_length;

                        dictionary[i].bits=next_code++;
                    }

                    for(int i=0;i<data_len;){
                        int dictionary_longest_match_found=0;
                        for(uint64_t d_i=0;d_i<current_dictionary_size;d_i++){
                            if(dictionary[d_i].sym_len<dictionary_longest_match_found)
                                continue;

                            if(strncmp(dictionary[d_i].val,encode_str+i,(uint32_t)dictionary[d_i].sym_len)==0){
                                dictionary_longest_match_found=dictionary[d_i].sym_len;
                            }
                        }

                        // insert new value into alphabet

                        uint64_t realloc_size=sizeof(DictionaryEntry)*(current_dictionary_size+1);
                        dictionary=(DictionaryEntry*)realloc(dictionary,realloc_size);

                        DictionaryEntry* new_entry=dictionary+current_dictionary_size;

                        bool should_increase_code_size=false;
                        if((next_code+1)>(1<<this->code_length)){
                            should_increase_code_size=true;
                        }

                        if(EARLY_INCREASE&&should_increase_code_size)
                            this->code_length++;

                        new_entry->bit_len=this->code_length;

                        if((!EARLY_INCREASE)&&should_increase_code_size)
                            this->code_length++;

                        new_entry->val=encode_str+i;
                        new_entry->sym_len=dictionary_longest_match_found+1;
                        new_entry->bits=next_code++;

                        num_bits_output+=new_entry->bit_len;

                        if(encode_str[i+dictionary_longest_match_found]==terminator)
                            break;

                        println("%.*s inserted",
                            new_entry->sym_len,
                            new_entry->val
                        );
                        
                        i+=new_entry->sym_len-1;
                        current_dictionary_size+=1;
                    }
                    // write terminator (a number of 0 bits at current code length)
                    num_bits_output+=this->code_length;
                    println("done after writing %d bits",num_bits_output);
                }
        };

        /*class LZWDecoder{
            static const char* const alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            int current_code_length;
            int current_dictionary_size=0;
            char** dictionary=nullptr;

            LZWDecoder(
                char*data,
                int data_length,
                int initial_code_length
            ){
                this->current_code_length=initial_code_length;

                bitStream::BitStream<bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT, false> stream((uint8_t*)data,(uint64_t)data_length);

            }
        };*/

        PngBlockSignature parse_next_block(){
            const char* const somedata="TOBEORNOTTOBEORTOBEORNOT#";
            LZWEncoder test_enc(somedata,5,26);
            //LZWDecoder test(somedata,strlen(somedata));

            PngBlockSignature next_block_signature{this->get_mem<uint8_t>()};
            switch(next_block_signature){
                case PngBlockSignature::Extension:
                    {
                        println("extension block");
                        PngBlockLabel extension_type{this->get_mem<uint8_t>()};
                        switch(extension_type){
                            case PngBlockLabel::GraphicControl:
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
                            case PngBlockLabel::Application:
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
                case PngBlockSignature::ImageDescriptor:
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
        parser.version=PngVersion::V87a;
    }else if(memcmp(png_version_str,"89a",3)==0){
        parser.version=PngVersion::V89a;
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

    while(parser.parse_next_block()!=PngBlockSignature::Trailer)
        ;

    bail(FATAL_UNEXPECTED_ERROR,"unimplemented");

    return IMAGE_PARSE_RESULT_OK;
}

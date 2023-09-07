#include "app/image.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <ctime>

#ifdef VK_USE_PLATFORM_METAL_EXT
    #include <arm_neon.h>
#elif VK_USE_PLATFORM_XCB_KHR
    #include <x86intrin.h>
#endif

#ifdef JPEG_DECODE_PARALLEL
    const uint32_t JPEG_DECODE_NUM_THREADS=4;
#else
    const uint32_t JPEG_DECODE_NUM_THREADS=1;
#endif

#define HB_U8(VARIABLE) ((VARIABLE&0xF0)>>4)
#define LB_U8(VARIABLE) (VARIABLE&0xF)

#include "app/app.hpp"
#include "app/error.hpp"
#include "app/huffman.hpp"
#include "app/bit_util.hpp"

typedef huffman::CodingTable<uint8_t, bitStream::BITSTREAM_DIRECTION_LEFT_TO_RIGHT, true> HuffmanTable;
typedef HuffmanTable::BitStream_ BitStream;

typedef int16_t MCU_EL;
#ifndef USE_FLOAT_PRECISION
    #define PRECISION 7
    typedef int16_t OUT_EL; // 16bits are kinda enough, but some images then peak on individual pixels (i.e. random pixels are white)
#else
    typedef float OUT_EL;
#endif

enum class JpegSegmentType:uint16_t{
    SOI=0xFFD8,
    EOI=0xFFD9,

    SOF0=0xFFC0,
    SOF1=0xFFC1,
    SOF2=0xFFC2,
    SOF3=0xFFC3,
    SOF5=0xFFC5,
    SOF6=0xFFC6,
    SOF7=0xFFC7,
    SOF8=0xFFC8,
    SOF9=0xFFC9,
    SOF10=0xFFCA,
    SOF11=0xFFCB,
    SOF13=0xFFCD,
    SOF14=0xFFCE,
    SOF15=0xFFCF,

    /// define huffman tables
    DHT=0xFFC4,
    /// define arithmetic coding conditions
    DAC=0xFFCC,

    /// start of scan
    SOS=0xFFDA,

    /// define quantization tables
    DQT=0xFFDB,
    /// define number of lines
    DNL=0xFFDC,
    /// define restart inverval
    DRI=0xFFDD,
    /// define hierachical progression
    DHP=0xFFDE,
    /// expand reference components
    EXP=0xFFDF,
    /// comment
    COM=0xFFFE,

    APP0=0xFFE0,
    APP1=0xFFE1,
    APP2=0xFFE2,
    APP3=0xFFE3,
    APP4=0xFFE4,
    APP5=0xFFE5,
    APP6=0xFFE6,
    APP7=0xFFE7,
    APP8=0xFFE8,
    APP9=0xFFE9,
    APP10=0xFFEA,
    APP11=0xFFEB,
    APP12=0xFFEC,
    APP13=0xFFED,
    APP14=0xFFEE,
    APP15=0xFFEF,
};

bool JpegSegmentType_hasSegmentBody(const JpegSegmentType segment_type)noexcept{
    switch(segment_type){
        case JpegSegmentType::SOI:
        case JpegSegmentType::EOI:
            return false;
        default:
            return true;
    }
}
const char* Image_jpeg_segment_type_name(JpegSegmentType segment_type){
    #define CASE(CASE_NAME) case JpegSegmentType::CASE_NAME: return #CASE_NAME;

    switch (segment_type) {
        CASE(SOI)
        CASE(EOI)

        CASE(SOF0)
        CASE(SOF1)
        CASE(SOF2)
        CASE(SOF3)
        CASE(SOF5)
        CASE(SOF6)
        CASE(SOF7)
        CASE(SOF8)
        CASE(SOF9)
        CASE(SOF10)
        CASE(SOF11)
        CASE(SOF13)
        CASE(SOF14)
        CASE(SOF15)
        CASE(DHT)
        CASE(DAC)
        CASE(SOS)
        CASE(DQT)
        CASE(DNL)
        CASE(DRI)
        CASE(DHP)
        CASE(EXP)
        CASE(COM)
        CASE(APP0)
        CASE(APP1)
        CASE(APP2)
        CASE(APP3)
        CASE(APP4)
        CASE(APP5)
        CASE(APP6)
        CASE(APP7)
        CASE(APP8)
        CASE(APP9)
        CASE(APP10)
        CASE(APP11)
        CASE(APP12)
        CASE(APP13)
        CASE(APP14)
        CASE(APP15)
    }

    return NULL;
}

typedef OUT_EL QUANT;
typedef QUANT QuantizationTable[64];

static const uint8_t ZIGZAG[64]={
    0,  1,  5,  6,  14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63,
};
[[maybe_unused]]
static const int UNZIGZAG[64]={
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

typedef struct ImageComponent{
    uint32_t vert_samples;
    uint32_t horz_samples;

    uint8_t component_id;
    uint8_t vert_sample_factor;
    uint8_t horz_sample_factor;
    uint8_t quant_table_specifier;

    uint32_t num_scans;
    uint32_t num_blocks_in_scan;

    uint32_t total_num_blocks;

    /// decompressed scans, where each scan has its own memory
    MCU_EL** scan_memory;

    OUT_EL* out_block_downsampled;

    uint32_t* conversion_indices;
}ImageComponent;

namespace ProcessBlock{
    [[gnu::flatten,gnu::hot,gnu::nonnull(1,2,3,4)]]
    static inline void decode_dc(
        MCU_EL* const  block_mem,

        const HuffmanTable* const  dc_table,
        MCU_EL* const  diff_dc,

        BitStream* const  stream,

        const uint8_t successive_approximation_bit_low
    ){
        uint8_t dc_magnitude=(uint8_t)dc_table->lookup(stream);

        const MCU_EL lookahead_dc_value_bits=(MCU_EL)stream->get_bits(12);

        if (dc_magnitude>0) {
            const MCU_EL dc_value_bits=lookahead_dc_value_bits>>(12-dc_magnitude);
            stream->advance_unsafe(dc_magnitude);
            
            MCU_EL dc_value=bitUtil::twos_complement(static_cast<MCU_EL>(dc_magnitude), dc_value_bits);

            *diff_dc+=dc_value;
        }

        block_mem[0]=(MCU_EL)(*diff_dc<<successive_approximation_bit_low);
    }
    /**
    * @brief decode block with successive approximation high at zero
    * 
    * @param block_mem 
    * @param ac_table 
    * @param spectral_selection_start 
    * @param spectral_selection_end 
    * @param bit_stream 
    * @param successive_approximation_bit_low 
    * @param eob_run 
    */
    [[gnu::always_inline,gnu::flatten,gnu::hot,gnu::nonnull(1,2,5,7)]]
    static inline void decode_block_ac(
        MCU_EL* const  block_mem,

        const HuffmanTable* const  ac_table,

        const uint8_t spectral_selection_start,
        const uint8_t spectral_selection_end,

        BitStream* const  stream,

        const uint8_t successive_approximation_bit_low,

        uint64_t* const  eob_run
    ){
        for(
            int spec_sel=spectral_selection_start;
            spec_sel<=spectral_selection_end;
        ){
            const auto ac_bits=ac_table->lookup(stream);

            if (ac_bits==0) {
                break;
            }

            const auto num_zeros=ac_bits>>4;
            const auto ac_magnitude=ac_bits&0xF;

            if (ac_magnitude==0) {
                if (num_zeros==15) {
                    spec_sel+=16;
                    continue;
                }else{
                    *eob_run=bitUtil::get_mask_u32(num_zeros);
                    *eob_run+=stream->get_bits_advance((uint8_t)num_zeros);

                    break;
                }
            }

            spec_sel+=num_zeros;
            if (spec_sel>spectral_selection_end) {
                break;
            }

            const MCU_EL ac_value_bits=static_cast<MCU_EL>(stream->get_bits_advance((uint8_t)ac_magnitude));

            const MCU_EL ac_value=bitUtil::twos_complement(static_cast<MCU_EL>(ac_magnitude),ac_value_bits);

            block_mem[spec_sel++]=static_cast<MCU_EL>(ac_value<<successive_approximation_bit_low);
        }
    }

    [[gnu::flatten,gnu::hot,gnu::nonnull(1,2)]]
    static inline uint8_t refine_block(
        MCU_EL* const  block_mem,

        BitStream* const  stream,

        const uint8_t range_start,
        const uint8_t range_end,

        uint8_t num_zeros,
        const MCU_EL bit
    ){
        for(uint8_t index= range_start;index<=range_end;index++){
            if(block_mem[index]==0){
                if(num_zeros==0){
                    return index;
                }

                num_zeros -= 1;
            }else{
                uint64_t next_bit=stream->get_bits_advance(1);
                if(next_bit==1 && (block_mem[index]&bit) == 0){
                    if(block_mem[index]>0){
                        block_mem[index] += bit;
                    }else{
                        block_mem[index] -= bit;
                    }
                }
            }
        }

        return range_end;
    }
    /**
    * @brief decode block with successive approximation high set
    * 
    * @param block_mem 
    * @param ac_table 
    * @param spectral_selection_start 
    * @param spectral_selection_end 
    * @param bit_stream 
    * @param successive_approximation_bit_low 
    * @param successive_approximation_bit_high 
    * @param eob_run 
    */
    [[gnu::flatten,gnu::hot,gnu::nonnull(1,2,5,7)]]
    static inline void decode_block_with_sbh(
        MCU_EL* const  block_mem,

        const HuffmanTable* const  ac_table,

        const uint8_t spectral_selection_start,
        const uint8_t spectral_selection_end,

        BitStream* const  stream,

        const MCU_EL succ_approx_bit_shifted,

        uint64_t* const  eob_run
    ){
        uint8_t next_pixel_index=spectral_selection_start;
        for(;next_pixel_index <= spectral_selection_end;){
            const auto ac_bits=ac_table->lookup(stream);

            // 4 most significant bits are number of zero-value bytes inserted before actual value (may be zero)
            auto num_zeros=ac_bits>>4;
            // no matter number of '0' bytes inserted, last 4 bits in ac value denote (additional) pixel value
            const uint32_t ac_magnitude=ac_bits&0xF;

            MCU_EL value=0;

            if(ac_magnitude==0){
                switch(num_zeros){
                    case 15:
                        break; // num_zeros is 15, value is zero => 16 zeros written already
                    default:
                        {
                            uint32_t eob_run_bits=static_cast<uint32_t>(stream->get_bits_advance((uint8_t)num_zeros));
                            *eob_run=bitUtil::get_mask_u32(num_zeros) + eob_run_bits;
                        }
                        num_zeros=64;
                        break;
                    case 0:
                        *eob_run=0;
                        num_zeros=64;
                }
            }else{
                if(stream->get_bits_advance(1)==1){
                    value = succ_approx_bit_shifted;
                }else{
                    value = -succ_approx_bit_shifted;
                }
            }

            next_pixel_index=refine_block(
                block_mem,
                stream, 

                next_pixel_index, 
                spectral_selection_end, 

                (uint8_t)num_zeros, 
                succ_approx_bit_shifted
            );

            if(value != 0){
                block_mem[next_pixel_index]=value;
            }
            
            next_pixel_index+=1;
        }
    }
}

class IDCTMaskSet {
    private:
        [[gnu::pure,gnu::hot]]
        constexpr static inline float coeff(const uint32_t u){
            if(u==0){
                return 1.0f/(float)M_SQRT2;
            }else{
                return 1.0f;
            }
        }
        
    public:
        OUT_EL idct_element_masks[64][64];

        IDCTMaskSet(){
            for(uint32_t mask_index = 0;mask_index<64;mask_index++){
                for(uint32_t ix = 0;ix<8;ix++){
                    for(uint32_t iy = 0;iy<8;iy++){
                        const uint32_t mask_u=mask_index%8;
                        const uint32_t mask_v=mask_index/8;

                        const float x_cos_arg = ((2.0f * (float)(iy) + 1.0f) * (float)(mask_u) * (float)M_PI) / 16.0f;
                        const float y_cos_arg = ((2.0f * (float)(ix) + 1.0f) * (float)(mask_v) * (float)M_PI) / 16.0f;

                        const float x_val = cosf(x_cos_arg) * coeff(mask_u);
                        const float y_val = cosf(y_cos_arg) * coeff(mask_v);

                        // the divide by 4 comes from the spec, from the algorithm to reverse the application of the IDCT
                        #ifndef USE_FLOAT_PRECISION
                            const OUT_EL value = (OUT_EL)(((x_val * y_val)/4)*(1<<PRECISION));
                        #else
                            const OUT_EL value = (x_val * y_val)/4;
                        #endif

                        const uint32_t mask_pixel_index=8*ix+iy;
                        this->idct_element_masks[ZIGZAG[mask_index]][mask_pixel_index]=value;
                    }
                }
            }
        }

        template<typename T>
        const OUT_EL* operator[](const T index)const noexcept{
            return (OUT_EL*)this->idct_element_masks[index];
        }
};
static const IDCTMaskSet IDCT_MASK_SET;

class ScanComponent{
    public:
        uint8_t vert_sample_factor;
        uint8_t horz_sample_factor;

        uint32_t horz_samples;
        uint32_t vert_samples;

        uint32_t num_scans;
        uint32_t num_blocks_in_scan;
        
        uint8_t component_index_in_image;

        uint32_t num_blocks;

        uint32_t num_horz_blocks;

        HuffmanTable* ac_table;
        HuffmanTable* dc_table;

        MCU_EL** scan_memory;

        ScanComponent(){
            vert_sample_factor=0;
            horz_sample_factor=0;
            horz_samples=0;
            vert_samples=0;
            num_scans=0;
            num_blocks_in_scan=0;
            component_index_in_image=0;
            num_blocks=0;
            num_horz_blocks=0;
            ac_table=0;
            dc_table=0;
            scan_memory=0;
        }

        [[gnu::hot,gnu::flatten,gnu::nonnull(3,4,6,7)]]
        inline void process_mcu_baseline(
            const uint32_t mcu_col,
            BitStream* const  stream,
            MCU_EL* const  diff_dc,
            const uint8_t successive_approximation_bit_low,
            MCU_EL* const  mcu_memory,
            uint64_t* const  eob_run
        )const noexcept{
            const uint32_t vert_sample_factor=this->vert_sample_factor;
            const uint32_t horz_sample_factor=this->horz_sample_factor;

            const HuffmanTable* const ac_table=this->ac_table;
            const HuffmanTable* const dc_table=this->dc_table;

            for (uint32_t vert_sid=0; vert_sid<vert_sample_factor; vert_sid++) {
                for (uint32_t horz_sid=0; horz_sid<horz_sample_factor; horz_sid++) {
                    const uint32_t block_col=mcu_col*horz_sample_factor + horz_sid;

                    uint32_t component_block_id = block_col + vert_sid * this->num_horz_blocks;
                    
                    MCU_EL* const block_mem=&mcu_memory[component_block_id*64];

                    ProcessBlock::decode_dc(block_mem, dc_table, diff_dc, stream, successive_approximation_bit_low);

                    if(*eob_run>0){
                        *eob_run-=1;
                        continue;
                    }
                    
                    ProcessBlock::decode_block_ac(block_mem, ac_table, 1, 63, stream, successive_approximation_bit_low, eob_run);
                
                }
            }
        }

        [[gnu::flatten,gnu::nonnull(3,4,6,11)]]
        inline void process_mcu_generic(
            uint32_t const mcu_col,
            BitStream* const  stream,
            MCU_EL* const  diff_dc,
            uint8_t const successive_approximation_bit_low,
            MCU_EL* const  mcu_memory,
            bool is_interleaved,
            uint8_t const spectral_selection_start,
            uint8_t const spectral_selection_end,
            uint8_t const successive_approximation_bit_high,
            uint64_t* const  eob_run,
            MCU_EL const succ_approx_bit_shifted
        )const noexcept{
            const uint32_t vert_sample_factor=this->vert_sample_factor;
            const uint32_t horz_sample_factor=this->horz_sample_factor;

            const HuffmanTable* const ac_table=this->ac_table;
            const HuffmanTable* const dc_table=this->dc_table;

            for (uint32_t vert_sid=0; vert_sid<vert_sample_factor; vert_sid++) {
                for (uint32_t horz_sid=0; horz_sid<horz_sample_factor; horz_sid++) {
                    const uint32_t block_col=mcu_col*horz_sample_factor + horz_sid;

                    uint32_t component_block_id;
                    if (is_interleaved) {
                        component_block_id = block_col + vert_sid * this->num_horz_blocks;
                    }else {
                        component_block_id =
                            mcu_col
                                * horz_sample_factor*vert_sample_factor
                            + horz_sid
                            + vert_sid
                                * horz_sample_factor;
                    }
                    
                    MCU_EL* const block_mem=&mcu_memory[component_block_id*64];

                    if (successive_approximation_bit_high==0){
                        uint8_t scan_start=spectral_selection_start;

                        // decode dc
                        if (spectral_selection_start==0) {
                            ProcessBlock::decode_dc(block_mem, dc_table, diff_dc, stream, successive_approximation_bit_low);

                            scan_start=1;
                        }

                        if(spectral_selection_end==0){
                            continue;
                        }

                        if(*eob_run>0){
                            *eob_run-=1;
                            continue;
                        }
                        
                        // decode ac's
                        const uint8_t sse=spectral_selection_end;
                        uint8_t spec_sel=scan_start;
                        for (; spec_sel<=sse;) {
                            const auto ac_bits=ac_table->lookup(stream);

                            if (ac_bits==0) {
                                break;
                            }

                            const auto num_zeros=static_cast<uint8_t>(ac_bits>>4);
                            const auto ac_magnitude=static_cast<uint8_t>(ac_bits&0xF);

                            if (ac_magnitude==0) {
                                if (num_zeros==15) {
                                    spec_sel+=16;
                                    continue;
                                }else {
                                    *eob_run=bitUtil::get_mask_u32(num_zeros);
                                    *eob_run+=stream->get_bits_advance((uint8_t)num_zeros);

                                    break;
                                }
                            }

                            spec_sel+=num_zeros;
                            if (spec_sel>sse) {
                                break;
                            }

                            const MCU_EL ac_value_bits=static_cast<MCU_EL>(stream->get_bits_advance(ac_magnitude));

                            const MCU_EL ac_value=bitUtil::twos_complement(static_cast<MCU_EL>(ac_magnitude),ac_value_bits);

                            block_mem[spec_sel++]=static_cast<MCU_EL>(ac_value<<successive_approximation_bit_low);
                        }
                    }else{
                        if(spectral_selection_start == 0){
                            const uint64_t test_bit=stream->get_bits_advance(1);
                            if(test_bit){
                                block_mem[0] += succ_approx_bit_shifted;
                            }

                            continue;
                        }

                        if(*eob_run>0){
                            *eob_run-=1;

                            discard ProcessBlock::refine_block(
                                block_mem,
                                stream, 

                                spectral_selection_start, 
                                spectral_selection_end, 

                                64, 
                                succ_approx_bit_shifted
                            );

                            continue;
                        }

                        if(spectral_selection_end == 0)
                            continue;

                        ProcessBlock::decode_block_with_sbh(
                            block_mem,
                            ac_table, 
                            spectral_selection_start, 
                            spectral_selection_end, 
                            stream, 
                            succ_approx_bit_shifted,
                            eob_run
                        );
                    }
                }
            }
        }
};

class JpegParser;
struct ProcessIncomingScan_Arguments{
    JpegParser* parser;
    uint8_t channel;
    std::atomic<uint32_t> num_scans_parsed;
    ImageData* image_data;
};
void* ProcessIncomingScans_pthread(struct ProcessIncomingScan_Arguments* async_args);

class JpegParser: public FileParser{
    public:
    #ifdef DEBUG
        double start_time;
        double parse_end_time;
        double process_end_time;
        double convert_end_time;
        double crop_end_time;
    #endif

    QuantizationTable quant_tables[4];
    HuffmanTable ac_coding_tables[4];
    HuffmanTable dc_coding_tables[4];

    uint8_t max_component_vert_sample_factor;
    uint8_t max_component_horz_sample_factor;

    ImageComponent image_components[3];

    /// sample precision
    uint32_t P;
    /// image size in x/y dimension (in px)
    uint32_t X,Y;
    /// number of components in frame (channels)
    uint32_t Nf;
    uint32_t real_X,real_Y;

    uint32_t component_label;
    uint32_t color_space;

    /// decode in parallel, using multiple threads
    const bool parallel;
    struct ProcessIncomingScan_Arguments async_scan_info[3];
    pthread_t async_scan_processors[3];
    ScanComponent scan_components[3];

    // +1 for each component to allow component with index 0 to be actively counted
    static const uint32_t CHANNEL_COMPLETE=2016+64; // sum(0..<64) + 64
    uint32_t channel_completeness[3]={0,0,0};

    enum class EncodingMethod{
        Baseline,
        Progressive,

        UNDEFINED
    };

    EncodingMethod encoding_method;

    bool parsing_done=false;

    JpegParser(
        const char* const filepath,
        ImageData* const image_data,
        const bool parallel
    ):
        FileParser(filepath, image_data),
        parallel(parallel),
        parsing_done(false)
    {
        this->encoding_method=EncodingMethod::UNDEFINED;

        for(int i=0;i<4;i++){
            for(int j=0;j<64;j++)
                this->quant_tables[i][j]=0;

            this->ac_coding_tables[i].lookup_table=NULL;
            this->ac_coding_tables[i].max_code_length_bits=0;

            this->dc_coding_tables[i].lookup_table=NULL;
            this->dc_coding_tables[i].max_code_length_bits=0;
        };

        this->max_component_horz_sample_factor=0;
        this->max_component_vert_sample_factor=0;

        for(int i=0;i<3;i++){    
            this->image_components[i].component_id=0;
            this->image_components[i].horz_sample_factor=0;
            this->image_components[i].vert_sample_factor=0;
            this->image_components[i].horz_samples=0;
            this->image_components[i].vert_samples=0;
            this->image_components[i].quant_table_specifier=0;

            this->image_components[i].scan_memory=NULL;
            this->image_components[i].out_block_downsampled=NULL;
        }

        this->P=0;
        this->X=0;
        this->Y=0;
        this->Nf=0;
        this->real_X=0;
        this->real_Y=0;

        #ifdef DEBUG
            this->start_time=current_time();
            this->parse_end_time=0.0;
            this->process_end_time=0.0;
            this->convert_end_time=0.0;
            this->crop_end_time=0.0;
        #endif

        this->component_label=0;
        this->color_space=0;

        for(int i=0;i<3;i++){
            async_scan_info[i].parser=this;
            async_scan_info[i].channel=static_cast<uint8_t>(i);
            async_scan_info[i].num_scans_parsed=0;
            async_scan_info[i].image_data=this->image_data;
        }
    }

    /// cleanup all resources
    void destroy(){
        free(this->file_contents);

        for(int i=0;i<4;i++){
            this->ac_coding_tables[i].destroy();
            this->dc_coding_tables[i].destroy();
        }

        for(uint32_t c=0;c<this->Nf;c++){
            free(this->image_components[c].conversion_indices);
            free(this->image_components[c].out_block_downsampled);
            // free batch allocated scan memory
            free(this->image_components[c].scan_memory[0]);
            free(this->image_components[c].scan_memory);
        }
    }

    inline uint16_t next_u16()noexcept{
        return bitUtil::byteswap(this->get_mem<uint16_t>(),2);
    }

    void parse_file();

    [[gnu::flatten]]
    void process_channel(
        const uint8_t c,
        const uint32_t scan_id_start,
        const uint32_t scan_id_end
    )const noexcept{
        QuantizationTable component_quant_table;
        memcpy(component_quant_table,this->quant_tables[this->image_components[c].quant_table_specifier],sizeof(component_quant_table));

        // -- reverse idct and quantization table application

        OUT_EL* const  out_block_downsampled=this->image_components[c].out_block_downsampled;

        const uint32_t num_blocks_in_scan=this->image_components[c].num_blocks_in_scan;

        const OUT_EL idct_m0_v0=IDCT_MASK_SET.idct_element_masks[0][0];

        // local cache, with expanded size to allow simd instructions reading past the real content
        MCU_EL in_block[80];
        for(int i=64;i<80;i++) in_block[i]=0;

        for (uint32_t scan_id=scan_id_start; scan_id<scan_id_end; scan_id++) {
            const MCU_EL* const  scan_mem=this->image_components[c].scan_memory[scan_id];

            for (uint32_t block_id=0; block_id<num_blocks_in_scan; block_id++) {

                memcpy(in_block,scan_mem+block_id*64,64*sizeof(MCU_EL));

                const uint32_t block_pixel_range_start=(scan_id*num_blocks_in_scan+block_id);

                OUT_EL out_block[64];

                // use first idct mask index to initialize storage
                {
                    const OUT_EL cosine_mask_strength=(OUT_EL)(in_block[0]*component_quant_table[0]);

                    const OUT_EL idct_m0_value=idct_m0_v0*cosine_mask_strength;

                    for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                        out_block[pixel_index]=idct_m0_value;
                    }
                }

                for(uint32_t cosine_index = 1;cosine_index<64;){
                    #ifndef USE_FLOAT_PRECISION
                        #ifdef VK_USE_PLATFORM_XCB_KHR
                            const __m128i mask_strengths=_mm_loadu_si128((__m128i*)(&in_block[cosine_index]));
                            const __m128i elements_zero_result=_mm_cmpeq_epi16(mask_strengths, _mm_set1_epi16(0));
                            uint32_t elements_nonzero=0xFFFF-(uint32_t)_mm_movemask_epi8(elements_zero_result);

                            const uint32_t num_cosines_remaining=64-cosine_index;
                            const uint32_t num_cosines_remaining_in_current_iteration=bitUtil::min(8u,num_cosines_remaining);

                            const uint32_t all_elements_mask=mask_u32(num_cosines_remaining_in_current_iteration*2);
                            elements_nonzero&=all_elements_mask;

                            if(elements_nonzero==0){
                                cosine_index+=8;
                                continue;
                            }

                            cosine_index+=bitUtil::tzcnt_32(elements_nonzero)/2;
                        #elif defined( VK_USE_PLATFORM_METAL_EXT)
                            const int16x8_t mask_strengths=vld1q_s16((int16_t*)(&in_block[cosine_index]));
                            const int16x8_t elements_zero_result=vceqq_s16(mask_strengths, vdupq_n_s16(0));
                            uint64_t elements_nonzero=0;
                            vst1_s8((int8_t*)&elements_nonzero,vqmovn_s16(elements_zero_result));
                            elements_nonzero=UINT64_MAX-elements_nonzero;

                            //printf("mask %16llx\n",elements_nonzero);

                            const uint32_t num_cosines_remaining=64-cosine_index;
                            const uint32_t num_cosines_remaining_in_current_iteration=bitUtil::min(8u,num_cosines_remaining);

                            const uint64_t all_elements_mask=mask_u64((num_cosines_remaining_in_current_iteration-1)*8+1);
                            elements_nonzero&=all_elements_mask;

                            if(elements_nonzero==0){
                                cosine_index+=8;
                                continue;
                            }

                            while (in_block[cosine_index]==0 && cosine_index<63) cosine_index++;
                        #endif
                    #else
                        if(in_block[cosine_index] == 0) {
                            cosine_index++;
                            continue;
                        }
                    #endif

                    const MCU_EL pre_quantized_mask_strength=in_block[cosine_index];

                    const OUT_EL cosine_mask_strength=pre_quantized_mask_strength*component_quant_table[cosine_index];
                    const OUT_EL* const idct_mask=IDCT_MASK_SET[cosine_index];
                    
                    for(uint32_t pixel_index = 0;pixel_index<64;pixel_index++){
                        out_block[pixel_index]+=static_cast<OUT_EL>(idct_mask[pixel_index]*cosine_mask_strength);
                    }

                    cosine_index++;
                }

                memcpy(out_block_downsampled+block_pixel_range_start*64,out_block,sizeof(OUT_EL)*64);
            }
        }
    }

    template<JpegSegmentType SEGMENT_TYPE>
    void parse_segment(){
        bail(FATAL_UNEXPECTED_ERROR,"unimplemented segment %s",Image_jpeg_segment_type_name(SEGMENT_TYPE));
    }

    template<EncodingMethod ENCODING_METHOD>
    void parse_sof(){
        this->encoding_method=ENCODING_METHOD;

        const uint16_t segment_size=this->next_u16();
        const uint32_t segment_end_position=static_cast<uint32_t>(this->current_file_content_index)+segment_size-2;

        this->P=this->get_mem<uint8_t>();
        this->real_Y=this->next_u16();
        this->real_X=this->next_u16();
        this->Nf=this->get_mem<uint8_t>();

        if (this->P!=8)
            bail(-46,"image precision is not 8 - is %d instead\n",this->P);

        // parse basic per-component metadata
        for (uint32_t i=0; i<this->Nf; i++) {
            this->image_components[i].component_id=this->get_mem<uint8_t>();

            const uint32_t sample_factors=this->get_mem<uint8_t>();

            this->image_components[i].vert_sample_factor=HB_U8(sample_factors);
            this->image_components[i].horz_sample_factor=LB_U8(sample_factors);

            this->image_components[i].quant_table_specifier=this->get_mem<uint8_t>();

            if (this->image_components[i].vert_sample_factor>this->max_component_vert_sample_factor) {
                this->max_component_vert_sample_factor=this->image_components[i].vert_sample_factor;
            }
            if (this->image_components[i].horz_sample_factor>this->max_component_horz_sample_factor) {
                this->max_component_horz_sample_factor=this->image_components[i].horz_sample_factor;
            }
        }

        this->X=ROUND_UP(this->real_X,8);
        this->Y=ROUND_UP(this->real_Y,8);

        image_data->height=this->Y;
        image_data->width=this->X;

        // calculate per-component metadata and allocate scan memory
        for (uint32_t i=0; i<this->Nf; i++) {
            this->image_components[i].vert_samples=(ROUND_UP(this->Y,8*this->max_component_vert_sample_factor))*this->image_components[i].vert_sample_factor/this->max_component_vert_sample_factor;
            this->image_components[i].horz_samples=(ROUND_UP(this->X,8*this->max_component_horz_sample_factor))*this->image_components[i].horz_sample_factor/this->max_component_horz_sample_factor;

            this->X=bitUtil::max(this->X,this->image_components[i].horz_samples);
            this->Y=bitUtil::max(this->Y,this->image_components[i].vert_samples);

            const uint32_t component_data_size=this->image_components[i].vert_samples*this->image_components[i].horz_samples;

            const uint32_t component_num_scans=this->image_components[i].vert_samples/this->image_components[i].vert_sample_factor/8;
            const uint32_t component_num_scan_elements=this->image_components[i].horz_samples*this->image_components[i].vert_sample_factor*8;

            this->image_components[i].scan_memory=(MCU_EL**)malloc(sizeof(MCU_EL*)*component_num_scans);

            uint32_t per_scan_memory_size=ROUND_UP<uint32_t>(component_num_scan_elements*sizeof(MCU_EL),4096);
            MCU_EL* const total_scan_memory=(MCU_EL*)calloc(component_num_scans,per_scan_memory_size);
            for (uint32_t s=0; s<component_num_scans; s++) {
                this->image_components[i].scan_memory[s]=total_scan_memory+s*per_scan_memory_size/sizeof(MCU_EL);
                //parser->image_components[i].scan_memory[s]=calloc(1,component_num_scan_elements*sizeof(MCU_EL));
            }

            this->image_components[i].num_scans=component_num_scans;
            this->image_components[i].num_blocks_in_scan=component_num_scan_elements/64;

            this->image_components[i].out_block_downsampled=(OUT_EL*)aligned_alloc(64,ROUND_UP(sizeof(OUT_EL)*(component_data_size+16),64));

            this->image_components[i].total_num_blocks=this->image_components[i].vert_samples*this->image_components[i].horz_samples/64;

            this->component_label|=((uint32_t)this->image_components[i].horz_sample_factor)<<(((this->Nf-i)*2-1)*4);
            this->component_label|=((uint32_t)this->image_components[i].vert_sample_factor)<<(((this->Nf-i)*2-2)*4);

            this->color_space|=(uint32_t)(this->image_components[i].component_id<<(4*(this->Nf-1-i)));
        }
        
        const uint32_t num_pixels_per_scan=this->max_component_vert_sample_factor*8*this->X;

        // pre-calculate indices to re-order pixel data from block-orientation to row-column orientation
        for (uint32_t i=0; i<this->Nf; i++) {
            this->image_components[i].conversion_indices=(uint32_t*)malloc(sizeof(uint32_t)*num_pixels_per_scan);
            uint32_t* const conversion_indices=this->image_components[i].conversion_indices;

            uint32_t ci=0;
            for(uint32_t img_y=0;img_y<this->max_component_vert_sample_factor*8;img_y++){
                for (uint32_t img_x=0;img_x<this->X;img_x++) {
                    const uint32_t adjusted_img_y=img_y*this->image_components[i].vert_sample_factor/this->max_component_vert_sample_factor;
                    const uint32_t adjusted_img_x=img_x*this->image_components[i].horz_sample_factor/this->max_component_horz_sample_factor;

                    const uint32_t index=
                            (adjusted_img_y/8)*this->image_components[i].horz_samples*8
                        +(adjusted_img_x/8)*64
                        +(adjusted_img_y%8)*8
                        +adjusted_img_x%8;

                    conversion_indices[ci++]=index;
                }
            }

            if(ci!=num_pixels_per_scan)
                bail(FATAL_UNEXPECTED_ERROR,"this is a bug. %d != %d",ci,num_pixels_per_scan);
        }

        const uint32_t total_num_pixels_in_image=this->X*this->Y;

        // overallocate for simd access overflows
        static  const uint32_t OVERALLOCATE_NUM_BYTES=256;
        image_data->data=(uint8_t*)malloc(sizeof(uint8_t)*total_num_pixels_in_image*4+OVERALLOCATE_NUM_BYTES);

        this->current_file_content_index=segment_end_position;
    }

    template<EncodingMethod ENCODING_METHOD>
    void parse_sos(){
        const uint16_t segment_size=this->next_u16();
        discard segment_size;

        const uint8_t num_scan_components=this->get_mem<uint8_t>();

        const bool is_interleaved=num_scan_components != 1;

        uint8_t scan_component_id[3];
        uint8_t scan_component_ac_table_index[3];
        uint8_t scan_component_dc_table_index[3];

        for (uint32_t i=0; i<num_scan_components; i++) {
            scan_component_id[i]=this->get_mem<uint8_t>();

            const uint8_t table_indices=this->get_mem<uint8_t>();

            scan_component_dc_table_index[i]=HB_U8(table_indices);
            scan_component_ac_table_index[i]=LB_U8(table_indices);
        }

        const uint8_t spectral_selection_start=this->get_mem<uint8_t>();
        const uint8_t spectral_selection_end=this->get_mem<uint8_t>();

        const uint8_t successive_approximation_bits=this->get_mem<uint8_t>();
        const uint8_t successive_approximation_bit_low=LB_U8(successive_approximation_bits);
        const uint8_t successive_approximation_bit_high=HB_U8(successive_approximation_bits);

        MCU_EL differential_dc[3]={0,0,0};

        BitStream _bit_stream;
        BitStream* const  stream=&_bit_stream;
        BitStream::BitStream_new(stream, &this->file_contents[this->current_file_content_index],this->file_size-this->current_file_content_index);

        const uint32_t mcu_cols=this->image_components[0].horz_samples/this->image_components[0].horz_sample_factor/8;
        const uint32_t mcu_rows=this->image_components[0].vert_samples/this->image_components[0].vert_sample_factor/8;

        uint8_t scan_component_vert_sample_factor[3];
        uint8_t scan_component_horz_sample_factor[3];
        uint8_t scan_component_index_in_image[3]={UINT8_MAX,UINT8_MAX,UINT8_MAX};

        for (uint8_t scan_component_index=0; scan_component_index<num_scan_components; scan_component_index++) {
            for (uint8_t i=0; i<this->Nf; i++) {
                if (this->image_components[i].component_id==scan_component_id[scan_component_index]) {
                    scan_component_vert_sample_factor[scan_component_index]=this->image_components[i].vert_sample_factor;
                    scan_component_horz_sample_factor[scan_component_index]=this->image_components[i].horz_sample_factor;

                    scan_component_index_in_image[scan_component_index]=i;
                    break;
                }
            }

            if (scan_component_index_in_image[scan_component_index]==UINT8_MAX)
                bail(-102,"did not find image component?!\n");
        }

        for (int c=0; c<num_scan_components; c++) {
            scan_components[c].vert_sample_factor=scan_component_vert_sample_factor[c];
            scan_components[c].horz_sample_factor=scan_component_horz_sample_factor[c];

            const uint8_t component_index_in_image=scan_component_index_in_image[c];
            scan_components[c].component_index_in_image=component_index_in_image;

            scan_components[c].num_blocks=this->image_components[component_index_in_image].horz_samples/8*this->image_components[component_index_in_image].vert_samples/8;

            scan_components[c].dc_table=&this->dc_coding_tables[scan_component_dc_table_index[c]];
            scan_components[c].ac_table=&this->ac_coding_tables[scan_component_ac_table_index[c]];

            scan_components[c].scan_memory=this->image_components[component_index_in_image].scan_memory;

            scan_components[c].num_scans=this->image_components[component_index_in_image].num_scans;
            scan_components[c].num_blocks_in_scan=this->image_components[component_index_in_image].num_blocks_in_scan;

            scan_components[c].horz_samples=this->image_components[component_index_in_image].horz_samples;
            scan_components[c].vert_samples=this->image_components[component_index_in_image].vert_samples;

            scan_components[c].num_horz_blocks=scan_components[c].horz_samples/8;
        }

        if(parallel && (successive_approximation_bit_low==0)){
            for (uint32_t c=0; c<num_scan_components; c++) {
                const uint32_t t=scan_components[c].component_index_in_image;
                for (uint32_t i=spectral_selection_start; i<=spectral_selection_end; i++) {
                    channel_completeness[t]+=i+1;
                }
                if (channel_completeness[t]==CHANNEL_COMPLETE){
                    pthread_create(&async_scan_processors[t], NULL, (pthread_callback)ProcessIncomingScans_pthread, &async_scan_info[t]);
                }
            }
        }

        // needed when successive_approximation_bit_high>0
        const MCU_EL succ_approx_bit_shifted=(MCU_EL)(1<<successive_approximation_bit_low);

        uint64_t eob_run=0;

        for (uint32_t mcu_row=0;mcu_row<mcu_rows;mcu_row++) {
            MCU_EL* const scan_memories[3]={
                scan_components[0].scan_memory[mcu_row],
                scan_components[1].scan_memory[mcu_row],
                scan_components[2].scan_memory[mcu_row],
            };

            if constexpr(ENCODING_METHOD==EncodingMethod::Baseline){
                if(successive_approximation_bit_high!=0)
                    bail(FATAL_UNEXPECTED_ERROR,"this is a bug.");

                for (uint32_t mcu_col=0;mcu_col<mcu_cols;mcu_col++) {
                    for (uint32_t c=0; c<num_scan_components; c++) {
                        scan_components[c].process_mcu_baseline(
                            mcu_col,
                            stream,
                            &differential_dc[c],
                            successive_approximation_bit_low,
                            scan_memories[c],
                            &eob_run
                        );
                    }
                }
            }else{
                for (uint32_t mcu_col=0;mcu_col<mcu_cols;mcu_col++) {
                    for (uint32_t c=0; c<num_scan_components; c++) {
                        scan_components[c].process_mcu_generic(
                            mcu_col,
                            stream,
                            &differential_dc[c],
                            successive_approximation_bit_low,
                            scan_memories[c],
                            is_interleaved,
                            spectral_selection_start,
                            spectral_selection_end,
                            successive_approximation_bit_high,
                            &eob_run,
                            succ_approx_bit_shifted
                        );
                    }
                }
            }

            if(parallel&&(successive_approximation_bit_low==0))
                for(int t=0;t<num_scan_components;t++){
                    const uint8_t index=scan_components[t].component_index_in_image;
                    if (channel_completeness[index]==CHANNEL_COMPLETE)
                        async_scan_info[index].num_scans_parsed+=1;
                }
        }

        const uint32_t bytes_read_from_stream=(uint32_t)(stream->next_data_index-stream->buffer_bits_filled/8);

        this->current_file_content_index+=bytes_read_from_stream;
    }

    /// skip segment body (if it exists), based on the encoded segment size
    void skip_segment(JpegSegmentType segment_type){
        if(JpegSegmentType_hasSegmentBody(segment_type)){
            const uint32_t segment_size=this->next_u16();
            const uint32_t segment_end_position=static_cast<uint32_t>(this->current_file_content_index)+segment_size-2;
            this->current_file_content_index=segment_end_position;
        }
    }

    void convert_colorspace();

    void correct_image_size(){
        if(this->X!=this->real_X || this->Y!=this->real_Y){
            uint8_t* const old_data=image_data->data;
            uint8_t* const real_data=(uint8_t*)aligned_alloc(64,ROUND_UP(this->real_X*this->real_Y*4,64));

            for (uint32_t y=0; y<this->real_Y; y++) {
                for(uint32_t x=0; x<this->real_X; x++) {
                    real_data[(y*this->real_X+x)*4+0]=old_data[(y*this->X+x)*4+0];
                    real_data[(y*this->real_X+x)*4+1]=old_data[(y*this->X+x)*4+1];
                    real_data[(y*this->real_X+x)*4+2]=old_data[(y*this->X+x)*4+2];
                    real_data[(y*this->real_X+x)*4+3]=old_data[(y*this->X+x)*4+3];
                }
            }

            free(old_data);

            image_data->height=this->real_Y;
            image_data->width=this->real_X;
            image_data->data=real_data;

            #ifdef DEBUG
                this->crop_end_time=current_time()-this->start_time;
            #endif
        }
    }
};

template<>
void JpegParser::parse_segment<JpegSegmentType::SOI>(){
    {}
}
template<>
void JpegParser::parse_segment<JpegSegmentType::EOI>(){
    this->parsing_done=true;
}
template<>
void JpegParser::parse_segment<JpegSegmentType::COM>(){
    const uint32_t segment_size=this->next_u16();
    
    const uint32_t segment_end_position=static_cast<uint32_t>(this->current_file_content_index)+segment_size-2;

    const uint32_t comment_length=segment_size-2;

    char* const comment_str=(char*)malloc(comment_length+1);
    comment_str[comment_length]=0;

    memcpy(comment_str,&this->file_contents[this->current_file_content_index],comment_length);

    image_data->image_file_metadata.file_comment=comment_str;

    this->current_file_content_index=segment_end_position;
}
template<>
void JpegParser::parse_segment<JpegSegmentType::DQT>(){
    const uint32_t segment_size=this->next_u16();
    const uint32_t segment_end_position=static_cast<uint32_t>(this->current_file_content_index)+segment_size-2;

    uint32_t segment_bytes_read=0;
    while(segment_bytes_read<segment_size-2){
        const uint8_t destination_and_precision=this->get_mem<uint8_t>();
        const uint8_t destination=LB_U8(destination_and_precision);
        const uint8_t precision=HB_U8(destination_and_precision);
        if (precision!=0)
            bail(-45, "jpeg quant table precision is not 0 - it is %d\n",precision);

        uint8_t table_entries[64];
        memcpy(table_entries,&this->file_contents[this->current_file_content_index],64);
        this->current_file_content_index+=64;

        segment_bytes_read+=65;

        for (int i=0; i<64; i++) {
            this->quant_tables[destination][i]=table_entries[i];
        }

    }

    this->current_file_content_index=segment_end_position;
}
template<>
void JpegParser::parse_segment<JpegSegmentType::DHT>(){
    const uint32_t segment_size=this->next_u16();
    const uint32_t segment_end_position=static_cast<uint32_t>(this->current_file_content_index)+segment_size-2;

    uint32_t segment_bytes_read=0;
    while(segment_bytes_read<segment_size-2){
        uint8_t table_index_and_class=this->get_mem<uint8_t>();

        const uint8_t table_index=LB_U8(table_index_and_class);
        const uint8_t table_class=HB_U8(table_index_and_class);

        HuffmanTable* target_table=NULL;
        switch (table_class) {
            case  0:
                target_table=&this->dc_coding_tables[table_index];
                break;
            case  1:
                target_table=&this->ac_coding_tables[table_index];
                break;
            default:
                exit(FATAL_UNEXPECTED_ERROR);
        }

        uint32_t total_num_values=0;
        uint8_t num_values_of_length[16];
        for(int i=0;i<16;i++){
            num_values_of_length[i]=this->file_contents[this->current_file_content_index++];

            total_num_values+=num_values_of_length[i];
        }

        segment_bytes_read+=17;

        HuffmanTable::VALUE_ values[260];
        for(uint32_t i=0;i<total_num_values;i++)
            values[i]=this->file_contents[this->current_file_content_index++];

        uint8_t value_code_lengths[260];
        memset(value_code_lengths,0,sizeof(value_code_lengths));

        uint32_t value_index=0;
        for (uint8_t code_length=0; code_length<16; code_length++) {
            for(uint32_t i=0;i<num_values_of_length[code_length];i++)
                value_code_lengths[value_index++]=code_length+1;
        }

        segment_bytes_read+=value_index;

        // destroy previous table, if there was one
        target_table->destroy();

        HuffmanTable::CodingTable_new(
            target_table,
            (int)total_num_values,
            value_code_lengths,
            values
        );
    }

    this->current_file_content_index=segment_end_position;
}
/// baseline encoding
template<>
void JpegParser::parse_segment<JpegSegmentType::SOF0>(){
    this->parse_sof<EncodingMethod::Baseline>();
}
/// progressive encoding
template<>
void JpegParser::parse_segment<JpegSegmentType::SOF2>(){
    this->parse_sof<EncodingMethod::Progressive>();
}
template<>
void JpegParser::parse_segment<JpegSegmentType::SOS>(){
    switch(this->encoding_method){
        case EncodingMethod::Baseline:
            this->parse_sos<EncodingMethod::Baseline>();
            break;
        case EncodingMethod::Progressive:
            this->parse_sos<EncodingMethod::Progressive>();
            break;
        case EncodingMethod::UNDEFINED:
            bail(FATAL_UNEXPECTED_ERROR,"this is a bug.");
    }
}

struct JpegParser_process_channel_argset{
    JpegParser* parser;
    uint8_t c;
    uint32_t block_range_start;
    uint32_t block_range_end;
    ImageData* image_data;
};
void* JpegParser_process_channel_pthread(struct JpegParser_process_channel_argset* args){
    args->parser->process_channel(args->c,args->block_range_start,args->block_range_end);
    return NULL;
}

void* ProcessIncomingScans_pthread(struct ProcessIncomingScan_Arguments* async_args){
    uint32_t scan_id_start=0;
    uint32_t total_num_scans=async_args->parser->image_components[1].num_scans;
    while(scan_id_start<total_num_scans){
        uint32_t scan_id_end=async_args->num_scans_parsed.load();

        uint32_t num_scans_to_process=scan_id_end-scan_id_start;
        if(num_scans_to_process){
            const uint8_t c=async_args->channel;

            async_args->parser->process_channel(c,scan_id_start,scan_id_end);

            scan_id_start=scan_id_end;
        }else{
            struct timespec sleeptime={.tv_sec=0,.tv_nsec=100000};
            nanosleep(&sleeptime, NULL);
        }
    }

    return NULL;
}

#include "jpeg/jpeg_ycbcr_to_rgb.cpp"

struct JpegParser_convert_colorspace_argset{
    JpegParser* parser;

    uint32_t scan_index_start;
    uint32_t scan_index_end;
};
void* JpegParser_convert_colorspace_pthread(struct JpegParser_convert_colorspace_argset* args){
    JpegParser_convert_colorspace(args->parser,args->scan_index_start,args->scan_index_end);
    return NULL;
}

void JpegParser::parse_file(){
    while (!parsing_done) {
        const JpegSegmentType next_header=JpegSegmentType(this->next_u16());
        
        switch (next_header) {
            case JpegSegmentType::SOI:
                this->parse_segment<JpegSegmentType::SOI>();
                break;
            case JpegSegmentType::EOI:
                this->parse_segment<JpegSegmentType::EOI>();
                break;

            case JpegSegmentType::COM:
                this->parse_segment<JpegSegmentType::COM>();
                break;

            // used by the JFIF standard
            case JpegSegmentType::APP0:
            // used by the EXIF standard
            case JpegSegmentType::APP1:
                this->skip_segment(next_header);
                break;

            // application-specific marker segments not commonly used, but may be present
            case JpegSegmentType::APP2:
            case JpegSegmentType::APP3:
            case JpegSegmentType::APP4:
            case JpegSegmentType::APP5:
            case JpegSegmentType::APP6:
            case JpegSegmentType::APP7:
            case JpegSegmentType::APP8:
            case JpegSegmentType::APP9:
            case JpegSegmentType::APP10:
            case JpegSegmentType::APP11:
            case JpegSegmentType::APP12:
            case JpegSegmentType::APP13:
            case JpegSegmentType::APP14:
            case JpegSegmentType::APP15:
                this->skip_segment(next_header);
                break;

            case JpegSegmentType::DQT:
                this->parse_segment<JpegSegmentType::DQT>();
                break;

            case JpegSegmentType::DHT:
                this->parse_segment<JpegSegmentType::DHT>();
                break;

            case JpegSegmentType::SOF0:
                this->parse_segment<JpegSegmentType::SOF0>();
                break;
            case JpegSegmentType::SOF2:
                this->parse_segment<JpegSegmentType::SOF2>();
                break;

            case JpegSegmentType::SOS:
                this->parse_segment<JpegSegmentType::SOS>();
                break;

            default:
                bail(-40,"unhandled segment %s ( %X ) \n",Image_jpeg_segment_type_name((JpegSegmentType)next_header),static_cast<uint32_t>(next_header));
        }
    }

    #ifdef DEBUG
        this->parse_end_time=current_time()-this->start_time;
    #endif

    if(parallel)
        for(uint8_t t=0;t<3;t++)
            pthread_join(async_scan_processors[t], NULL);

    if (!parallel) {
        for(uint8_t c=0;c<3;c++){
            this->process_channel(c,0,this->image_components[c].num_scans);
        }
    }

    #ifdef DEBUG
        this->process_end_time=current_time()-this->start_time;
    #endif
}
void JpegParser::convert_colorspace(){
    switch(this->color_space){
        case 0x123:
            {
                if(this->parallel){
                    struct JpegParser_convert_colorspace_argset* const thread_args=(struct JpegParser_convert_colorspace_argset*)malloc(JPEG_DECODE_NUM_THREADS*sizeof(struct JpegParser_convert_colorspace_argset));
                    pthread_t* const threads=(pthread_t*)malloc(JPEG_DECODE_NUM_THREADS*sizeof(pthread_t));

                    const uint32_t num_scans_per_thread=this->image_components[0].num_scans/JPEG_DECODE_NUM_THREADS;
                    for(uint32_t i=0;i<JPEG_DECODE_NUM_THREADS;i++){
                        thread_args[i].parser=this;
                        thread_args[i].scan_index_start=i*num_scans_per_thread;
                        thread_args[i].scan_index_end=(i+1)*num_scans_per_thread;
                    }
                    thread_args[JPEG_DECODE_NUM_THREADS-1].scan_index_end=this->image_components[0].num_scans;

                    for(uint32_t i=0;i<JPEG_DECODE_NUM_THREADS;i++){
                        if(pthread_create(&threads[i], NULL, (pthread_callback)JpegParser_convert_colorspace_pthread, &thread_args[i])!=0){
                            bail(-107,"failed to launch pthread\n");
                        }
                    }

                    for(uint32_t i=0;i<JPEG_DECODE_NUM_THREADS;i++){
                        if(pthread_join(threads[i],NULL)!=0){
                            bail(-108,"failed to join pthread\n");
                        }
                    }

                    free(thread_args);
                    free(threads);
                }else{
                    JpegParser_convert_colorspace(this,0,this->image_components[0].num_scans);
                }
            }
            break;

        default:
            bail(-65,"color space %3X other than YCbCr (component IDs 1,2,3) currently unimplemented",this->color_space);
    }

    #ifdef DEBUG
        this->convert_end_time=current_time()-this->start_time;
    #endif
}

ImageParseResult Image_read_jpeg(
    const char* const filepath,
    ImageData* const  image_data
){
    JpegParser parser{filepath,image_data,JPEG_DECODE_NUM_THREADS>1};

    parser.parse_file();

    // -- convert idct magnitude values to channel pixel values
    // then upsample channels to final resolution
    // and convert ycbcr to rgb

    image_data->interleaved=true;
    image_data->pixel_format=PIXEL_FORMAT_Ru8Gu8Bu8Au8;

    parser.convert_colorspace();

    // -- crop to real size

    parser.correct_image_size();

    // -- parsing done. free all resources

    parser.destroy();

    #ifdef DEBUG
        println(
            "decoded %s: parsed %.3fms processed %.3fms converted %.3fms",
            filepath,
            parser.parse_end_time*1000,
            parser.process_end_time*1000,
            parser.convert_end_time*1000
        );
    #endif

    return IMAGE_PARSE_RESULT_OK;
}

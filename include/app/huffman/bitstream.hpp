#pragma once

#include <cstdlib>
#include <inttypes.h>
#include <cstdint>
#include <cstdio>

enum BitStreamDirection{
    /// read bits from least to most significant bit in each byte
    BITSTREAM_DIRECTION_RIGHT_TO_LEFT,
    /// read bits from most to least significant bit in each byte
    BITSTREAM_DIRECTION_LEFT_TO_RIGHT,
};

template <BitStreamDirection DIRECTION,bool REMOVE_JPEG_BYTE_STUFFING>
class BitStream{
    public:
        uint8_t* data;
        uint64_t next_data_index;

        uint64_t buffer;
        uint64_t buffer_bits_filled;

    /**
    * @brief initialise stream
    * 
    * @param stream 
    * @param data 
    * @param direction
    */
    static void BitStream_new(BitStream* stream,void* const data);

    /**
    * @brief advance stream
    * 
    * @param stream 
    * @param n_bits 
    */
    [[gnu::always_inline,gnu::flatten]]
    static inline void BitStream_advance_unsafe(
        BitStream* const  stream,
        const uint8_t n_bits
    ){
        stream->buffer_bits_filled-=n_bits;
        switch(DIRECTION){
            case BITSTREAM_DIRECTION_LEFT_TO_RIGHT:
                stream->buffer<<=n_bits;
                break;
            case BITSTREAM_DIRECTION_RIGHT_TO_LEFT:
                stream->buffer>>=n_bits;
                break;
        }
    }

    /// advance stream by some bits
    ///
    /// may not skip more bits than are present in the buffer currently
    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    static inline void BitStream_advance(
        BitStream* const  stream,
        const uint8_t n_bits
    ){
        if (n_bits>stream->buffer_bits_filled) {
            fprintf(stderr, "bitstream advance by %d bits invalid with %" PRIu64 " current buffer length\n",n_bits,stream->buffer_bits_filled);
            exit(-50);
        }

        BitStream_advance_unsafe(stream, n_bits);
    }

    static inline void BitStream_fill_buffer(
        BitStream* const  stream
    );

    /// skip bits in stream
    ///
    /// number may be much larger than cache size
    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    static inline void BitStream_skip(
        BitStream* const  stream,
        const uint64_t n_bits
    ){
        if(n_bits>stream->buffer_bits_filled){
            uint64_t remaining_bits=n_bits-stream->buffer_bits_filled;
            stream->next_data_index+=remaining_bits/8;

            stream->buffer_bits_filled=0;
            stream->buffer=0;

            remaining_bits=remaining_bits%8;
            if(remaining_bits>0){
                BitStream_fill_buffer(stream);
                BitStream_advance_unsafe(stream, (uint8_t)remaining_bits);
            }
        }else{
            if(n_bits>stream->buffer_bits_filled){
                uint64_t bits_remaining=n_bits-stream->buffer_bits_filled;
                BitStream_advance_unsafe(stream, (uint8_t)stream->buffer_bits_filled);
                BitStream_fill_buffer(stream);
                BitStream_advance_unsafe(stream, (uint8_t)bits_remaining);
            }else{
                BitStream_advance_unsafe(stream, (uint8_t)n_bits);
            }
        }
    }

    /**
    * @brief ensure that the bitstream has at least n bits cached
    * this function will fill the internal cache if the cache does not already have sufficient number of bits
    * @param stream 
    * @param n_bits 
    */
    [[gnu::always_inline,gnu::flatten]]
    static inline void BitStream_ensure_filled(
        BitStream* const  stream,
        const uint8_t n_bits
    ){
        if(stream->buffer_bits_filled<n_bits){
            BitStream_fill_buffer(stream);
        }
    }

    [[gnu::always_inline,gnu::flatten]]
    static inline uint64_t BitStream_get_bits_unsafe(
        const BitStream* const  stream,
        uint8_t n_bits
    ){
        switch(DIRECTION){
            case BITSTREAM_DIRECTION_LEFT_TO_RIGHT:
                {
                    const uint64_t ret=stream->buffer>>(64-n_bits);
                    return ret;
                }
            case BITSTREAM_DIRECTION_RIGHT_TO_LEFT:
                {
                    const uint64_t ret=stream->buffer&mask_u64(n_bits);
                    return ret;
                }
        }
    }

    /**
    * @brief get next n bits from stream
    * n must not be larger than 57. the internal bit buffer is automatically filled if it was not big enough at function start.
    * this function does NOT advance the internal state, i.e. repeated calls to this function with the same arguments will yield the same result.
    * call BitStream_advance to manually advance the stream.
    * @param stream 
    * @param n_bits 
    * @return int 
    */
    [[gnu::always_inline,gnu::flatten]]
    static inline uint64_t BitStream_get_bits(
        BitStream* const  stream,
        const uint8_t n_bits
    ){
        BitStream_ensure_filled(stream, n_bits);

        const uint64_t ret=BitStream_get_bits_unsafe(stream, n_bits);

        return ret;
    }

    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    static inline uint64_t BitStream_get_bits_advance(
        BitStream* const  stream,
        const uint8_t n_bits
    ){
        const uint64_t res=BitStream_get_bits(stream, n_bits);
        BitStream_advance_unsafe(stream, n_bits);

        return res;
    }
};

template <BitStreamDirection DIR,bool REM_JPG_STUFF>
void BitStream<DIR,REM_JPG_STUFF>::BitStream_new(BitStream* stream,void* const data){
    stream->data=static_cast<uint8_t*>(data);
    stream->next_data_index=0;
    stream->buffer=0;
    stream->buffer_bits_filled=0;
}

/**
* @brief fill internal bit buffer (used for fast lookup)
* this function is called automatically (internally) when required
* @param stream 
*/
template <BitStreamDirection DIRECTION,bool REMOVE_JPEG_BYTE_STUFFING>
[[gnu::hot,gnu::flatten]]
inline void BitStream<DIRECTION,REMOVE_JPEG_BYTE_STUFFING>::BitStream_fill_buffer(
    BitStream* const  stream
){
    const uint64_t num_bytes_missing = (64-stream->buffer_bits_filled)/8;

    if constexpr(DIRECTION==BITSTREAM_DIRECTION_RIGHT_TO_LEFT){
        uint64_t new_bytes=0;
        for(uint64_t i=0; i<num_bytes_missing; i++){
            const uint64_t next_byte = stream->data[stream->next_data_index++];

            const uint64_t shift_by = i*8;
            new_bytes |= next_byte << shift_by;

            if constexpr(REMOVE_JPEG_BYTE_STUFFING)
                if(next_byte==0xFF && stream->data[stream->next_data_index]==0){
                    stream->next_data_index++;
                }
        }
        stream->buffer |= new_bytes << stream->buffer_bits_filled;
        stream->buffer_bits_filled += num_bytes_missing*8;
    }else if constexpr(DIRECTION==BITSTREAM_DIRECTION_LEFT_TO_RIGHT){
        uint64_t new_bytes=0;
        for(uint64_t i=0; i<num_bytes_missing; i++){
            const uint64_t next_byte = stream->data[stream->next_data_index++];

            const uint64_t shift_by = (7-i)*8;
            new_bytes |= next_byte << shift_by;

            if constexpr(REMOVE_JPEG_BYTE_STUFFING)
                if(next_byte==0xFF && stream->data[stream->next_data_index]==0){
                    stream->next_data_index++;
                }
        }
        stream->buffer |= new_bytes >> stream->buffer_bits_filled;
        stream->buffer_bits_filled += num_bytes_missing*8;
    }
}
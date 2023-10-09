#pragma once

#include <cstdlib>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

#include "app/macros.hpp"
#include "app/bit_util.hpp"
#include "app/error.hpp"

namespace bitStream {
enum Direction{
    /// read bits from least to most significant bit in each byte
    BITSTREAM_DIRECTION_RIGHT_TO_LEFT,
    /// read bits from most to least significant bit in each byte
    BITSTREAM_DIRECTION_LEFT_TO_RIGHT,
};

template <Direction DIRECTION,bool REMOVE_JPEG_BYTE_STUFFING>
class BitStream{
    public:
        uint8_t* data;
        uint64_t data_size;

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
    static void BitStream_new(BitStream* stream,void* const data,const uint64_t data_size)noexcept;

    /**
    * @brief advance stream
    * 
    * @param stream 
    * @param n_bits 
    */
    [[gnu::always_inline,gnu::flatten]]
    inline void advance_unsafe(
        const uint8_t n_bits
    )noexcept{
        this->buffer_bits_filled-=n_bits;
        switch(DIRECTION){
            case BITSTREAM_DIRECTION_LEFT_TO_RIGHT:
                this->buffer<<=n_bits;
                break;
            case BITSTREAM_DIRECTION_RIGHT_TO_LEFT:
                this->buffer>>=n_bits;
                break;
        }
    }

    /// advance stream by some bits
    ///
    /// may not skip more bits than are present in the buffer currently
    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    inline void advance(
        const uint8_t n_bits
    )noexcept{
        if (n_bits>this->buffer_bits_filled) {
            fprintf(stderr, "bitstream advance by %d bits invalid with %" PRIu64 " current buffer length\n",n_bits,this->buffer_bits_filled);
            exit(-50);
        }

        this->advance_unsafe(n_bits);
    }

    inline void fill_buffer()noexcept;

    /// skip bits in stream
    ///
    /// number may be much larger than cache size
    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    inline void skip(
        const uint64_t n_bits
    )noexcept{
        if(n_bits>this->buffer_bits_filled){
            uint64_t remaining_bits=n_bits-this->buffer_bits_filled;
            this->next_data_index+=remaining_bits/8;

            this->buffer_bits_filled=0;
            this->buffer=0;

            remaining_bits=remaining_bits%8;
            if(remaining_bits>0){
                this->fill_buffer();
                this->advance_unsafe((uint8_t)remaining_bits);
            }
        }else{
            if(n_bits>this->buffer_bits_filled){
                uint64_t bits_remaining=n_bits-this->buffer_bits_filled;
                this->advance_unsafe((uint8_t)this->buffer_bits_filled);
                this->fill_buffer();
                this->advance_unsafe((uint8_t)bits_remaining);
            }else{
                this->advance_unsafe((uint8_t)n_bits);
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
    inline void ensure_filled(
        const uint8_t n_bits
    )noexcept{
        if(this->buffer_bits_filled<n_bits){
            this->fill_buffer();
        }
    }

    template<typename T = uint64_t>
    [[gnu::always_inline,gnu::flatten]]
    inline T get_bits_unsafe(
        uint8_t n_bits
    )const noexcept{
        if constexpr(DIRECTION==BITSTREAM_DIRECTION_LEFT_TO_RIGHT){
            const uint64_t ret=this->buffer>>(64-n_bits);
            return static_cast<T>(ret);
        }else if constexpr(DIRECTION==BITSTREAM_DIRECTION_RIGHT_TO_LEFT){
            const uint64_t ret=this->buffer & bitUtil::get_mask<uint64_t>(n_bits);
            return static_cast<T>(ret);
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
    template<typename T = uint64_t>
    [[gnu::always_inline,gnu::flatten]]
    inline T get_bits(
        const uint8_t n_bits
    )noexcept{
        this->ensure_filled(n_bits);

        const T ret=this->get_bits_unsafe<T>(n_bits);

        return ret;
    }

    template<typename T = uint64_t>
    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    inline T get_bits_advance(
        const uint8_t n_bits
    )noexcept{
        const T res=this->get_bits<T>(n_bits);
        this->advance_unsafe(n_bits);

        return res;
    }
};

template <Direction DIR,bool REM_JPG_STUFF>
void BitStream<DIR,REM_JPG_STUFF>::BitStream_new(BitStream* stream,void* const data,const uint64_t data_size)noexcept{
    stream->data=static_cast<uint8_t*>(data);
    stream->data_size=data_size;
    stream->next_data_index=0;
    stream->buffer=0;
    stream->buffer_bits_filled=0;
}

/**
* @brief fill internal bit buffer (used for fast lookup)
* this function is called automatically (internally) when required
* @param stream 
*/
template <Direction DIRECTION,bool REMOVE_JPEG_BYTE_STUFFING>
[[gnu::hot,gnu::flatten]]
inline void BitStream<DIRECTION,REMOVE_JPEG_BYTE_STUFFING>::fill_buffer(
)noexcept{
    uint64_t num_bytes_missing = (64-this->buffer_bits_filled)/8;

    if(this->next_data_index+num_bytes_missing>this->data_size){
        num_bytes_missing=this->data_size-this->next_data_index;
    }

    if constexpr(DIRECTION==BITSTREAM_DIRECTION_RIGHT_TO_LEFT){
        uint64_t new_bytes=0;
        for(uint64_t i=0; i<num_bytes_missing; i++){
            const uint64_t next_byte = this->data[this->next_data_index++];

            const uint64_t shift_by = i*8;
            new_bytes |= next_byte << shift_by;

            if constexpr(REMOVE_JPEG_BYTE_STUFFING)
                if(next_byte==0xFF && this->data[this->next_data_index]==0){
                    this->next_data_index++;
                }
        }
        this->buffer |= new_bytes << this->buffer_bits_filled;
        this->buffer_bits_filled += num_bytes_missing*8;
    }else if constexpr(DIRECTION==BITSTREAM_DIRECTION_LEFT_TO_RIGHT){
        uint64_t new_bytes=0;
        for(uint64_t i=0; i<num_bytes_missing; i++){
            const uint64_t next_byte = this->data[this->next_data_index++];

            const uint64_t shift_by = (7-i)*8;
            new_bytes |= next_byte << shift_by;

            if constexpr(REMOVE_JPEG_BYTE_STUFFING)
                if(next_byte==0xFF && this->data[this->next_data_index]==0){
                    this->next_data_index++;
                }
        }
        this->buffer |= new_bytes >> this->buffer_bits_filled;
        this->buffer_bits_filled += num_bytes_missing*8;
    }
}
};

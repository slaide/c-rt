#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define mask_u32(n) ((1ul<<(uint32_t)(n))-1ul)
#define mask_u64(n) ((1ull<<(uint64_t)(n))-1ull)

static uint64_t MASKS_U64[64]={
    mask_u64(0),
    mask_u64(1),
    mask_u64(2),
    mask_u64(3),
    mask_u64(4),
    mask_u64(5),
    mask_u64(6),
    mask_u64(7),
    mask_u64(8),
    mask_u64(9),
    mask_u64(10),
    mask_u64(11),
    mask_u64(12),
    mask_u64(13),
    mask_u64(14),
    mask_u64(15),
    mask_u64(16),
    mask_u64(17),
    mask_u64(18),
    mask_u64(19),
    mask_u64(20),
    mask_u64(21),
    mask_u64(22),
    mask_u64(23),
    mask_u64(24),
    mask_u64(25),
    mask_u64(26),
    mask_u64(27),
    mask_u64(28),
    mask_u64(29),
    mask_u64(30),
    mask_u64(31),
    mask_u64(32),
    mask_u64(33),
    mask_u64(34),
    mask_u64(35),
    mask_u64(36),
    mask_u64(37),
    mask_u64(38),
    mask_u64(39),
    mask_u64(40),
    mask_u64(41),
    mask_u64(42),
    mask_u64(43),
    mask_u64(44),
    mask_u64(45),
    mask_u64(46),
    mask_u64(47),
    mask_u64(48),
    mask_u64(49),
    mask_u64(50),
    mask_u64(51),
    mask_u64(52),
    mask_u64(53),
    mask_u64(54),
    mask_u64(55),
    mask_u64(56),
    mask_u64(57),
    mask_u64(58),
    mask_u64(59),
    mask_u64(60),
    mask_u64(61),
    mask_u64(62),
    mask_u64(63),
};
static uint32_t MASKS_U32[32]={
    mask_u32(0),
    mask_u32(1),
    mask_u32(2),
    mask_u32(3),
    mask_u32(4),
    mask_u32(5),
    mask_u32(6),
    mask_u32(7),
    mask_u32(8),
    mask_u32(9),
    mask_u32(10),
    mask_u32(11),
    mask_u32(12),
    mask_u32(13),
    mask_u32(14),
    mask_u32(15),
    mask_u32(16),
    mask_u32(17),
    mask_u32(18),
    mask_u32(19),
    mask_u32(20),
    mask_u32(21),
    mask_u32(22),
    mask_u32(23),
    mask_u32(24),
    mask_u32(25),
    mask_u32(26),
    mask_u32(27),
    mask_u32(28),
    mask_u32(29),
    mask_u32(30),
    mask_u32(31)
};

#define get_mask_u32(N) (MASKS_U32[(N)])
#define get_mask_u64(N) (MASKS_U64[(N)])

struct LookupLeaf{
    uint8_t value;
    uint8_t len;
};
typedef struct HuffmanCodingTable{
    uint8_t max_code_length_bits;

    struct LookupLeaf* lookup_table;
}HuffmanCodingTable;

struct ParseLeaf{
    uint8_t value;
    uint8_t len;

    uint32_t code;
};
/**
 * @brief create a new huffman coding table at the target location based on the input values
 * 
 * @param table 
 * @param num_values_of_length 
 * @param total_num_values 
 * @param value_code_lengths 
 * @param values 
 */
void HuffmanCodingTable_new(
    HuffmanCodingTable* table,

    const uint8_t num_values_of_length[16],

    const uint32_t total_num_values,
    const uint8_t value_code_lengths[260],
    const uint8_t values[260]
);
void HuffmanCodingTable_destroy(HuffmanCodingTable* table);

typedef struct BitStream{
    uint8_t* data;
    uint64_t next_data_index;

    uint64_t buffer;
    uint64_t buffer_bits_filled;
}BitStream;
/**
 * @brief initialise stream
 * 
 * @param stream 
 * @param data 
 */
void BitStream_new(BitStream* stream,void* data);

/**
 * @brief get next n bits from stream
 * n must not be larger than 57. the internal bit buffer is automatically filled if it was not big enough at function start.
 * this function does NOT advance the internal state, i.e. repeated calls to this function with the same arguments will yield the same result.
 * call BitStream_advance to manually advance the stream.
 * @param stream 
 * @param n_bits 
 * @return int 
 */
/*FORCE_INLINE
static inline uint64_t BitStream_get_bits(BitStream* stream,uint8_t n_bits);

FORCE_INLINE
static inline void BitStream_advance_unsafe(BitStream* stream,uint8_t n_bits);*/
/**
 * @brief advance stream
 * 
 * @param stream 
 * @param n_bits 
 */
[[clang::always_inline,gnu::flatten]]
static inline void BitStream_advance(BitStream* const stream,const uint8_t n_bits);

/**
 * @brief fill internal bit buffer (used for fast lookup)
 * this function is called automatically (internally) when required
 * @param stream 
 */
[[clang::always_inline,gnu::flatten]]
static inline void BitStream_fill_buffer(BitStream* const stream){
    uint64_t num_bytes_missing=7-stream->buffer_bits_filled/8;

    stream->buffer=stream->buffer<<(8*num_bytes_missing);
    for(uint64_t i=0;i<num_bytes_missing;i++){
        uint64_t index=stream->next_data_index+i;
        uint64_t shift_by=(num_bytes_missing-1-i)*8;
        uint64_t next_byte=stream->data[index];
        stream->buffer |= next_byte << shift_by;
    }
    stream->buffer_bits_filled+=num_bytes_missing*8;
    stream->next_data_index+=num_bytes_missing;
}
/**
 * @brief ensure that the bitstream has at least n bits cached
 * this function will fill the internal cache if the cache does not already have sufficient number of bits
 * @param stream 
 * @param n_bits 
 */
[[clang::always_inline,gnu::flatten]]
static inline void BitStream_ensure_filled(BitStream* stream,uint8_t n_bits) {
    if(stream->buffer_bits_filled<n_bits){
        BitStream_fill_buffer(stream);
    }
}

[[clang::always_inline,gnu::flatten]]
static inline uint64_t BitStream_get_bits_unsafe(BitStream* stream,uint8_t n_bits){
    uint64_t ret=stream->buffer>>(stream->buffer_bits_filled-n_bits);
    return ret;
}

[[clang::always_inline,gnu::flatten]]
static inline uint64_t BitStream_get_bits(BitStream* const stream,const uint8_t n_bits){
    BitStream_ensure_filled(stream, n_bits);

    uint64_t ret=BitStream_get_bits_unsafe(stream, n_bits);

    return ret;
}

[[clang::always_inline,gnu::flatten]]
static inline void BitStream_advance_unsafe(BitStream* const stream,const uint8_t n_bits) {
    stream->buffer_bits_filled-=n_bits;
    stream->buffer&=get_mask_u64(stream->buffer_bits_filled);
}

[[clang::always_inline,gnu::flatten]]
static inline void BitStream_advance(BitStream* const stream,const uint8_t n_bits){
    if (n_bits>stream->buffer_bits_filled) {
        fprintf(stderr, "bitstream advance by %d bits invalid with %lu current buffer length\n",n_bits,stream->buffer_bits_filled);
        exit(-50);
    }

    BitStream_advance_unsafe(stream, n_bits);
}

[[clang::always_inline,gnu::flatten]]
static inline uint8_t HuffmanCodingTable_lookup(
    const HuffmanCodingTable* const table,
    BitStream* stream
){
    uint64_t bits=BitStream_get_bits(stream,table->max_code_length_bits);

    struct LookupLeaf leaf=table->lookup_table[bits];
    BitStream_advance_unsafe(stream, leaf.len);

    return leaf.value;
}

[[clang::always_inline,gnu::flatten]]
static inline uint64_t BitStream_get_bits_advance(BitStream* const stream,const uint8_t n_bits){
    uint64_t res=BitStream_get_bits(stream, n_bits);
    BitStream_advance_unsafe(stream, n_bits);

    return res;
}


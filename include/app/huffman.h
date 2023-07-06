#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct HuffmanCodingTable{
    uint32_t max_code_length_bits;

    int* value_lookup_table;
    uint32_t* code_length_lookup_table;
}HuffmanCodingTable;

struct LookupLeaf{
    uint8_t value;
    uint32_t len;
};
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

    uint8_t num_values_of_length[16],

    uint32_t total_num_values,
    uint8_t value_code_lengths[260],
    uint8_t values[260]
);

typedef struct BitStream{
    uint8_t* data;
    int next_data_index;

    uint64_t buffer;
    int buffer_bits_filled;
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
uint64_t BitStream_get_bits(BitStream* stream,uint8_t n_bits);

void BitStream_advance_unsafe(BitStream* stream,uint8_t n_bits);
/**
 * @brief advance stream
 * 
 * @param stream 
 * @param n_bits 
 */
void BitStream_advance(BitStream* stream,uint8_t n_bits);

/**
 * @brief ensure that the bitstream has at least n bits cached
 * this function will fill the internal cache if the cache does not already have sufficient number of bits
 * @param stream 
 * @param n_bits 
 */
void BitStream_ensure_filled(BitStream* stream,uint8_t n_bits);
/**
 * @brief fill internal bit buffer (used for fast lookup)
 * this function is called automatically (internally) when required
 * @param stream 
 */
void BitStream_fill_buffer(BitStream* stream);

uint64_t BitStream_get_bits_advance(BitStream* stream,uint8_t n_bits);
uint64_t BitStream_get_bits_advance_unsafe(BitStream* stream,uint8_t n_bits);

int HuffmanCodingTable_lookup(
    HuffmanCodingTable* table,
    BitStream* bit_stream
);
int HuffmanCodingTable_lookup_unsafe(
    HuffmanCodingTable* table,
    BitStream* bit_stream
);
uint32_t mask_u32(uint32_t n);
uint64_t mask_u64(uint64_t n);

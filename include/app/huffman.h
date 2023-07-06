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
    int value;
    uint32_t len;
};
struct ParseLeaf{
    bool present;

    int value;
    uint32_t len;

    uint32_t code;
    uint32_t rcode;
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

    int num_values_of_length[16],

    uint32_t total_num_values,
    uint32_t value_code_lengths[260],
    int values[260]
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
uint64_t BitStream_get_bits(BitStream* stream,int n_bits);
/**
 * @brief advance stream
 * 
 * @param stream 
 * @param n_bits 
 */
void BitStream_advance(BitStream* stream,int n_bits);

uint64_t BitStream_get_bits_advance(BitStream* stream,int n_bits);

int HuffmanCodingTable_lookup(
    HuffmanCodingTable* table,
    BitStream* bit_stream
);
uint32_t mask_u32(uint32_t n);
uint64_t mask_u64(uint64_t n);

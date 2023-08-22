#include "app/huffman.h"

/**
 * @brief format integer as binary number
 * 
 * @param v 
 * @param s 
 */
void integer_to_str(int v,char s[32]){
    const int num_bits=32;
    for (int i=0; i<num_bits; i++) {
        if ((v>>(num_bits-1-i))&1) {
            s[i]='1';
        }else {
            s[i]='0';
        }
    }
}
/**
 * @brief format integer as binary number
 * 
 * @param v 
 * @param s 
 */
void uint64_to_str(uint64_t v,char s[64]){
    const int num_bits=64;
    for (int i=0; i<num_bits; i++) {
        if ((v>>(num_bits-1-i))&1) {
            s[i]='1';
        }else {
            s[i]='0';
        }
    }
}

/**
 * @brief reverse a sequence of len bits
 * 
 * @param bits 
 * @param len 
 * @return int 
 */
[[maybe_unused]]
static inline int32_t reverse_bits(
    const int32_t bits,
    const int32_t len
){
    int32_t ret=0;
    for (int32_t i=0; i<len; i++) {
        int32_t nth_bit=(bits&(1<<i))>>i;
        ret|=nth_bit<<(len-1-i);
    }
    return ret;
}

#define MAX_HUFFMAN_TABLE_CODE_LENGTH 16
#define MAX_HUFFMAN_TABLE_ENTRIES 260

void HuffmanCodingTableRtL_new(
    HuffmanCodingTableRtL* const restrict table,

    const uint8_t num_values_of_length[MAX_HUFFMAN_TABLE_CODE_LENGTH],

    const uint32_t total_num_values,
    const uint8_t value_code_lengths[MAX_HUFFMAN_TABLE_ENTRIES],
    const uint8_t values[MAX_HUFFMAN_TABLE_ENTRIES]
){
    for (uint8_t i=0; i<MAX_HUFFMAN_TABLE_CODE_LENGTH; i++) {
        if (num_values_of_length[i]>0){
            table->max_code_length_bits=i+1;
        }
    }

    struct ParseLeaf parse_leafs[MAX_HUFFMAN_TABLE_ENTRIES];
    for (uint32_t i=0; i<total_num_values; i++) {
        parse_leafs[i].value=values[i];
        parse_leafs[i].len=value_code_lengths[i];

        parse_leafs[i].code=0;
    }

    uint32_t bl_count[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
    memset(bl_count,0,(MAX_HUFFMAN_TABLE_CODE_LENGTH+1)*4);
    for (uint32_t i=0; i<total_num_values; i++) {
        bl_count[parse_leafs[i].len]+=1;
    }

    uint32_t next_code[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
    memset(next_code,0,(MAX_HUFFMAN_TABLE_CODE_LENGTH+1)*4);
    for (uint32_t i=1; i<=table->max_code_length_bits; i++) {
        next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
    }

    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* current_leaf=&parse_leafs[i];
        current_leaf->code=next_code[current_leaf->len] & get_mask_u32(current_leaf->len);
        next_code[current_leaf->len]+=1;

        //current_leaf->rcode=reverse_bits(current_leaf->code,current_leaf->len);
    }

    uint32_t num_possible_leafs=1<<table->max_code_length_bits;
    table->lookup_table=malloc(num_possible_leafs*sizeof(struct LookupLeaf));
    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* leaf=&parse_leafs[i];

        uint32_t mask_len=table->max_code_length_bits - leaf->len;
        uint32_t mask=get_mask_u32(mask_len);

        for (uint32_t j=0; j<=mask; j++) {
            uint32_t leaf_index=(leaf->code<<mask_len)+j;
            
            table->lookup_table[leaf_index].value=leaf->value;
            table->lookup_table[leaf_index].len=leaf->len;
        }
    }
}
void HuffmanCodingTableRtL_destroy(HuffmanCodingTableRtL* table){
    if(table->lookup_table)
        free(table->lookup_table);
}

void BitStreamRtL_new(BitStream* stream,void* const data){
    stream->data=data;
    stream->next_data_index=0;
    stream->buffer=0;
    stream->buffer_bits_filled=0;
}

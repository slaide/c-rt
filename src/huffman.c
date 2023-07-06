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

uint32_t mask_u32(uint32_t n){
    if (n==0) return  0;
    uint32_t shift_by=32-n;
    uint32_t base=0xffffffff;
    uint32_t ret=base>>shift_by;

    return ret;
}
uint64_t mask_u64(uint64_t n){
    if (n==0) return  0;
    uint64_t shift_by=64-n;
    uint64_t base=0xffffffffffffffff;
    uint64_t ret=base>>shift_by;

    return ret;
}

/**
 * @brief reverse a sequence of len bits
 * 
 * @param bits 
 * @param len 
 * @return int 
 */
int reverse_bits(int bits,int len){
    int ret=0;
    for (int i=0; i<len; i++) {
        int nth_bit=(bits&(1<<i))>>i;
        ret|=nth_bit<<(len-1-i);
    }
    return ret;
}

int ParseLeaf_compare(const struct ParseLeaf* left, const struct ParseLeaf* right){
    if (left->rcode<right->rcode) {
        return -1;
    }else if(left->rcode>right->rcode) {
        return 1;
    }else {
        return 0;
    }
}

void HuffmanCodingTable_new(
    HuffmanCodingTable* table,

    int num_values_of_length[16],

    uint32_t total_num_values,
    uint32_t value_code_lengths[260],
    int values[260]
){
    for (int i=0; i<16; i++) {
        if (num_values_of_length[i]>0){
            table->max_code_length_bits=i+1;
        }
    }

    struct ParseLeaf parse_leafs[260];
    for (uint32_t i=0; i<260; i++) {
        if (i<total_num_values) {
            parse_leafs[i].present=true;

            parse_leafs[i].value=values[i];
            parse_leafs[i].len=value_code_lengths[i];

            parse_leafs[i].code=0;
            parse_leafs[i].rcode=0;
        }else {
            parse_leafs[i].present=false;
            
            parse_leafs[i].value=0;
            parse_leafs[i].len=0;

            parse_leafs[i].code=0;
            parse_leafs[i].rcode=0;
        }
    }

    uint32_t bl_count[17];
    memset(bl_count,0,17*4);
    for (uint32_t i=0; i<total_num_values; i++) {
        bl_count[parse_leafs[i].len]+=1;
    }

    uint32_t next_code[17];
    memset(next_code,0,17*4);
    for (uint32_t i=1; i<=table->max_code_length_bits; i++) {
        next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
    }

    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* current_leaf=&parse_leafs[i];
        current_leaf->code=next_code[current_leaf->len] & mask_u32(current_leaf->len);
        next_code[current_leaf->len]+=1;

        current_leaf->rcode=reverse_bits(current_leaf->code,current_leaf->len);
    }

    qsort(parse_leafs, total_num_values, sizeof(struct ParseLeaf), (int(*)(const void*,const void*))ParseLeaf_compare);

    uint32_t num_possible_leafs=1<<table->max_code_length_bits;
    table->value_lookup_table=malloc(num_possible_leafs*sizeof(int));
    table->code_length_lookup_table=malloc(num_possible_leafs*sizeof(uint32_t));
    for (uint32_t i=0; i<total_num_values; i++) {
        struct ParseLeaf* leaf=&parse_leafs[i];

        uint32_t mask_len=table->max_code_length_bits - leaf->len;
        uint32_t mask=mask_u32(mask_len);

        for (uint32_t j=0; j<=mask; j++) {
            uint32_t leaf_index=(leaf->code<<mask_len)+j;
            
            table->value_lookup_table[leaf_index]=leaf->value;
            table->code_length_lookup_table[leaf_index]=leaf->len;
        }
    }
}

void BitStream_new(BitStream* stream,void* data){
    stream->data=data;
    stream->next_data_index=0;
    stream->buffer=0;
    stream->buffer_bits_filled=0;
}
/**
 * @brief fill internal bit buffer (used for fast lookup)
 * this function is called automatically (internally) when required
 * @param stream 
 */
void BitStream_fill_buffer(BitStream* stream){
    uint64_t num_bytes_missing=7-stream->buffer_bits_filled/8;
    stream->buffer=stream->buffer<<(8*num_bytes_missing);

    for(uint64_t i=0;i<num_bytes_missing;i++){
        uint64_t index=stream->next_data_index+i;
        uint64_t shift_by=(num_bytes_missing-1-i)*8;

        uint64_t next_byte=stream->data[index];
        stream->buffer |=  next_byte << shift_by;
    }
    stream->buffer_bits_filled+=num_bytes_missing*8;
    stream->next_data_index+=num_bytes_missing;
}

uint64_t BitStream_get_bits(BitStream* stream,int n_bits){
    if (n_bits>stream->buffer_bits_filled) {
        BitStream_fill_buffer(stream);
    }

    uint64_t shift_by=stream->buffer_bits_filled-n_bits;
    uint64_t res=stream->buffer>>shift_by;
    return res;
}

void BitStream_advance(BitStream* stream,int n_bits){
    if (n_bits>stream->buffer_bits_filled) {
        fprintf(stderr, "bitstream advance by %d bits invalid with %d current buffer length\n",n_bits,stream->buffer_bits_filled);
        exit(-50);
    }
    stream->buffer_bits_filled-=n_bits;
    uint64_t mask=mask_u64(stream->buffer_bits_filled);
    stream->buffer&=mask;
}

uint64_t BitStream_get_bits_advance(BitStream* stream,int n_bits){
    uint64_t res=BitStream_get_bits(stream, n_bits);

    BitStream_advance(stream,n_bits);

    return res;
}

int HuffmanCodingTable_lookup(
    HuffmanCodingTable* table,
    BitStream* bit_stream
){
    uint64_t bits=BitStream_get_bits(bit_stream, table->max_code_length_bits);

    int value=table->value_lookup_table[bits];
    int code_length=table->code_length_lookup_table[bits];
    BitStream_advance(bit_stream, code_length);

    return value;
}

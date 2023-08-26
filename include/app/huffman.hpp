#pragma  once

#include <cstdlib>
#include <cstring>
#include <string>
#include <inttypes.h>
#include <cstdint>
#include <cstdio>
#include <cstdint>

#include "app.hpp"
#include "bitstream.hpp"
#include "bit_util.hpp"

namespace huffman{
template <
    typename VALUE,
    bool REVERSE_CODES,
    bitStream::Direction BITSTREAM_DIRECTION, 
    bool BITSTREAM_REMOVE_JPEG_BYTE_STUFFING
>
class CodingTable{
    public:
        typedef bitStream::BitStream<BITSTREAM_DIRECTION,BITSTREAM_REMOVE_JPEG_BYTE_STUFFING> BitStream_;
        typedef VALUE VALUE_;

        struct LookupLeaf{
            VALUE value;
            uint8_t len;
        };

        uint8_t max_code_length_bits;
        struct LookupLeaf* lookup_table;

    private:
    struct ParseLeaf{
        VALUE value;
        uint8_t len;

        uint32_t code;
    };

    #define MAX_HUFFMAN_TABLE_CODE_LENGTH 16
    #define MAX_HUFFMAN_TABLE_ENTRIES 260

    public:

    [[gnu::always_inline,gnu::flatten,maybe_unused]]
    inline VALUE lookup(
        BitStream_* const  stream
    )const noexcept{
        const uint64_t bits=stream->get_bits(this->max_code_length_bits);

        const struct LookupLeaf leaf=this->lookup_table[bits];
        stream->advance_unsafe(leaf.len);

        return leaf.value;
    }

    /**
    * @brief create a new huffman coding table at the target location based on the input values
    * 
    * @param table
    * @param value_code_lengths 
    * @param values 
    */
    static void CodingTable_new(
        CodingTable* const  table,

        uint8_t value_code_lengths[MAX_HUFFMAN_TABLE_ENTRIES],
        const VALUE values[MAX_HUFFMAN_TABLE_ENTRIES]
    ){
        uint8_t num_values_of_length[MAX_HUFFMAN_TABLE_CODE_LENGTH];
        for(int i=0;i<MAX_HUFFMAN_TABLE_CODE_LENGTH;i++)
            num_values_of_length[i]=0;

        int total_num_values=0;
        for(int i=0;i<MAX_HUFFMAN_TABLE_ENTRIES;i++){
            uint8_t code_len=value_code_lengths[i];
            if(code_len==0){
                break;
            }
            total_num_values++;
            num_values_of_length[code_len-1]++;
        }

        for (uint8_t i=0; i<MAX_HUFFMAN_TABLE_CODE_LENGTH; i++) {
            if (num_values_of_length[i]>0){
                table->max_code_length_bits=i+1;
            }
        }

        struct ParseLeaf parse_leafs[MAX_HUFFMAN_TABLE_ENTRIES];
        for (int i=0; i<total_num_values; i++) {
            parse_leafs[i].value=values[i];
            parse_leafs[i].len=value_code_lengths[i];

            parse_leafs[i].code=0;
        }

        uint32_t bl_count[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
        memset(bl_count,0,(MAX_HUFFMAN_TABLE_CODE_LENGTH+1)*4);
        for (int i=0; i<total_num_values; i++) {
            bl_count[parse_leafs[i].len]+=1;
        }

        uint32_t next_code[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
        memset(next_code,0,(MAX_HUFFMAN_TABLE_CODE_LENGTH+1)*4);
        for (uint32_t i=1; i<=table->max_code_length_bits; i++) {
            next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
        }

        for (int i=0; i<total_num_values; i++) {
            struct ParseLeaf* current_leaf=&parse_leafs[i];
            current_leaf->code=next_code[current_leaf->len] & bitUtil::get_mask_u32(current_leaf->len);
            next_code[current_leaf->len]+=1;

            //current_leaf->rcode=reverse_bits(current_leaf->code,current_leaf->len);
        }

        uint32_t num_possible_leafs=1<<table->max_code_length_bits;
        table->lookup_table=static_cast<struct LookupLeaf*>(malloc(num_possible_leafs*sizeof(struct LookupLeaf)));
        for (int i=0; i<total_num_values; i++) {
            struct ParseLeaf* leaf=&parse_leafs[i];

            uint32_t mask_len=table->max_code_length_bits - leaf->len;
            uint32_t mask=bitUtil::get_mask_u32(mask_len);

            for (uint32_t j=0; j<=mask; j++) {
                uint32_t leaf_index=(leaf->code<<mask_len)+j;
                
                table->lookup_table[leaf_index].value=leaf->value;
                table->lookup_table[leaf_index].len=leaf->len;
            }
        }
    }
    
    void destroy()noexcept{
        if(this->lookup_table){
            free(this->lookup_table);
            this->lookup_table=nullptr;
        }
    }
};
};

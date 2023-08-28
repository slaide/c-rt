#pragma  once

#include <cstdlib>
#include <cstring>
#include <string>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "app.hpp"
#include "bitstream.hpp"
#include "bit_util.hpp"
#include "error.hpp"

namespace huffman{
    template <
        typename VALUE,
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
            #define MAX_HUFFMAN_TABLE_ENTRIES 288

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

            int total_num_values,
            uint8_t unfiltered_value_code_lengths[MAX_HUFFMAN_TABLE_ENTRIES],
            const VALUE unfiltered_values[MAX_HUFFMAN_TABLE_ENTRIES]
        ){
            uint8_t value_code_lengths[MAX_HUFFMAN_TABLE_ENTRIES];
            VALUE values[MAX_HUFFMAN_TABLE_ENTRIES];
            uint8_t num_values_of_length[MAX_HUFFMAN_TABLE_CODE_LENGTH];

            int num_filtered_leafs=0;
            for(int i=0;i<total_num_values;i++){
                auto len=unfiltered_value_code_lengths[i];
                if(len==0)
                    continue;

                value_code_lengths[num_filtered_leafs]=len;
                values[num_filtered_leafs]=unfiltered_values[i];

                num_values_of_length[len-1]++;

                num_filtered_leafs++;
            }
            total_num_values=num_filtered_leafs;

            for(int i=total_num_values;i<MAX_HUFFMAN_TABLE_CODE_LENGTH;i++)
                num_values_of_length[i]=0;

            table->max_code_length_bits=0;

            struct ParseLeaf parse_leafs[MAX_HUFFMAN_TABLE_ENTRIES];
            for (int i=0; i<total_num_values; i++) {
                parse_leafs[i].value=values[i];
                parse_leafs[i].len=value_code_lengths[i];

                parse_leafs[i].code=0;

                if(parse_leafs[i].len>table->max_code_length_bits)
                    table->max_code_length_bits=parse_leafs[i].len;
            }

            uint32_t bl_count[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
            memset(bl_count,0,sizeof(bl_count));
            for (int i=0; i<total_num_values; i++) {
                bl_count[parse_leafs[i].len]++;
            }

            uint32_t next_code[MAX_HUFFMAN_TABLE_CODE_LENGTH+1];
            memset(next_code,0,(MAX_HUFFMAN_TABLE_CODE_LENGTH+1)*4);
            for (uint32_t i=1; i<=table->max_code_length_bits; i++) {
                next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
            }

            for (int i=0; i<total_num_values; i++) {
                struct ParseLeaf* current_leaf=&parse_leafs[i];
                current_leaf->code=next_code[current_leaf->len] & bitUtil::get_mask_u32(current_leaf->len);
                if constexpr(BITSTREAM_DIRECTION == bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT)
                    current_leaf->code=bitUtil::reverse_bits(current_leaf->code,current_leaf->len);
                next_code[current_leaf->len]+=1;
            }

            uint32_t num_possible_leafs=1<<table->max_code_length_bits;
            table->lookup_table=static_cast<struct LookupLeaf*>(malloc(num_possible_leafs*sizeof(struct LookupLeaf)));
            //memset(table->lookup_table,0,num_possible_leafs*sizeof(struct LookupLeaf));
            for (int i=0; i<total_num_values; i++) {
                struct ParseLeaf* leaf=&parse_leafs[i];

                uint32_t mask_len=table->max_code_length_bits - leaf->len;
                if(leaf->len>table->max_code_length_bits)
                    bail(FATAL_UNEXPECTED_ERROR,"this should not be possible %d > %d",leaf->len,table->max_code_length_bits);
                uint32_t mask=bitUtil::get_mask_u32(mask_len);

                for (uint32_t j=0; j<=mask; j++) {
                    uint32_t leaf_index;
                    if(BITSTREAM_DIRECTION==bitStream::BITSTREAM_DIRECTION_LEFT_TO_RIGHT)
                        leaf_index=(leaf->code<<mask_len)+j;
                    else
                        leaf_index=(j<<leaf->len)+leaf->code;
                    
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

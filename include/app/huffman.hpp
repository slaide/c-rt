#pragma  once

#include <cstdlib>
#include <cstring>
#include <string>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <span>

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

            typedef struct LookupLeaf{
                VALUE value=0;
                uint8_t len=0;
            }LookupLeaf;

            uint8_t max_code_length_bits;
            LookupLeaf* lookup_table;

            typedef struct ParseLeaf{
                VALUE value=0;
                int len=0;
                uint code=0;
            }ParseLeaf;

        private:
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
            CodingTable& table,

            std::size_t total_num_values,
            int unfiltered_value_code_lengths[MAX_HUFFMAN_TABLE_ENTRIES],
            const VALUE unfiltered_values[MAX_HUFFMAN_TABLE_ENTRIES]
        ){
            std::array<int,MAX_HUFFMAN_TABLE_ENTRIES> value_code_lengths;
            std::array<VALUE,MAX_HUFFMAN_TABLE_ENTRIES> values;
            std::array<int,MAX_HUFFMAN_TABLE_CODE_LENGTH> num_values_of_length;

            std::size_t num_filtered_leafs=0;
            for(std::size_t i=0;i<total_num_values;i++){
                auto len=unfiltered_value_code_lengths[i];
                if(len==0)
                    continue;

                value_code_lengths[num_filtered_leafs]=len;
                values[num_filtered_leafs]=unfiltered_values[i];

                num_values_of_length[static_cast<std::size_t>(len-1)]++;

                num_filtered_leafs++;
            }
            total_num_values=num_filtered_leafs;

            for(auto i=total_num_values;i<MAX_HUFFMAN_TABLE_CODE_LENGTH;i++)
                num_values_of_length[static_cast<std::size_t>(i)]=0;

            table.max_code_length_bits=0;

            struct ParseLeaf parse_leafs[MAX_HUFFMAN_TABLE_ENTRIES];
            for (std::size_t i=0; i<static_cast<std::size_t>(total_num_values); i++) {
                parse_leafs[i].value=values[i];
                parse_leafs[i].len=value_code_lengths[i];

                parse_leafs[i].code=0;

                if(parse_leafs[i].len>table.max_code_length_bits)
                    table.max_code_length_bits=static_cast<uint8_t>(parse_leafs[i].len);
            }

            std::array<uint,MAX_HUFFMAN_TABLE_CODE_LENGTH+1> bl_count={};
            for (std::size_t i=0; i<total_num_values; i++) {
                bl_count[static_cast<std::size_t>(parse_leafs[i].len)]++;
            }

            std::array<uint,MAX_HUFFMAN_TABLE_CODE_LENGTH+1> next_code={};
            for (std::size_t i=1; i<=static_cast<std::size_t>(table.max_code_length_bits); i++) {
                next_code[i]=(next_code[i-1]+bl_count[i-1])<<1;
            }

            for (std::size_t i=0; i<total_num_values; i++) {
                const auto current_leaf=parse_leafs+i;
                current_leaf->code=next_code[static_cast<std::size_t>(current_leaf->len)] & bitUtil::get_mask<uint>(current_leaf->len);
                next_code[static_cast<std::size_t>(current_leaf->len)]+=1;
            }

            construct_from_parseleafs(table,std::span{parse_leafs,total_num_values},table.max_code_length_bits);
        }

        static void construct_from_parseleafs(
            CodingTable& table,
            std::span<ParseLeaf> parse_leafs,
            uint8_t max_code_size
        ){
            table.max_code_length_bits=max_code_size;

            const int num_possible_leafs=1<<table.max_code_length_bits;
            table.lookup_table=new struct LookupLeaf[static_cast<std::size_t>(num_possible_leafs)];
            for (ParseLeaf& leaf : parse_leafs) {
                int mask_len=table.max_code_length_bits - leaf.len;
                if(leaf.len>table.max_code_length_bits)
                    bail(FATAL_UNEXPECTED_ERROR,"this should not be possible %d > %d",leaf.len,table.max_code_length_bits);
                
                const auto mask=bitUtil::get_mask<int>(mask_len);

                auto leaf_code=leaf.code;
                if constexpr(BITSTREAM_DIRECTION == bitStream::BITSTREAM_DIRECTION_RIGHT_TO_LEFT)
                    leaf_code=bitUtil::reverse_bits(leaf.code,leaf.len);

                for (int j=0; j<=mask; j++) {
                    int leaf_index;
                    if(BITSTREAM_DIRECTION==bitStream::BITSTREAM_DIRECTION_LEFT_TO_RIGHT)
                        leaf_index=static_cast<int>(leaf_code<<mask_len)+j;
                    else
                        leaf_index=(j<<static_cast<int>(leaf.len))+static_cast<int>(leaf_code);
                    
                    table.lookup_table[leaf_index].value=leaf.value;
                    table.lookup_table[leaf_index].len=static_cast<uint8_t>(leaf.len);
                }
            }
        }
        
    public:
        void destroy()noexcept{
            delete[] this->lookup_table;
            this->lookup_table=nullptr;
        }
    };
};

#pragma  once

#include <cstdint>
#include <cstdlib>

#ifdef VK_USE_PLATFORM_XCB_KHR
#include <x86intrin.h>
#endif

namespace bitUtil {

#define UINT32_1 ((uint32_t)1u)
#define UINT64_1 ((uint64_t)1u)
#define mask_u32(n) (((UINT32_1)<<(uint32_t)(n))-UINT32_1)
#define mask_u64(n) (((UINT64_1)<<(uint64_t)(n))-UINT64_1)

[[maybe_unused]]
static const uint64_t MASKS_U64[64]={
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
[[maybe_unused]]
static const uint32_t MASKS_U32[32]={
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

template <typename T>
static uint32_t get_mask_u32(T num_bits){
    return MASKS_U32[num_bits];
}
template <typename T>
static uint64_t get_mask_u64(T num_bits){
    return MASKS_U64[num_bits];
}

/**
* @brief format integer as binary number
* 
* @param v 
* @param s 
*/
template <typename T>
void int_to_str(T v,char s[sizeof(T)*8]){
    const int num_bits=sizeof(T)*8;
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
template <typename T>
void uint_to_str(T v,char s[sizeof(T)*8]){
    const int num_bits=sizeof(T)*8;
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
template <typename T, typename L>
[[maybe_unused]]
static inline T reverse_bits(
    const T bits,
    const L len
){
    T ret=0;
    for (L i=0; i<len; i++) {
        T nth_bit=(bits&(1<<i))>>i;
        ret|=nth_bit<<(len-1-(T)i);
    }
    return ret;
}

#ifdef VK_USE_PLATFORM_XCB_KHR
[[maybe_unused]]
static uint32_t tzcnt_32(const uint32_t v){
    #ifdef __clang__
        return (uint32_t)_mm_tzcnt_32(v);
    #elif defined( __GNUC__)
        return __builtin_ia32_lzcnt_u32(v);
    #endif
}
#endif

template<typename T>
static inline T byteswap(T v, uint8_t num_bytes){
    union B4{
        uint8_t bytes[sizeof(T)];
        T v;
    };
    union B4 arg={.v=v};
    union B4 ret={.v=0};

    switch(num_bytes){
        case 2:
            if constexpr(sizeof(T)>=2){
                ret.bytes[1]=arg.bytes[0];
                ret.bytes[0]=arg.bytes[1];
                break;
            }
        case 4:
            if constexpr(sizeof(T)>=4){
                ret.bytes[3]=arg.bytes[0];
                ret.bytes[2]=arg.bytes[1];
                ret.bytes[1]=arg.bytes[2];
                ret.bytes[0]=arg.bytes[3];
                break;
            }
        default:
            exit(-1);
    }
    return ret.v;
}

template <typename  T>
[[gnu::always_inline,gnu::pure,gnu::flatten,gnu::hot]]
static inline T max(const T a,const T b){
    return (a>b)?a:b;
}
template <typename  T>
[[gnu::always_inline,gnu::pure,gnu::flatten,gnu::hot]]
static inline T min(const T a,const T b){
    return (a<b)?a:b;
}

template <typename  T>
[[gnu::always_inline,gnu::pure,gnu::flatten,gnu::hot,maybe_unused]]
static inline T clamp(const T v_min,const T v_max,const T v){
    return max(v_min, min(v_max, v));
}

template <typename  T>
[[gnu::always_inline,gnu::pure,gnu::flatten,gnu::hot]]
static inline T twos_complement(const T magnitude, const T value){
    T threshold=(T)(1<<(magnitude-1));
    if (value<threshold){
        T ret=value+1;
        ret-=1<<magnitude;
        return ret;
    }

    return value;
}

};

#pragma  once

#include <cstdint>
#include <cstdlib>

#ifdef VK_USE_PLATFORM_XCB_KHR
#include <x86intrin.h>
#endif

namespace bitUtil {


template <typename M = uint32_t, typename T>
static M get_mask(T num_bits){
    return (1<<num_bits)-1;
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
        ret|=nth_bit<<((T)(len-1)-(T)i);
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

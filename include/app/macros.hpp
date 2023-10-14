#pragma once

#include <cstdio>

/// utility macro
#define discard (void)

#define fprintln(stream,...) { \
    fprintf(stream,"%s : %d | ",__FILE__,__LINE__); \
    fprintf(stream,__VA_ARGS__); \
    fprintf(stream,"\n"); \
}
#define println(...) { \
    fprintln(stdout,__VA_ARGS__); \
}
#define bail(ERROR_CODE,...) { \
    fprintln(stderr,"ERROR - bailed : " __VA_ARGS__); \
    exit(ERROR_CODE); \
}
#ifdef DEBUG
    #define BREAKPOINT { \
        raise(SIGTRAP); \
    }
#else
    #define BREAKPOINT
#endif

template<typename V,typename M>
[[gnu::always_inline,gnu::flatten]]
static const inline V ROUND_UP(V value,M multiple_of){
    V multiple_of_as_v=static_cast<V>(multiple_of);
    V ret=((value) + (multiple_of_as_v-1)) / multiple_of_as_v;
    return ret * multiple_of_as_v;
}

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
static const inline V ROUND_UP(V VALUE,M MULTIPLE_OF){
    V ret=((VALUE) + (((V)MULTIPLE_OF)-1)) / (V)MULTIPLE_OF;
    return ret * (V)MULTIPLE_OF;
}

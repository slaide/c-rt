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
    fprintln(stderr,__VA_ARGS__); \
    exit(ERROR_CODE); \
}
#ifdef DEBUG
    #define BREAKPOINT { \
        raise(SIGTRAP); \
    }
#else
    #define BREAKPOINT
#endif

template<typename T,typename M>
inline T ROUND_UP(const T value,const M multiple)noexcept{
    const T multiple_t=static_cast<T>(multiple);
    return  (((value) + (multiple_t-1)) / multiple_t) * multiple_t;
}

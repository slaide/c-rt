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

#define ROUND_UP(VALUE,MULTIPLE_OF) (((VALUE) + ((MULTIPLE_OF)-1)) / (MULTIPLE_OF)) * (MULTIPLE_OF)

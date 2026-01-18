// #ifndef QLIB_QINC_H
// #define QLIB_QINC_H

#if defined(_REAL_IMPL) || defined(EDITOR)
#ifdef func
#undef func
#endif
#define func(x) x
#else
#define func(x) ;
#endif

// #endif

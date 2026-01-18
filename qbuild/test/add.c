#ifdef IMPL
#undef IMPL
#endif

#include <stdbool.h>
// #include <stdio.h>

#ifdef IMPL
#define _REAL_IMPL
#endif
#include "../qinc.h"

int add(int a, int b, bool sub) func({
  // printf("added\n");
  if (!sub)
    return a + b;
  else
    return a - b;
})

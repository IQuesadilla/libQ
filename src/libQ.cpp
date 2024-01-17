#include "libQ.h"

uint strlen(const char *str)
{
    uint n = 0;
    while (str[n] != '\0') ++n;
    return n;
}
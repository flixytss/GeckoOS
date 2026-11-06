#include "sys/types.h"

i64 clamp(i64 n, i64 min, i64 max)
    { return n > min ? (n < max ? n : max) : min; }
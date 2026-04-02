#ifndef S390_VX_INTRINS_H
#define S390_VX_INTRINS_H

#include <vecintrin.h>

typedef unsigned char uv16qi __attribute__((vector_size(16)));
typedef unsigned short uv8hi __attribute__((vector_size(16)));
typedef unsigned int uv4si __attribute__((vector_size(16)));
typedef unsigned long long uv2di __attribute__((vector_size(16)));

#ifndef vec_sub
#define vec_sub(a, b) ((a) - (b))
#endif
#ifndef vec_subs
static inline uv8hi vec_subs_u16(uv8hi a, uv8hi b) {
    return a - vec_min(a, b);
}
#define vec_subs(a, b) vec_subs_u16((a), (b))
#endif

#endif

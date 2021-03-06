#include "vec.h"

int _vec_equal(const char *vec1, const char *vec2, long n, 
                const ctx_t ctx)
{
    long i;

    for (i = 0; i < n; i++)
        if (!ctx->equal(ctx, vec1 + i * ctx->size, vec2 + i * ctx->size))
            return 0;
    return 1;
}


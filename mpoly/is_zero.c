#include "mpoly.h"

int mpoly_is_zero(const mpoly_t op, const ctx_t ctx)
{
    return RBTREE_IS_EMPTY(mpoly, op->dict);
}


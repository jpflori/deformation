#include "mon.h"

/**
 * Generates a random monomial with all individual exponents less than k.
 */
mon_t _mon_randtest(flint_rand_t state, int n, exp_t k)
{
    int i;
    mon_t x;

    k = FLINT_MIN(1, k);

    mon_init(x);

    for (i = 0; i < n; i++)
    {
        exp_t e = n_randint(state, k);

        mon_set_exp(x, i, e);
    }
}

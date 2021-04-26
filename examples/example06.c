#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>

#include "flint/flint.h"
#include "flint/fmpz.h"
#include "flint/ulong_extras.h"

#include "mpoly.h"
#include "mat.h"
#include "gmconnection.h"
#include "deformation.h"

#include "flint/padic_mat.h"

int
main(void)
{
    int i, result;
    flint_rand_t state;

    _randinit(state);

    /*
        A generic sextic curve.
     */

    {
        const char *str = 
            "3  [6 0 0] [0 6 0] [0 0 6] "
            "(2  0 -1)[5 1 0] (2  0 7)[5 0 1] (2  0 2)[1 5 0] (2  0 1)[0 5 1] (2  0 2)[1 0 5] (2  0 1)[0 1 5] "
            "(2  0 2)[4 2 0] (2  0 2)[4 0 2] (2  0 3)[2 4 0] (2  0 1)[0 4 2] (2  0 3)[2 0 4] (2  0 1)[0 2 4] "
            "(2  0 3)[4 1 1] (2  0 3)[1 4 1] (2  0 1)[1 1 4] "
            "(2  0 -1)[3 3 0] (2  0 -2)[3 0 3] (2  0 4)[0 3 3] "
            "(2  0 2)[3 2 1] (2  0 1)[3 1 2] (2  0 -1)[2 3 1] (2  0 1)[1 3 2] (2  0 2)[2 1 3] (2  0 1)[1 2 3] "
            "(2  0 1)[2 2 2]";
        const long n = atoi(str) - 1;

        mpoly_t P;
        ctx_t ctxFracQt;
        qadic_ctx_t Qq;
        fmpz_t p  = {5L};
        long d    = 1;
        qadic_t t1;
        prec_t prec, prec_in;

        ctx_init_fmpz_poly_q(ctxFracQt);
        qadic_ctx_init_conway(Qq, p, d, 1, 1, "X", PADIC_SERIES);

        qadic_init2(t1, 1);
        qadic_set_ui(t1, 2, Qq);

        mpoly_init(P, n + 1, ctxFracQt);
        mpoly_set_str(P, str, ctxFracQt);

        frob(P, ctxFracQt, t1, Qq, &prec, NULL, 1);

        qadic_clear(t1);
        mpoly_clear(P, ctxFracQt);
        ctx_clear(ctxFracQt);
        qadic_ctx_clear(Qq);
    }

    _randclear(state);
    _fmpz_cleanup();
    return EXIT_SUCCESS;
}


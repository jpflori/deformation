#include <stdio.h>
#include <stdlib.h>
#include <mpir.h>

#include "flint.h"
#include "fmpz.h"
#include "ulong_extras.h"

#include "mpoly.h"
#include "mat.h"
#include "gmconnection.h"
#include "deformation.h"

#include "padic_mat.h"

int
main(void)
{
    int i, result;
    flint_rand_t state;

    _randinit(state);

    {
        const char *str = 
            "4  [3 0 0 0] [0 3 0 0] [0 0 3 0] [0 0 0 3] (2  0 4)[1 1 1 0]";
        const long n = atoi(str) - 1;

        mpoly_t P;
        ctx_t ctxFracQt;
        qadic_ctx_t Qq;
        fmpz_t p  = {2L};
        long d    = 1;
        fmpz *t1;
        prec_t prec, prec_in;

        t1 = _fmpz_vec_init(d);
        t1[0] = 1L;

        ctx_init_fmpz_poly_q(ctxFracQt);
        qadic_ctx_init_conway(Qq, p, d, 1, "X", PADIC_SERIES);

        mpoly_init(P, n + 1, ctxFracQt);
        mpoly_set_str(P, str, ctxFracQt);

        frob(P, ctxFracQt, t1, Qq, &prec, NULL, 1);

        _fmpz_vec_clear(t1, d);
        mpoly_clear(P, ctxFracQt);
        ctx_clear(ctxFracQt);
        qadic_ctx_clear(Qq);
    }

    _randclear(state);
    _fmpz_cleanup();
    return EXIT_SUCCESS;
}

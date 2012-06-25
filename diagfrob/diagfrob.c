/******************************************************************************

    Copyright (C) 2012 Sebastian Pancratz

******************************************************************************/

#include <time.h>

#include "gmconnection.h"
#include "diagfrob.h"

/*
    Computes the expression 
    \[
    \mu_m = 
        \sum_{k=0}^{\floor{m/2}} 2^{\floor{3m/4}-\nu_m-k} \frac{1}{(m-2k)! k!}
    \]
    modulo $p^N$ in the case that $p = 2$.

    Assumptions:

        * $m \geq 0$
        * $3 m$ fits into a signed long
        * The computation does fit into the MPIR model; 
          in particular $m!$ and $2^{\floor{3m/4}}$ have 
          to fit into a signed int
 */

void mu_2(fmpz_t rop, long m, long N)
{
    const fmpz_t P = {2L};

    if (m == 3)  /* 4/3 */
    {
        if (2 >= N)
            fmpz_zero(rop);
        else
        {
            fmpz_set_ui(rop, 3);
            _padic_inv(rop, rop, P, N - 2);
            fmpz_mul_2exp(rop, rop, 2);
        }
    }
    else if (m == 7)  /* 232/315 = 8*29/315 */
    {
        if (3 >= N)
            fmpz_zero(rop);
        else
        {
            fmpz_set_ui(rop, 315);
            _padic_inv(rop, rop, P, N - 3);
            fmpz_mul_ui(rop, rop, 29);
            fmpz_fdiv_r_2exp(rop, rop, N - 3);
            fmpz_mul_2exp(rop, rop, 3);
        }
    }
    else
    {
        fmpz_t f, s, t;
        long k, v;

        fmpz_init(f);
        fmpz_init(s);
        fmpz_init(t);

        fmpz_fac_ui(f, m);
        fmpz_one(s);
        fmpz_one(t);

        for (k = 0; k < m / 2; k++)
        {
            ulong h = ((m - 2 * k - 1) * (m - 2 * k)) / 2;

            fmpz_mul_ui(t, t, h);
            fmpz_divexact_si(t, t, (k + 1));
            fmpz_add(s, s, t);
        }

        /*
            Now we have that 
                f = m!
                s = \sum_{k=0}^{\floor{m/2}} 
                        2^{-k} \frac{m!}{(m-pk)! k!}
         */

        v  = fmpz_remove(s, s, P) - fmpz_remove(f, f, P) + (3*m)/4;

        if (v >= N)
        {
            fmpz_zero(rop);
        }
        else
        {
            _padic_inv(f, f, P, N - v);
            fmpz_mul(rop, f, s);
            fmpz_fdiv_r_2exp(rop, rop, N - v);
            fmpz_mul_2exp(rop, rop, v);
        }

        fmpz_clear(f);
        fmpz_clear(s);
        fmpz_clear(t);
    }
}

/*
    Computes the expression 
    \[
    \mu_m = \sum_{k=0}^{\floor{m/p}} p^{\floor{m/p} - k} \frac{1}{(m-pk)! k!}
    \]
    modulo $p^N$ in the case that $p > 2$ is an odd prime.

    The restriction that $p > 2$ is crucial as otherwise 
    the expression is not $p$-adically integral.

    Assumptions:

        * $m \geq 0$
        * The computation does fit into the MPIR model
 */

void mu_p(fmpz_t rop, long m, long p, long N)
{
    fmpz_t f, s, t, P;
    long k, v;

    fmpz_init(f);
    fmpz_init(s);
    fmpz_init(t);
    fmpz_init(P);

    fmpz_set_si(P, p);

    fmpz_pow_ui(t, P, m / p);
    fmpz_set(s, t);

    for (k = 0; k < m / p; k++)
    {
        fmpz_rfac_uiui(f, m - p*k - (p-1), p);
        fmpz_mul(t, t, f);
        fmpz_divexact_si(t, t, p * (k + 1));
        fmpz_add(s, s, t);
    }

    fmpz_fac_ui(f, m);

    /*
        Now we have that 
            f = m!
            s = \sum_{k=0}^{\floor{m/p}} 
                    p^{\floor{m/p}-k} \frac{m!}{(m-pk)! k!}
            P = p
     */

    v = fmpz_remove(s, s, P) - fmpz_remove(f, f, P);

    if (v >= N)
    {
        fmpz_zero(rop);
    }
    else
    {
        _padic_inv(f, f, P, N - v);
        fmpz_pow_ui(t, P, N - v);
        fmpz_mul(rop, f, s);
        fmpz_mod(rop, rop, t);
        fmpz_pow_ui(t, P, v);
        fmpz_mul(rop, rop, t);
    }

    fmpz_clear(f);
    fmpz_clear(s);
    fmpz_clear(t);
    fmpz_clear(P);
}

void mu(fmpz_t rop, long m, long p, long N)
{
    if (p == 2)
        mu_2(rop, m, N);
    else
        mu_p(rop, m, p, N);
}

/*
    Computes the list $\mu_0, \dotsc, \mu_M$ modulo $p^N$.
 */

void precompute_mu(fmpz *list, long M, long p, long N)
{
    long m;

    for (m = 0; m <= M; m++)
        mu(list + m, m, p, N);
}

/*
    Let $R = \floor{M/p}$.  This functions computes the list of 
    powers of inverses $(1/d)^r$ modulo $p^N$ for $r = 0, \dotsc, R$.

    Assumptions:

        * $p$ prime
        * $p \nmid d$
        * $N \geq 1$
        * $M \geq 0$
        * The list has to be allocated to (at least) the correct length
 */

void precompute_dinv_2(fmpz *list, long M, long d, long N)
{
    const fmpz_t P = {2L};

    fmpz_one(list + 0);

    if (M >= 2)
    {
        long r;

        fmpz_set_ui(list + 1, d);
        _padic_inv(list + 1, list + 1, P, N);

        for (r = 2; r <= M / 2; r++)
        {
            fmpz_mul(list + r, list + (r - 1), list + 1);
            fmpz_fdiv_r_2exp(list + r, list + r, N);
        }
    }
}

void precompute_dinv_p(fmpz *list, long M, long d, long p, long N)
{
    fmpz_one(list + 0);

    if (M >= p)
    {
        long r;
        fmpz_t P, PN;

        fmpz_init(P);
        fmpz_init(PN);

        fmpz_set_ui(P, p);
        fmpz_pow_ui(PN, P, N);

        fmpz_set_ui(list + 1, d);
        _padic_inv(list + 1, list + 1, P, N);

        for (r = 2; r <= M / p; r++)
        {
            fmpz_mul(list + r, list + (r - 1), list + 1);
            fmpz_mod(list + r, list + r, PN);
        }

        fmpz_clear(P);
        fmpz_clear(PN);
    }
}

void precompute_dinv(fmpz *list, long M, long d, long p, long N)
{
    if (p == 2)
        precompute_dinv_2(list, M, d, N);
    else
        precompute_dinv_p(list, M, d, p, N);
}

/*
    Computes the double sum involved in the expression for 
    $\alpha_{u+1, v+1}$ when $p = 2$, namely
    \[
    \sum_{m,r} 
        \Bigl(\frac{u_i + 1}{d}\Bigr)_r 2^{- \floor{(m+1)/4} + \nu} \mu_m.
    \]

    Assumptions:

        * $p d$ fits into a signed long
        * With $u = u_i + 1$, u (u + d) \dotsm (u + 4d)$ fits into 
          a signed long, which is guaranteed whenever $(5d)^5$ fits into 
          a signed long
        * With $u = u_i + 1$, $(u + (M - 1) d) (u + M d)$ fits into 
          a signed long, which is guaranteed whenever $((M + 1)d)^2$ 
          fits into a signed long
 */

void dsum_2(
    fmpz_t rop, const fmpz *dinv, const fmpz *mu, 
    long ui, long vi, long M, long n, long d, long N)
{
    const fmpz_t P = {2L};
    const long m0  = (2 * (ui + 1) - (vi + 1)) / d;
    const long u   = ui + 1;

    long m, r;
    fmpz_t f0, f1, f2, g;

    fmpz_init(f0);
    fmpz_init(f1);
    fmpz_init(f2);
    fmpz_init(g);

    fmpz_zero(rop);

    for (m = m0; m <= M; m += 2)
    {
        r = m / 2;

        switch (r)
        {
          case 0:
            fmpz_one(f2);
            break;
          case 1:
            fmpz_one(f1);
            fmpz_set_ui(f2, u);
            break;
          case 5:
            fmpz_swap(f1, f2);
            fmpz_set_ui(f2, (u * (u + d) * (u + 2*d) * (u + 3*d) * (u + 4*d)) 
                / ((m0 == 0) ? 4 : 8));
            break;
          default:
            fmpz_swap(f0, f1);
            fmpz_swap(f1, f2);
            fmpz_mul_ui(f2, f0, ((u + (r-2)*d) * (u + (r-1)*d)) / 2);
            fmpz_fdiv_r_2exp(f2, f2, N);
        }

        /*
            g = f_{r} d^{-r} \mu_m
              = f2 * dinv[r] * mu[m]
         */

        fmpz_mul(g, f2, dinv + r);
        fmpz_fdiv_r_2exp(g, g, N);
        fmpz_mul(g, g, mu + m);
        fmpz_fdiv_r_2exp(g, g, N);

        fmpz_add(rop, rop, g);
    }

    fmpz_fdiv_r_2exp(rop, rop, N);

    fmpz_clear(f0);
    fmpz_clear(f1);
    fmpz_clear(f2);
    fmpz_clear(g);
}

/*
    Computes the double sum involved in the expression for 
    $\alpha_{u+1, v+1}$ when $p$ is an odd prime, namely
    \[
    \sum_{m,r} \Bigl(\frac{u_i + 1}{d}\Bigr)_r p^{r - \floor{m/p}} \mu_m.
    \]

    Assumptions:

        * $p d$ fits into a signed long
        * $\floor{M/p} * d$ fits into a signed long
 */

void dsum_p(
    fmpz_t rop, const fmpz *dinv, const fmpz *mu, 
    long ui, long vi, long M, long n, long d, long p, long N)
{
    long m, r;
    fmpz_t f, g, h, P, PN;

    fmpz_init(f);
    fmpz_init(g);
    fmpz_init(h);
    fmpz_init_set_ui(P, p);
    fmpz_init(PN);

    fmpz_pow_ui(PN, P, N);

    fmpz_zero(rop);

    r = 0;
    m = (p * (ui + 1) - (vi + 1)) / d;

    if (m <= M)
    {
        fmpz_pow_ui(h, P, r - (m / p));
        fmpz_mul(g, h, mu + m);
        fmpz_mod(g, g, PN);
        fmpz_add(rop, rop, g);
    }

    fmpz_one(f);

    for (r = 1, m += p; m <= M; r++, m += p)
    {
        fmpz_mul_ui(f, f, ui + 1 + (r - 1) * d);
        fmpz_mul(g, f, dinv + r);
        fmpz_pow_ui(h, P, r - (m / p));
        fmpz_mul(g, g, h);
        fmpz_mul(g, g, mu + m);
        fmpz_mod(g, g, PN);
        fmpz_add(rop, rop, g);
    }

    fmpz_mod(rop, rop, PN);

    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(h);
    fmpz_clear(P);
    fmpz_clear(PN);
}

void dsum(
    fmpz_t rop, const fmpz *dinv, const fmpz *mu, 
    long ui, long vi, long M, long n, long d, long p, long N)
{
    if (p == 2)
        dsum_2(rop, dinv, mu, ui, vi, M, n, d, N);
    else
        dsum_p(rop, dinv, mu, ui, vi, M, n, d, p, N);
}

/*
    Computes $\alpha_{u+1,v+1}$ modulo $p^N$ in the case when $p = 2$.
 */

void alpha_2(fmpz_t rop, const long *u, const long *v, 
    const fmpz *dinv, const fmpz *mu, long M, 
    long n, long d, long N)
{
    const fmpz_t P = {2L};

    long i, ud;
    fmpz_t g;

    ud = n + 1;
    for (i = 0; i <= n; i++)
        ud += u[i];
    ud /= d;

    fmpz_init(g);

    fmpz_zero(rop);
    fmpz_setbit(rop, ud);

    for (i = 0; i <= n; i++)
    {
        dsum(g, dinv, mu, u[i], v[i], M, n, d, 2, N);
        fmpz_mul(rop, rop, g);
        fmpz_fdiv_r_2exp(rop, rop, N);
    }

    if (ud % 2 != 0 && !fmpz_is_zero(rop))
    {
        fmpz_zero(g);
        fmpz_setbit(g, N);
        fmpz_sub(rop, g, rop);
    }

    fmpz_clear(g);
}

/*
    Computes $\alpha_{u+1,v+1}$ modulo $p^N$ when $p > 2$ is an odd prime.
 */

void alpha_p(fmpz_t rop, const long *u, const long *v, 
    const fmpz *a, const fmpz *dinv, const fmpz *mu, long M, 
    long n, long d, long p, long N)
{
    long e, i, ud;
    fmpz_t f, g, P, PN;

    ud = n + 1;
    for (i = 0; i <= n; i++)
        ud += u[i];
    ud /= d;

    fmpz_init(f);
    fmpz_init(g);
    fmpz_init_set_ui(P, p);
    fmpz_init(PN);

    fmpz_pow_ui(PN, P, N);

    fmpz_pow_ui(rop, P, ud);

    for (i = 0; i <= n; i++)
    {
        e = (p * (u[i] + 1) - (v[i] + 1)) / d;
        fmpz_powm_ui(f, a + i, e, PN);
        dsum(g, dinv, mu, u[i], v[i], M, n, d, p, N);
        fmpz_mul(rop, rop, f);
        fmpz_mul(rop, rop, g);
        fmpz_mod(rop, rop, PN);
    }

    if (ud % 2 != 0 && !fmpz_is_zero(rop))
    {
        fmpz_sub(rop, PN, rop);
    }

    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(P);
    fmpz_clear(PN);
}

void alpha(fmpz_t rop, const long *u, const long *v, 
    const fmpz *a, const fmpz *dinv, const fmpz *mu, long M, 
    long n, long d, long p, long N)
{
    if (p == 2)
        alpha_2(rop, u, v, dinv, mu, M, n, d, N);
    else
        alpha_p(rop, u, v, a, dinv, mu, M, n, d, p, N);
}

void entry(padic_t rop, const long *u, const long *v, 
    const fmpz *a, const fmpz *dinv, const fmpz *mu, long M, long C, 
    long n, long d, long p, long N)
{
    long i, ud, vd;
    fmpz_t f, g, h, P;

    fmpz_init(f);
    fmpz_init(g);
    fmpz_init(h);
    fmpz_init_set_ui(P, p);

    /*
        Compute $f := (-1)^{u'+v'} (v'-1)! p^n$ exactly.
     */

    ud = n + 1;
    for (i = 0; i <= n; i++)
        ud += u[i];
    ud /= d;

    vd = n + 1;
    for (i = 0; i <= n; i++)
        vd += v[i];
    vd /= d;

    fmpz_fac_ui(f, vd - 1);
    fmpz_pow_ui(h, P, n);
    fmpz_mul(f, f, h);
    if ((ud + vd) % 2 != 0)
    {
        fmpz_neg(f, f);
    }

    /*
        Compute $g := (u'-1)! \alpha_{u+1,v+1}$ to precision $N - n + 2 C$.
     */

    fmpz_fac_ui(g, ud - 1);
    alpha(h, u, v, a, dinv, mu, M, n, d, p, N - n + 2 * C);
    fmpz_mul(g, g, h);

    /*
        Set rop to the product of $f$ and $g^{-1}$.
     */

    padic_val(rop) = fmpz_remove(f, f, P) - fmpz_remove(g, g, P);

    if (padic_val(rop) >= N)
    {
        padic_zero(rop);
    }
    else
    {
        _padic_inv(g, g, P, N - padic_val(rop));

        fmpz_mul(padic_unit(rop), f, g);
        fmpz_pow_ui(h, P, N - padic_val(rop));
        fmpz_mod(padic_unit(rop), padic_unit(rop), h);
    }

    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(h);
    fmpz_clear(P);
}

void diagfrob(mat_t F, const fmpz *a, long n, long d, const ctx_t ctx, 
              int verbose)
{
    const fmpz *P = ctx->pctx->p;
    const long p  = fmpz_get_si(P);
    const long N  = ctx->pctx->N;

    /*
        $C$ is such that $\ord_p((u'-1)! \alpha_{u+1,v+1}) \leq C$.
     */

    const long C  = n + 2 * padic_val_fac_ui(n - 1, P) + (n + 1) * n_flog(n - 1, p);
    const long N2 = N - n + 2 * C;
    const long M  =  (p * p * N2) / (p-1) + p * p * n_flog(N2 / (p-1) + 2, p) 
                     + p * p * 4;

    mon_t *B;
    long *iB, lenB, lo, hi;

    long i, j;

    fmpz *alift, *dinv, *mu;

    long *u, *v;

    clock_t t0, t1;
    double t;

    gmc_basis_sets(&B, &iB, &lenB, &lo, &hi, n, d);

if (verbose)
{
    printf("Frobenius on the diagonal fibre\n");
    printf("N  = %ld\n", N);
    printf("N2 = %ld\n", N2);
    printf("M  = %ld\n",  M);
}

if (verbose)
{
    printf("Basis for H_{dR}^%ld(U)\n", n);
    gmc_basis_print(B, iB, lenB, n, d);
    printf("\n");
}

    alift = _fmpz_vec_init(n + 1);
    dinv  = _fmpz_vec_init(M/p + 1);
    mu    = _fmpz_vec_init(M + 1);

    u = malloc((n + 1) * sizeof(long));
    v = malloc((n + 1) * sizeof(long));

if (verbose)
{
    printf("Teichmuller lifts\n");
    t0 = clock();
}

    for (i = 0; i <= n; i++)
    {
        _padic_teichmuller(alift + i, a + i, P, N2);
    }

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

if (verbose)
{
    printf("Sequence d^{-r}\n");
    t0 = clock();
}

    precompute_dinv(dinv, M, d, p, N2);

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

if (verbose)
{
    printf("Sequence mu_{m}\n");
    t0 = clock();
}

    precompute_mu(mu, M, p, N2);

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

    mat_clear(F, ctx);
    mat_init(F, lenB, lenB, ctx);
    mat_zero(F, ctx);

if (verbose)
{
    printf("Matrix F\n");
    t0 = clock();
}

    for (i = 0; i < lenB; i++)
    {
        for (j = 0; j < lenB; j++)
        {
            long k;

            for (k = 0; k <= n; k++)
            {
                u[k] = mon_get_exp(B[i], k);
                v[k] = mon_get_exp(B[j], k);
                if ((p * (u[k] + 1) - (v[k] + 1)) % d != 0)
                {
                     break;
                }
            }

            if (k <= n)
            {
                padic_zero((void *) mat_entry(F, i, j, ctx));
            }
            else
            {
                entry((void *) mat_entry(F, i, j, ctx), 
                    u, v, alift, dinv, mu, M, C, n, d, p, N);
            }
        }
    }

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

    _fmpz_vec_clear(alift, n + 1);
    _fmpz_vec_clear(dinv, M/p + 1);
    _fmpz_vec_clear(mu, M + 1);
    free(u);
    free(v);
    free(B);
    free(iB);
}

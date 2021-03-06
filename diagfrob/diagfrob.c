/******************************************************************************

    Copyright (C) 2012, 2013 Sebastian Pancratz

******************************************************************************/

#include <time.h>

#include "gmconnection.h"
#include "diagfrob.h"

/* 
    Insertion sort for an array of length n. 
 */
static void _sort(long *a, long n)
{
    long i, j, v;
    for (i = 1; i < n; i++)
    {
        v = a[i];
        for (j = i; j > 0 && v < a[j-1]; j--)
        {
            a[j] = a[j-1];
        }
        a[j] = v;
    }
}

/*
    Linear search in an array a[lo], ..., a[hi-1] for the value x.
 */
static long _lsearch(const long *a, long lo, long hi, long x)
{
    long i;
    for (i = lo; i < hi; i++)
        if (a[i] == x)
            return i;
    return -1;
}

/* 
    Binary search in an array a[lo], ..., a[hi-1] for the value x.
 */
static long _bsearch(const long *a, long lo, long hi, long x)
{
    hi--;
    while (lo < hi)
    {
        long mi = lo + (hi - lo) / 2;
        if (a[mi] < x)
            lo = mi + 1;
        else
            hi = mi;
    }
    return (lo == hi && a[lo] == x) ? lo : -1;
}

/*
    Moves the unique elements of {C, *lenC} to the front of the array 
    and sets *lenC to number of these.
 */
static void _remove_duplicates(long *C, long *lenC)
{
    long i, j, k;

    k = (*lenC > 0) ? 1 : 0;
    for (i = 1; i < *lenC; i++)
    {
        for (j = 0; j < k; j++)
        {
            if (C[i] == C[j])
                break;
        }
        if (j == k)
            C[k++] = C[i];
    }
    *lenC = k;
}

/*
    Computes the array {C,lenB} of congruence classes $m \mod p$ 
    for which we need to $\mu_i(m)$, in ascending order.

    Returns the number of elements written into the array as 
    as lenC.  Note that 1 <= lenC <= lenB.
 */
void _congruence_class(long *C, long *lenC, long ind, 
                       const mon_t *B, long lenB, 
                       long n, long d, long p)
{
    long i, j, k, u, v;

    *lenC = 0;
    for (i = 0; i < lenB; i++)
        for (j = 0; j < lenB; j++)
        {
            for (k = 0; k <= n; k++)
            {
                u = mon_get_exp(B[i], k);
                v = mon_get_exp(B[j], k);
                if ((p * (u + 1) - (v + 1)) % d != 0)
                {
                    break;
                }
            }

            if (k > n)
            {
                u = mon_get_exp(B[i], ind);
                v = mon_get_exp(B[j], ind);
                C[(*lenC)++] = ((p * (u + 1) - (v + 1)) / d) % p;
            }
        }

    /* Remove duplicates */
    _remove_duplicates(C, lenC);
    _sort(C, *lenC);
}

/*
    Computes the rising factorial $\prod_{i=0}^{n-1} (x+i) mod m$.
 */
void fmpz_mod_rfac_uiui(fmpz_t r, ulong x, ulong n, const fmpz_t m)
{
    if (fmpz_sgn(m) <= 0)
    {
        printf("Exception (fmpz_mod_rfac_uiui).  m < 0.");
        abort();
    }

    if (fmpz_is_one(m))
    {
        fmpz_zero(r);
    }
    else if (n == 0)
    {
        fmpz_one(r);
    }
    else if (n == 1)
    {
        fmpz_set_ui(r, x);
        fmpz_mod(r, r, m);
    }
    else if (x == 0)
    {
        fmpz_zero(r);
    }
    else  /* m > 1, n > 1, x > 0 */
    {
        /*
            Choose l such that we can multiple l factors in 
            this rising factorial without overflow mod m
         */
        ulong i, l;

        /* Set l = log_2(x + n - 1), avoiding overflow */
        {
            fmpz_t t;

            fmpz_init_set_ui(t, x);
            fmpz_add_ui(t, t, n - 1);
            l = fmpz_clog_ui(t, 2);
            fmpz_clear(t);
        }
        l = (fmpz_clog_ui(m, 2) + (l - 1)) / l - 1;

        if (l > 1)
        {
            fmpz_t t;

            fmpz_init(t);
            fmpz_rfac_uiui(r, x, n % l);
            for (i = n % l; i < n; i += l)
            {
                fmpz_rfac_uiui(t, x + i, l);
                fmpz_mul(r, r, t);
                fmpz_mod(r, r, m);
            }
            fmpz_clear(t);
        }
        else 
        {
            fmpz_set_ui(r, x);
            fmpz_mod(r, r, m);
            for (i = 1; i < n; i++)
            {
                fmpz_mul_ui(r, r, x + i);
                fmpz_mod(r, r, m);
            }
        }
    }
}

/*
    Computes the sequence of inverses of $i!$ to relative 
    $p$-adic precision $N$, where $i$ is one of the following:

        o Equal to $m - p k$ for some $m$ such that we have to compute 
          the value $\mu_m$; or 
        o In the range $0 \leq i \leq \floor{M/p}$.

    The array (C, lenC) contains the sorted list of congruence classes 
    modulo $p$ for which we have to compute $\mu_m$.

    TODO:  Note that this implementation handles both cases $p = 2$ 
    and $p > 2$, even though the former could be handled by a more 
    specialised implementation of the same algorithm.
 */

static void precompute_nu(fmpz *nu, long *v, long M, 
                          const long *C, long lenC, long p, long N)
{
    const long R  = M / p;
    const long N2 = N + (M / (p - 1));

    fmpz_t P, PN2, t;
    padic_inv_t S;
    double pinv;

    long i, j;

    fmpz_init_set_ui(P, p);
    fmpz_init(PN2);
    fmpz_pow_ui(PN2, P, N2);
    fmpz_init(t);

    /*
        Step 1. Compute $i! mod p^{N_2}$ where $N_2 \geq N + \max \ord_p (i!)$
        Step 2. Invert the unit part of $i!$ modulo $p^N$
     */

    fmpz_one(nu + 0);
    for (i = 1; i <= R; i++)
    {
        fmpz_mul_ui(nu + i, nu + (i - 1), i);
        fmpz_mod(nu + i, nu + i, PN2);
    }

    /* Let j denote the greatest index s.t. nu[j] has been computed */
    for (j = R, i = R + 1; i <= M; i++)
    {
        if (_bsearch(C, 0, lenC, i % p) != -1)
        {
            fmpz_mod_rfac_uiui(t, j + 1, i - j, PN2);
            fmpz_mul(nu + i, nu + j, t);
            fmpz_mod(nu + i, nu + i, PN2);
            j = i;
        }
    }

    _padic_inv_precompute(S, P, N);

    pinv = n_precompute_inverse(p);

    v[0] = 0;
    for (i = 1; i <= R; i++)
    {
        v[i] = - _fmpz_remove(nu + i, P, pinv);
        _padic_inv_precomp(nu + i, nu + i, S);
    }
    for (i = R + 1; i <= M; i++)
    {
        if (_bsearch(C, 0, lenC, i % p) != -1)
        {
            v[i] = - _fmpz_remove(nu + i, P, pinv);
            _padic_inv_precomp(nu + i, nu + i, S);
        }
    }

    fmpz_clear(P);
    fmpz_clear(PN2);
    fmpz_clear(t);
    _padic_inv_clear(S);
}

/*
    Computes the sequences $\mu_{i,0}, \dotsc, \mu_{i,M}$ modulo $p^N$.

    The array (C, lenC) contains the sorted list of congruence classes 
    modulo $p$ for which we have to compute $\mu_m$.
 */
 
void precompute_muex(fmpz **mu, long M, 
                     const long **C, const long *lenC, 
                     const fmpz *a, long n, long p, long N)
{
    const long ve = (p == 2) ? M / 4 + 1 : M / (p * (p - 1)) + 1;

    fmpz_t P, pNe, pe;
    fmpz_t apow, f, g, h;

    fmpz *nu;
    long *v;

    long i, j;

    fmpz_init_set_ui(P, p);
    fmpz_init(pNe);
    fmpz_init(pe);
    fmpz_pow_ui(pNe, P, N + ve);
    fmpz_pow_ui(pe, P, ve);

    fmpz_init(apow);
    fmpz_init(f);
    fmpz_init(g);
    fmpz_init(h);

    /* Precompute $(l!)^{-1}$ */
    nu = _fmpz_vec_init(M + 1);
    v  = malloc((M + 1) * sizeof(long));

    {
        long *D, lenD = 0, k = 0;

        for (i = 0; i <= n; i++)
            lenD += lenC[i];

        D = malloc(lenD * sizeof(long));

        for (i = 0; i <= n; i++)
            for (j = 0; j < lenC[i]; j++)
                D[k++] = C[i][j];

        _remove_duplicates(D, &lenD);
        _sort(D, lenD);

        precompute_nu(nu, v, M, D, lenD, p, N + ve);

        free(D);
    }

    for (i = 0; i <= n; i++)
    {
        long m = -1, quo, idx, w;
        fmpz *z;

        /* Set apow = a[i]^{-(p-1)} mod p^N */
        fmpz_invmod(apow, a + i, pNe);
        fmpz_powm_ui(apow, apow, p - 1, pNe);

        /*
            Run over all relevant m in [0, M]. 
            Note that lenC[i] > 0 for all i.
         */
        for (quo = 0; m <= M; quo++)
        {
            for (idx = 0; idx < lenC[i]; idx++)
            {
                m = quo * p + C[i][idx];

                if (m > M)
                    break;

                /*
                    Note that $\mu_m$ is equal to 
                    $\sum_{k=0}^{\floor{m/p}} p^{\floor{m/p}-k}\nu_{m-pk}\nu_k$
                    where $\nu_i$ denotes the number with unit part nu[i] 
                    and valuation v[i].
                 */
                w = (p == 2) ? (3 * m) / 4 - (m == 3 || m == 7) : m / p;
                z = mu[i] + lenC[i] * quo + idx;
                fmpz_zero(z);
                fmpz_one(h);
                for (j = 0; j <= m / p; j++)
                {
                    fmpz_pow_ui(f, P, ve + w - j + v[m - p*j] + v[j]);
                    fmpz_mul(g, nu + (m - p*j), nu + j);

                    fmpz_mul(f, f, g);
                    fmpz_mul(f, f, h);

                    fmpz_add(z, z, f);
                    fmpz_mod(z, z, pNe);

                    /* Set h = a[i]^{- (j+1)(p-1)} mod p^{N+e} */
                    fmpz_mul(h, h, apow);
                    fmpz_mod(h, h, pNe);
                }
                fmpz_divexact(z, z, pe);
            }
        }
    }

    fmpz_clear(P);
    fmpz_clear(pNe);
    fmpz_clear(pe);

    fmpz_clear(apow);
    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(h);

    _fmpz_vec_clear(nu, M + 1);
    free(v);
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

static void precompute_dinv_2(fmpz *list, long M, long d, long N)
{
    fmpz_one(list + 0);

    if (M >= 2)
    {
        const fmpz_t P = {2L};
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

static void precompute_dinv_p(fmpz *list, long M, long d, long p, long N)
{
    fmpz_one(list + 0);

    if (M >= p)
    {
        fmpz_t P, PN;
        long r;

        fmpz_init_set_ui(P, p);
        fmpz_init(PN);
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

static void precompute_dinv(fmpz *list, long M, long d, long p, long N)
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

static void dsum_2(
    fmpz_t rop, 
    const fmpz *dinv, const fmpz *mu, long M, const long *C, long lenC, 
    const fmpz_t a, long ui, long vi, long n, long d, long N)
{
    const fmpz_t P = {2L};
    const long m0  = (2 * (ui + 1) - (vi + 1)) / d;
    const long u   = ui + 1;

    long m, r;
    fmpz_t apow, f0, f1, f2, g;

    fmpz_init(apow);
    fmpz_init(f0);
    fmpz_init(f1);
    fmpz_init(f2);
    fmpz_init(g);

    fmpz_zero(rop);

    for (m = m0; m <= M; m += 2)
    {
        /* Note that r = 0 in the first iteration */
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

        if (r == 0)
        {
            fmpz_one(apow);
        }
        else
        {
            fmpz_mul(apow, apow, a);
            fmpz_fdiv_r_2exp(apow, apow, N);
        }

        /*
            g = a_i^r f_{r} d^{-r} \mu_m
              = h * f2 * dinv[r] * mu[m]
         */

        fmpz_mul(g, f2, dinv + r);
        fmpz_fdiv_r_2exp(g, g, N);
        fmpz_mul(g, g, apow);
        fmpz_fdiv_r_2exp(g, g, N);
        fmpz_mul(g, g, mu + m);
        fmpz_fdiv_r_2exp(g, g, N);

        fmpz_add(rop, rop, g);
    }

    fmpz_fdiv_r_2exp(rop, rop, N);

    fmpz_clear(apow);
    fmpz_clear(f0);
    fmpz_clear(f1);
    fmpz_clear(f2);
    fmpz_clear(g);
}

/*
    Computes the double sum involved in the expression for 
    $\alpha_{u+1, v+1}$ when $p$ is an odd prime, namely
    \[
    \sum_{m,r} \Bigl(\frac{u_i + 1}{d}\Bigr)_r \mu_m.
    \]

    Assumptions:

        * $p d$ fits into a signed long
        * $\floor{M/p} * d$ fits into a signed long
 */

static void dsum_p(
    fmpz_t rop, 
    const fmpz *dinv, const fmpz *mu, long M, const long *C, long lenC, 
    const fmpz_t a, long ui, long vi, long n, long d, long p, long N)
{
    long m, r, idx;
    fmpz_t apm1, apow, f, g, P, PN;

    fmpz_init(apm1);
    fmpz_init(apow);
    fmpz_init(f);
    fmpz_init(g);
    fmpz_init_set_ui(P, p);
    fmpz_init(PN);

    fmpz_pow_ui(PN, P, N);

    fmpz_zero(rop);

    r = 0;
    m = (p * (ui + 1) - (vi + 1)) / d;

    if (m <= M)  /* Step {r = 0} */
    {
        idx = _bsearch(C, 0, lenC, m % p);

        fmpz_powm_ui(apm1, a, p - 1, PN);
        fmpz_one(apow);
        fmpz_one(f);
        fmpz_mod(rop, mu + idx + lenC * (m / p), PN);
    }

    for (r = 1, m += p; m <= M; r++, m += p)
    {
        idx = _bsearch(C, 0, lenC, m % p);

        fmpz_mul(apow, apow, apm1);
        fmpz_mod(apow, apow, PN);
        fmpz_mul_ui(f, f, ui + 1 + (r - 1) * d);
        fmpz_mod(f, f, PN);
        fmpz_mul(g, f, dinv + r);
        fmpz_mul(g, g, apow);
        fmpz_mul(g, g, mu + idx + lenC * (m / p));
        fmpz_mod(g, g, PN);
        fmpz_add(rop, rop, g);
    }

    fmpz_mod(rop, rop, PN);

    fmpz_clear(apm1);
    fmpz_clear(apow);
    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(P);
    fmpz_clear(PN);
}

static void dsum(
    fmpz_t rop, 
    const fmpz *dinv, const fmpz *mu, long M, const long *C, long lenC, 
    const fmpz_t a, long ui, long vi, long n, long d, long p, long N)
{
    if (p == 2)
        dsum_2(rop, dinv, mu, M, C, lenC, a, ui, vi, n, d, N);
    else
        dsum_p(rop, dinv, mu, M, C, lenC, a, ui, vi, n, d, p, N);
}

/*
    Computes $\alpha_{u+1,v+1}$ modulo $p^N$.
 */

static void alpha(fmpz_t rop, const long *u, const long *v, 
    const fmpz *a, const fmpz *dinv, const fmpz **mu, long M, const long **C, const long *lenC, 
    long n, long d, long p, long N)
{
    const long ku = diagfrob_k(u, n, d);

    long i;
    fmpz_t f, g, P, PN;

    fmpz_init(f);
    fmpz_init(g);
    fmpz_init_set_ui(P, p);
    fmpz_init(PN);
    fmpz_pow_ui(PN, P, N);

    fmpz_pow_ui(rop, P, ku);
    fmpz_mod(rop, rop, PN);

    for (i = 0; i <= n; i++)
    {
        long e = (p * (u[i] + 1) - (v[i] + 1)) / d;

        fmpz_powm_ui(f, a + i, e, PN);
        dsum(g, dinv, mu[i], M, C[i], lenC[i], a + i, u[i], v[i], n, d, p, N);
        fmpz_mul(rop, rop, f);
        fmpz_mul(rop, rop, g);
        fmpz_mod(rop, rop, PN);
    }

    if (ku % 2 != 0 && !fmpz_is_zero(rop))
    {
        fmpz_sub(rop, PN, rop);
    }

    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(P);
    fmpz_clear(PN);
}

static void entry(fmpz_t rop_u, long *rop_v, 
    const long *u, const long *v, const fmpz *a, const fmpz *dinv, 
    const fmpz **mu, long M, const long **C, const long *lenC, 
    long n, long d, long p, long N, long N2)
{
    const long ku = diagfrob_k(u, n, d);
    const long kv = diagfrob_k(v, n, d);

    fmpz_t f, g, P;

    fmpz_init(f);
    fmpz_init(g);
    fmpz_init_set_ui(P, p);

    /*
        Compute $g := (u'-1)! \alpha_{u+1,v+1}$ to precision $N2$.
     */

    fmpz_fac_ui(f, ku - 1);
    alpha(g, u, v, a, dinv, mu, M, C, lenC, n, d, p, N2);
    fmpz_mul(g, f, g);

    /*
        Compute $f := (-1)^{u'+v'} (v'-1)!$ exactly.
     */

    fmpz_fac_ui(f, kv - 1);
    if ((ku + kv) % 2 != 0)
    {
        fmpz_neg(f, f);
    }

    /*
        Set rop to the product of $f$ and $g^{-1} mod $p^N$.
     */

    *rop_v = fmpz_remove(f, f, P) + n - fmpz_remove(g, g, P);

    if (*rop_v >= N)
    {
        fmpz_zero(rop_u);
        *rop_v = 0;
    }
    else
    {
        _padic_inv(g, g, P, N - *rop_v);

        fmpz_mul(rop_u, f, g);
        fmpz_pow_ui(f, P, N - *rop_v);
        fmpz_mod(rop_u, rop_u, f);
    }

    fmpz_clear(f);
    fmpz_clear(g);
    fmpz_clear(P);
}

void diagfrob(padic_mat_t F, const fmpz *a, long n, long d, long N, 
              const padic_ctx_t ctx, const int verbose)
{
    const fmpz *P = ctx->p;
    const long p  = fmpz_get_si(P);

    const long delta = diagfrob_delta(n, P);
    const long N2    = N - n + 2 * (padic_val_fac_ui(n - 1, P) + n + delta);
    const long M     = (p * p * (N2 + n_clog(N2 + 3, p) + 4) + (p - 2))
                       / (p - 1) - 1;

    mon_t *B;
    long *iB, lenB, lo, hi;

    long i, j, k, *u, *v;

    long **C, *lenC;

    fmpz *dinv, **mu;

    clock_t t0 = 0, t1 = 0;
    double t;

    gmc_basis_sets(&B, &iB, &lenB, &lo, &hi, n, d);

if (verbose)
{
    printf("Frobenius on the diagonal fibre\n");
    printf("N  = %ld\n", N);
    printf("N2 = %ld\n", N2);
    printf("M  = %ld\n", M);
}

if (verbose)
{
    printf("Basis for H_{dR}^%ld(U)\n", n);
    gmc_basis_print(B, iB, lenB, n, d);
    printf("\n");
}

    C    = malloc((n + 1) * sizeof(long *));
    C[0] = malloc((n + 1) * lenB * sizeof(long));
    for (i = 1; i <= n; i++)
    {
        C[i] = C[i-1] + lenB;
    }
    lenC = malloc((n + 1) * sizeof(long));

    for (i = 0; i <= n; i++)
    {
        _congruence_class(C[i], &lenC[i], i, B, lenB, n, d, p);
    }

    dinv  = _fmpz_vec_init(M/p + 1);
    mu    = malloc((n + 1) * sizeof(fmpz *));
    for (i = 0; i <= n; i++)
    {
        mu[i] = _fmpz_vec_init(((M + 1 + p - 1) / p) * lenC[i]);
    }

    u = malloc((n + 1) * sizeof(long));
    v = malloc((n + 1) * sizeof(long));

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

    precompute_muex(mu, M, (const long **) C, lenC, a, n, p, N2);  /* XXX */

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

if (verbose)
{
    printf("Matrix F\n");
    t0 = clock();
}

    for (i = 0; i < lenB; i++)
        for (j = 0; j < lenB; j++)
        {
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
                fmpz_zero(padic_mat_entry(F, i, j));
            }
            else
            {
                long o;

                entry(padic_mat_entry(F, i, j), &o, 
                      u, v, a, dinv, (const fmpz **) mu, M, (const long **) C, lenC, n, d, p, N, N2);

                if (o != - delta)
                {
                    fmpz_t w;
                    fmpz_init(w);
                    fmpz_pow_ui(w, P, o + delta);
                    fmpz_mul(padic_mat_entry(F, i, j), 
                             padic_mat_entry(F, i, j), w);
                    fmpz_clear(w);
                }
            }
        }

    padic_mat_val(F) = - delta;
    _padic_mat_canonicalise(F, ctx);

if (verbose)
{
    t1 = clock();
    t  = (double) (t1 - t0) / CLOCKS_PER_SEC;
    printf("T = %f\n", t);
}

    _fmpz_vec_clear(dinv, M/p + 1);
    for (i = 0; i <= n; i++)
    {
        _fmpz_vec_clear(mu[i], ((M + 1 + p - 1) / p) * lenC[i]);
    }
    free(mu);
    free(C[0]);
    free(C);
    free(lenC);
    free(u);
    free(v);
    free(B);
    free(iB);
}


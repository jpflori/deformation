/******************************************************************************

    Copyright (C) 2010, 2011, 2012 Sebastian Pancratz

******************************************************************************/

#ifndef GMCONNECTION_H
#define GMCONNECTION_H

#include <stdlib.h>

#include <gmp.h>
#include "flint/flint.h"
#include "flint/fmpz.h"
#include "flint/fmpz_poly.h"
#include "flint/fmpz_poly_q.h"
#include "flint/fmpz_poly_mat.h"

#include "generics.h"
#include "mat.h"
#include "mat_csr.h"
#include "vec.h"
#include "mpoly.h"

long gmc_basis_size(long n, long d);

void gmc_basis_sets(mon_t **B, long **iB, long *lenB, long *l, long *u, 
                    long n, long d);

int gmc_basis_contains(const mpoly_t f, long d);

void gmc_basis_print(const mon_t *B, const long *iB, long lenB, long n, long d);

void gmc_init_auxmatrix(mat_csr_t M, 
                        mon_t **R, mon_t **C, long *p, 
                        const mpoly_t P, long k, 
                        const ctx_t ctx);

void gmc_array2poly(mpoly_t poly, const char *c, const mon_t *m, long len, 
                    const ctx_t ctx);

void gmc_poly2array(char *c, const mpoly_t poly, const mon_t *m, long len, 
                    const ctx_t ctx);

void gmc_decompose_poly(mpoly_t * A, const mpoly_t poly, 
                        const mat_csr_solve_t s, 
                        mon_t * const rows, 
                        mon_t * const cols, 
                        long * const p, 
                        const ctx_t ctx);

void gmc_reduce(mpoly_t *R, 
                const mpoly_t Q, long k, long d, mpoly_t *dP, 
                mat_csr_solve_t *s, 
                mon_t **rows, mon_t **cols, long **p, 
                long l, long u, 
                const ctx_t ctx);

void gmc_derivatives(mpoly_t *D, const mpoly_t P, const ctx_t ctx);

void gmc_compute(mat_t M, mon_t **rows, mon_t **cols, 
                 const mpoly_t P, const ctx_t ctx);

void gmc_convert(fmpz_poly_mat_t numM, fmpz_poly_t denM, 
                 const mat_t M, const ctx_t ctx);

#endif


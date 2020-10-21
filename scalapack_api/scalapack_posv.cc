// Copyright (c) 2017-2020, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "slate/slate.hh"
#include "scalapack_slate.hh"

#include <complex>

namespace slate {
namespace scalapack_api {

// -----------------------------------------------------------------------------

// Required CBLACS calls
extern "C" void Cblacs_gridinfo(int context, int* np_row, int* np_col, int*  my_row, int*  my_col);

// Type generic function calls the SLATE routine
template< typename scalar_t >
void slate_pposv(const char* uplostr, int n, int nrhs, scalar_t* a, int ia, int ja, int* desca, scalar_t* b, int ib, int jb, int* descb, int* info);

// -----------------------------------------------------------------------------
// C interfaces (FORTRAN_UPPER, FORTRAN_LOWER, FORTRAN_UNDERSCORE)
// Each C interface calls the type generic slate_pher2k

extern "C" void PSPOSV(const char* uplo, int* n, int* nrhs, float* a, int* ia, int* ja, int* desca, float* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void psposv(const char* uplo, int* n, int* nrhs, float* a, int* ia, int* ja, int* desca, float* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void psposv_(const char* uplo, int* n, int* nrhs, float* a, int* ia, int* ja, int* desca, float* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

// -----------------------------------------------------------------------------

extern "C" void PDPOSV(const char* uplo, int* n, int* nrhs, double* a, int* ia, int* ja, int* desca, double* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pdposv(const char* uplo, int* n, int* nrhs, double* a, int* ia, int* ja, int* desca, double* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pdposv_(const char* uplo, int* n, int* nrhs, double* a, int* ia, int* ja, int* desca, double* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

// -----------------------------------------------------------------------------

extern "C" void PCPOSV(const char* uplo, int* n, int* nrhs, std::complex<float>* a, int* ia, int* ja, int* desca, std::complex<float>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pcposv(const char* uplo, int* n, int* nrhs, std::complex<float>* a, int* ia, int* ja, int* desca, std::complex<float>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pcposv_(const char* uplo, int* n, int* nrhs, std::complex<float>* a, int* ia, int* ja, int* desca, std::complex<float>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

// -----------------------------------------------------------------------------

extern "C" void PZPOSV(const char* uplo, int* n, int* nrhs, std::complex<double>* a, int* ia, int* ja, int* desca, std::complex<double>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pzposv(const char* uplo, int* n, int* nrhs, std::complex<double>* a, int* ia, int* ja, int* desca, std::complex<double>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

extern "C" void pzposv_(const char* uplo, int* n, int* nrhs, std::complex<double>* a, int* ia, int* ja, int* desca, std::complex<double>* b, int* ib, int* jb, int* descb, int* info)
{
    slate_pposv(uplo, *n, *nrhs, a, *ia, *ja, desca, b, *ib, *jb, descb, info);
}

// -----------------------------------------------------------------------------
template< typename scalar_t >
void slate_pposv(const char* uplostr, int n, int nrhs, scalar_t* a, int ia, int ja, int* desca, scalar_t* b, int ib, int jb, int* descb, int* info)
{
    // todo: figure out if the pxq grid is in row or column

    // make blas single threaded
    // todo: does this set the omp num threads correctly
    int saved_num_blas_threads = slate_set_num_blas_threads(1);

    blas::Uplo uplo = blas::char2uplo(uplostr[0]);
    static slate::Target target = slate_scalapack_set_target();
    static int verbose = slate_scalapack_set_verbose();
    int64_t lookahead = 1;

    // Matrix sizes
    int64_t Am = n;
    int64_t An = n;
    int64_t Bm = n;
    int64_t Bn = nrhs;
    slate::Pivots pivots;

    // create SLATE matrices from the ScaLAPACK layouts
    int nprow, npcol, myrow, mycol;
    Cblacs_gridinfo(desc_CTXT(desca), &nprow, &npcol, &myrow, &mycol);
    auto A = slate::HermitianMatrix<scalar_t>::fromScaLAPACK(uplo, desc_N(desca), a, desc_LLD(desca), desc_MB(desca), nprow, npcol, MPI_COMM_WORLD);
    A = slate_scalapack_submatrix(Am, An, A, ia, ja, desca);

    Cblacs_gridinfo(desc_CTXT(descb), &nprow, &npcol, &myrow, &mycol);
    auto B = slate::Matrix<scalar_t>::fromScaLAPACK(desc_M(descb), desc_N(descb), b, desc_LLD(descb), desc_MB(descb), nprow, npcol, MPI_COMM_WORLD);
    B = slate_scalapack_submatrix(Bm, Bn, B, ib, jb, descb);

    if (verbose && myrow == 0 && mycol == 0)
        logprintf("%s\n", "posv");

    slate::posv(A, B, {
        {slate::Option::Lookahead, lookahead},
        {slate::Option::Target, target},
    });

    slate_set_num_blas_threads(saved_num_blas_threads);

    // todo: extract the real info
    *info = 0;
}

} // namespace scalapack_api
} // namespace slate

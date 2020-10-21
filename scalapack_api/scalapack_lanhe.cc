// Copyright (c) 2017-2020, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "scalapack_slate.hh"

namespace slate {
namespace scalapack_api {

// -----------------------------------------------------------------------------

// Required CBLACS calls
extern "C" void Cblacs_gridinfo(int context, int*  np_row, int* np_col, int*  my_row, int*  my_col);

// Type generic function calls the SLATE routine
template< typename scalar_t >
blas::real_type<scalar_t> slate_planhe(const char* normstr, const char* uplostr, int n, scalar_t* a, int ia, int ja, int* desca, blas::real_type<scalar_t>* work);

// -----------------------------------------------------------------------------
// C interfaces (FORTRAN_UPPER, FORTRAN_LOWER, FORTRAN_UNDERSCORE)
// Each C interface calls the type generic slate_pher2k

// -----------------------------------------------------------------------------

extern "C" float PCLANHE(const char* norm, const char* uplo, int* n, std::complex<float>* a, int* ia, int* ja, int* desca, float* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

extern "C" float pclanhe(const char* norm, const char* uplo, int* n, std::complex<float>* a, int* ia, int* ja, int* desca, float* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

extern "C" float pclanhe_(const char* norm, const char* uplo, int* n, std::complex<float>* a, int* ia, int* ja, int* desca, float* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

// -----------------------------------------------------------------------------

extern "C" double PZLANHE(const char* norm, const char* uplo, int* n, std::complex<double>* a, int* ia, int* ja, int* desca, double* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

extern "C" double pzlanhe(const char* norm, const char* uplo, int* n, std::complex<double>* a, int* ia, int* ja, int* desca, double* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

extern "C" double pzlanhe_(const char* norm, const char* uplo, int* n, std::complex<double>* a, int* ia, int* ja, int* desca, double* work)
{
    return slate_planhe(norm, uplo, *n, a, *ia, *ja, desca, work);
}

// -----------------------------------------------------------------------------
template< typename scalar_t >
blas::real_type<scalar_t> slate_planhe(const char* normstr, const char* uplostr, int n, scalar_t* a, int ia, int ja, int* desca, blas::real_type<scalar_t>* work)
{
    // todo: figure out if the pxq grid is in row or column

    // make blas single threaded
    // todo: does this set the omp num threads correctly
    int saved_num_blas_threads = slate_set_num_blas_threads(1);

    blas::Uplo uplo = blas::char2uplo(uplostr[0]);
    lapack::Norm norm = lapack::char2norm(normstr[0]);
    static slate::Target target = slate_scalapack_set_target();
    static int verbose = slate_scalapack_set_verbose();
    int64_t lookahead = 1;

    // Matrix sizes
    int64_t Am = n;
    int64_t An = n;

    // create SLATE matrices from the ScaLAPACK layouts
    int nprow, npcol, myrow, mycol;
    Cblacs_gridinfo(desc_CTXT(desca), &nprow, &npcol, &myrow, &mycol);
    auto A = slate::HermitianMatrix<scalar_t>::fromScaLAPACK(uplo, desc_N(desca), a, desc_LLD(desca), desc_MB(desca), nprow, npcol, MPI_COMM_WORLD);
    A = slate_scalapack_submatrix(Am, An, A, ia, ja, desca);

    if (verbose && myrow == 0 && mycol == 0)
        logprintf("%s\n", "lanhe");

    blas::real_type<scalar_t> A_norm = 1.0;
    A_norm = slate::norm(norm, A, {
        {slate::Option::Target, target},
        {slate::Option::Lookahead, lookahead}
    });

    slate_set_num_blas_threads(saved_num_blas_threads);

    return A_norm;
}

} // namespace scalapack_api
} // namespace slate

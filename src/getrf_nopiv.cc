// Copyright (c) 2017-2020, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "slate/slate.hh"
#include "aux/Debug.hh"
#include "slate/Matrix.hh"
#include "slate/Tile_blas.hh"
#include "slate/TriangularMatrix.hh"
#include "internal/internal.hh"

namespace slate {

// specialization namespace differentiates, e.g.,
// internal::getrf_nopiv from internal::specialization::getrf_nopiv
namespace internal {
namespace specialization {

//------------------------------------------------------------------------------
/// Distributed parallel LU factorization without pivoting.
/// Generic implementation for any target.
/// Panel and lookahead computed on host using Host OpenMP task.
/// @ingroup gesv_specialization
///
template <Target target, typename scalar_t>
void getrf_nopiv(slate::internal::TargetType<target>,
           Matrix<scalar_t>& A,
           int64_t ib, int max_panel_threads, int64_t lookahead)
{
    using BcastList = typename Matrix<scalar_t>::BcastList;
    using BcastListTag = typename Matrix<scalar_t>::BcastListTag;

    Layout layout = Layout::ColMajor;

    const int priority_one = 1;
    const int priority_zero = 0;
    int64_t A_nt = A.nt();
    int64_t A_mt = A.mt();
    int64_t min_mt_nt = std::min(A.mt(), A.nt());

    // OpenMP needs pointer types, but vectors are exception safe
    std::vector< uint8_t > column_vector(A_nt);
    std::vector< uint8_t > diag_vector(A_nt);
    uint8_t* column = column_vector.data();
    uint8_t* diag = diag_vector.data();
    uint8_t mpi_bandwidth;

    #pragma omp parallel
    #pragma omp master
    {
        omp_set_nested(1);
        for (int64_t k = 0; k < min_mt_nt; ++k) {

            // panel, high priority
            #pragma omp task depend(inout:column[k]) \
                             depend(out:diag[k]) \
                             priority(priority_one)
            {
                // factor A(k, k)
                internal::getrf_nopiv<Target::HostTask>(A.sub(k, k, k, k),
                                                        ib,
                                                        priority_one);

                // Update panel
                int tag_k = k;
                BcastList bcast_list_A;
                bcast_list_A.push_back({k, k, {A.sub(k+1, A_mt-1, k, k),
                                               A.sub(k, k, k+1, A_nt-1)}});
                A.template listBcast(bcast_list_A, layout, tag_k);
            }

            #pragma omp task depend(inout:column[k]) \
                             depend(in:diag[k]) \
                             priority(priority_one)
            {
                auto Akk = A.sub(k, k, k, k);
                auto Tkk = TriangularMatrix<scalar_t>(Uplo::Upper, Diag::NonUnit, Akk);

                internal::trsm<Target::HostTask>(
                    Side::Right,
                    scalar_t(1.0), std::move(Tkk),
                                   A.sub(k+1, A_mt-1, k, k),
                    priority_one);
            }

            #pragma omp task depend(inout:column[k]) \
                             depend(inout:mpi_bandwidth) \
                             priority(priority_one)
            {
                BcastListTag bcast_list_A;
                // bcast the tiles of the panel to the right hand side
                for (int64_t i = k+1; i < A_mt; ++i) {
                    // send A(i, k) across row A(i, k+1:nt-1)
                    const int64_t tag = i;
                    bcast_list_A.push_back({i, k, {A.sub(i, i, k+1, A_nt-1)}, tag});
                }
                A.template listBcastMT(bcast_list_A, layout);

            }
            // update lookahead column(s), high priority
            for (int64_t j = k+1; j < k+1+lookahead && j < A_nt; ++j) {
                #pragma omp task depend(in:diag[k]) \
                                 depend(inout:column[j]) \
                                 priority(priority_one)
                {
                    int tag_j = j;
                    auto Akk = A.sub(k, k, k, k);
                    auto Tkk =
                        TriangularMatrix<scalar_t>(Uplo::Lower, Diag::Unit, Akk);

                    // solve A(k, k) A(k, j) = A(k, j)
                    internal::trsm<Target::HostTask>(
                        Side::Left,
                        scalar_t(1.0), std::move(Tkk),
                                       A.sub(k, k, j, j), priority_one);

                    // send A(k, j) across column A(k+1:mt-1, j)
                    // todo: trsm still operates in ColMajor
                    A.tileBcast(k, j, A.sub(k+1, A_mt-1, j, j), layout, tag_j);
                }

                #pragma omp task depend(in:column[k]) \
                                 depend(inout:column[j]) \
                                 priority(priority_one)
                {
                    // A(k+1:mt-1, j) -= A(k+1:mt-1, k) * A(k, j)
                    internal::gemm<Target::HostTask>(
                        scalar_t(-1.0), A.sub(k+1, A_mt-1, k, k),
                                        A.sub(k, k, j, j),
                        scalar_t(1.0),  A.sub(k+1, A_mt-1, j, j),
                        layout, priority_one);
                }
            }
            // update trailing submatrix, normal priority
            if (k+1+lookahead < A_nt) {
                #pragma omp task depend(in:diag[k]) \
                                 depend(inout:column[k+1+lookahead]) \
                                 depend(inout:column[A_nt-1])
                {
                    auto Akk = A.sub(k, k, k, k);
                    auto Tkk =
                        TriangularMatrix<scalar_t>(Uplo::Lower, Diag::Unit, Akk);

                    // solve A(k, k) A(k, kl+1:nt-1) = A(k, kl+1:nt-1)
                    // todo: target
                    internal::trsm<Target::HostTask>(
                        Side::Left,
                        scalar_t(1.0), std::move(Tkk),
                                       A.sub(k, k, k+1+lookahead, A_nt-1));
            }

            #pragma omp task depend(inout:column[k+1+lookahead]) \
                             depend(inout:column[A_nt-1]) \
                             depend(inout:mpi_bandwidth)
            {
                    // send A(k, kl+1:A_nt-1) across A(k+1:mt-1, kl+1:nt-1)
                    BcastListTag bcast_list_A;
                    for (int64_t j = k+1+lookahead; j < A_nt; ++j) {
                        // send A(k, j) across column A(k+1:mt-1, j)
                        // tag must be distinct from sending left panel
                        const int64_t tag = j + A_mt;
                        bcast_list_A.push_back({k, j, {A.sub(k+1, A_mt-1, j, j)}, tag});
                    }
                    // todo: trsm still operates in ColMajor
                    A.template listBcastMT(bcast_list_A, layout);
                }

                #pragma omp task depend(in:column[k]) \
                                 depend(inout:column[k+1+lookahead]) \
                                 depend(inout:column[A_nt-1])
                {
                    // A(k+1:mt-1, kl+1:nt-1) -= A(k+1:mt-1, k) * A(k, kl+1:nt-1)
                    internal::gemm<target>(
                        scalar_t(-1.0), A.sub(k+1, A_mt-1, k, k),
                                        A.sub(k, k, k+1+lookahead, A_nt-1),
                        scalar_t(1.0),  A.sub(k+1, A_mt-1, k+1+lookahead, A_nt-1),
                        layout, priority_zero);
                }
            }
        }

        #pragma omp taskwait
        A.tileUpdateAllOrigin();
    }
    A.clearWorkspace();
}

//------------------------------------------------------------------------------
/// Distributed parallel non-pivoted LU factorization.
/// GPU device batched cuBLAS implementation.
/// @ingroup gesv_specialization
template <typename scalar_t>
void getrf_nopiv(slate::internal::TargetType<Target::Devices>,
           Matrix<scalar_t>& A,
           int64_t ib, int max_panel_threads, int64_t lookahead)
{
    using BcastList = typename Matrix<scalar_t>::BcastList;
    using BcastListTag = typename Matrix<scalar_t>::BcastListTag;

    Layout layout = Layout::ColMajor;

    const int priority_one = 1;
    const int priority_zero = 0;
    const int64_t A_nt = A.nt();
    const int64_t A_mt = A.mt();
    const int64_t min_mt_nt = std::min(A.mt(), A.nt());
    const int life_factor_one = 1;
    const bool is_shared = lookahead > 0;
    const int64_t batch_size_zero = 0;
    const int64_t num_arrays_two  = 2; // Number of kernels without lookahead

    // two batch arrays plus one for each lookahead
    A.allocateBatchArrays(batch_size_zero, (num_arrays_two + lookahead));
    A.reserveDeviceWorkspace();

    // OpenMP needs pointer types, but vectors are exception safe
    std::vector< uint8_t > column_vector(A_nt);
    std::vector< uint8_t > diag_vector(A_nt);
    uint8_t* column = column_vector.data();
    uint8_t* diag = diag_vector.data();
    uint8_t mpi_bandwidth;

    #pragma omp parallel
    #pragma omp master
    {
        omp_set_nested(1);
        for (int64_t k = 0; k < min_mt_nt; ++k) {

            // panel, high priority
            #pragma omp task depend(inout:column[k]) \
                             depend(out:diag[k]) \
                             priority(priority_one)
            {
                // factor A(k, k)
                internal::getrf_nopiv<Target::HostTask>(A.sub(k, k, k, k),
                                                        ib,
                                                        priority_one);

                // Update panel
                int tag_k = k;
                BcastList bcast_list_A;
                bcast_list_A.push_back({k, k, {A.sub(k+1, A_mt-1, k, k),
                                               A.sub(k, k, k+1, A_nt-1)}});
                A.template listBcast<Target::Devices>(
                    bcast_list_A, layout, tag_k, life_factor_one, true);
            }

            #pragma omp task depend(inout:column[k]) \
                             depend(in:diag[k]) \
                             priority(priority_one)
            {
                auto Akk = A.sub(k, k, k, k);
                auto Tkk = TriangularMatrix<scalar_t>(Uplo::Upper, Diag::NonUnit, Akk);

                internal::trsm<Target::Devices>(
                    Side::Right,
                    scalar_t(1.0), std::move(Tkk),
                                   A.sub(k+1, A_mt-1, k, k),
                    priority_one, layout, 0);
            }

            #pragma omp task depend(inout:column[k]) \
                             depend(inout:mpi_bandwidth) \
                             priority(priority_one)
            {
                BcastListTag bcast_list_A;
                // bcast the tiles of the panel to the right hand side
                for (int64_t i = k+1; i < A_mt; ++i) {
                    // send A(i, k) across row A(i, k+1:nt-1)
                    const int64_t tag = i;
                    bcast_list_A.push_back({i, k, {A.sub(i, i, k+1, A_nt-1)}, tag});
                }
                A.template listBcastMT<Target::Devices>(
                  bcast_list_A, layout, life_factor_one, is_shared);
            }
            // update lookahead column(s), high priority
            for (int64_t j = k+1; j < k+1+lookahead && j < A_nt; ++j) {
                #pragma omp task depend(in:diag[k]) \
                                 depend(inout:column[j]) \
                                 priority(priority_one)
                {
                    int tag_j = j;
                    auto Akk = A.sub(k, k, k, k);
                    auto Tkk =
                        TriangularMatrix<scalar_t>(Uplo::Lower, Diag::Unit, Akk);

                    // solve A(k, k) A(k, j) = A(k, j)
                    internal::trsm<Target::Devices>(
                        Side::Left,
                        scalar_t(1.0), std::move(Tkk),
                                       A.sub(k, k, j, j),
                        priority_one, layout, j-k+1);

                    // send A(k, j) across column A(k+1:mt-1, j)
                    A.tileBcast(k, j, A.sub(k+1, A_mt-1, j, j), layout, tag_j);
                }

                #pragma omp task depend(in:column[k]) \
                                 depend(inout:column[j]) \
                                 priority(priority_one)
                {
                    // A(k+1:mt-1, j) -= A(k+1:mt-1, k) * A(k, j)
                    internal::gemm<Target::Devices>(
                        scalar_t(-1.0), A.sub(k+1, A_mt-1, k, k),
                                        A.sub(k, k, j, j),
                        scalar_t(1.0),  A.sub(k+1, A_mt-1, j, j),
                        layout, priority_one, j-k+1);
                }
            }
            // update trailing submatrix, normal priority
            if (k+1+lookahead < A_nt) {
                #pragma omp task depend(in:diag[k]) \
                                 depend(inout:column[k+1+lookahead]) \
                                 depend(inout:column[A_nt-1])
                {
                    auto Akk = A.sub(k, k, k, k);
                    auto Tkk =
                        TriangularMatrix<scalar_t>(Uplo::Lower, Diag::Unit, Akk);

                    // solve A(k, k) A(k, kl+1:nt-1) = A(k, kl+1:nt-1)
                    // todo: target
                    internal::trsm<Target::Devices>(
                        Side::Left,
                        scalar_t(1.0), std::move(Tkk),
                                       A.sub(k, k, k+1+lookahead, A_nt-1),
                    priority_zero, layout, 1);
                }

                #pragma omp task depend(inout:column[k+1+lookahead]) \
                                 depend(inout:column[A_nt-1]) \
                                 depend(inout:mpi_bandwidth)
                {
                    // send A(k, kl+1:A_nt-1) across A(k+1:mt-1, kl+1:nt-1)
                    BcastListTag bcast_list_A;
                    for (int64_t j = k+1+lookahead; j < A_nt; ++j) {
                        // send A(k, j) across column A(k+1:mt-1, j)
                        // tag must be distinct from sending left panel
                        const int64_t tag = j + A_mt;
                        bcast_list_A.push_back({k, j, {A.sub(k+1, A_mt-1, j, j)}, tag});
                    }
                    A.template listBcastMT<Target::Devices>(
                        bcast_list_A, layout);
                }

                #pragma omp task depend(in:column[k]) \
                                 depend(inout:column[k+1+lookahead]) \
                                 depend(inout:column[A_nt-1])
                {
                    // A(k+1:mt-1, kl+1:nt-1) -= A(k+1:mt-1, k) * A(k, kl+1:nt-1)
                    internal::gemm<Target::Devices>(
                        scalar_t(-1.0), A.sub(k+1, A_mt-1, k, k),
                                        A.sub(k, k, k+1+lookahead, A_nt-1),
                        scalar_t(1.0),  A.sub(k+1, A_mt-1, k+1+lookahead, A_nt-1),
                        layout, priority_zero, 1);
                }
            }
            #pragma omp task depend(inout:diag[k])
            {
                if (A.tileIsLocal(k, k) && k+1 < A_nt) {
                    // release hold on the diagonal tile, since it's not managed by panelRelease
                    std::set<int> dev_set;
                    A.sub(k+1, A_mt-1, k, k).getLocalDevices(&dev_set);
                    A.sub(k, k, k+1, A_nt-1).getLocalDevices(&dev_set);

                    for (auto device : dev_set) {
                        A.tileUnsetHold(k, k, device);
                        A.tileRelease(k, k, device);
                    }
                }
            }
            #pragma omp task depend(inout:column[k])
            {
                const int64_t A_nt = A.nt();
                for (int64_t i = k+1; i < A_nt; ++i) {
                    if (A.tileIsLocal(i, k)) {
                        A.tileUpdateOrigin(i, k);

                        std::set<int> dev_set;
                        A.sub(i, i, k+1, A_mt-1).getLocalDevices(&dev_set);

                        for (auto device : dev_set) {
                            A.tileUnsetHold(i, k, device);
                            A.tileRelease(i, k, device);
                        }
                    }
                }
            }
        }

        #pragma omp taskwait
        A.tileUpdateAllOrigin();
    }
    A.clearWorkspace();
}

} // namespace specialization
} // namespace internal

//------------------------------------------------------------------------------
/// Version with target as template parameter.
/// @ingroup gesv_specialization
///
template <Target target, typename scalar_t>
void getrf_nopiv(Matrix<scalar_t>& A,
           Options const& opts)
{
    int64_t lookahead;
    try {
        lookahead = opts.at(Option::Lookahead).i_;
        assert(lookahead >= 0);
    }
    catch (std::out_of_range&) {
        lookahead = 1;
    }

    int64_t ib;
    try {
        ib = opts.at(Option::InnerBlocking).i_;
        assert(ib >= 0);
    }
    catch (std::out_of_range&) {
        ib = 16;
    }

    int64_t max_panel_threads;
    try {
        max_panel_threads = opts.at(Option::MaxPanelThreads).i_;
        assert(max_panel_threads >= 1 && max_panel_threads <= omp_get_max_threads());
    }
    catch (std::out_of_range&) {
        max_panel_threads = std::max(omp_get_max_threads()/2, 1);
    }

    internal::specialization::getrf_nopiv(internal::TargetType<target>(),
                                    A,
                                    ib, max_panel_threads, lookahead);
}

//------------------------------------------------------------------------------
/// Distributed parallel LU factorization without pivoting.
///
/// Computes an LU factorization without pivoting of a general m-by-n matrix $A$
///
/// The factorization has the form
/// \[
///     A = L U
/// \]
/// where $L$ is lower triangular with unit diagonal elements
/// (lower trapezoidal if m > n), and $U$ is upper triangular
/// (upper trapezoidal if m < n).
///
/// This is the right-looking Level 3 BLAS version of the algorithm.
///
//------------------------------------------------------------------------------
/// @tparam scalar_t
///     One of float, double, std::complex<float>, std::complex<double>.
//------------------------------------------------------------------------------
/// @param[in,out] A
///     On entry, the matrix $A$ to be factored.
///     On exit, the factors $L$ and $U$ from the factorization $A = P L U$;
///     the unit diagonal elements of $L$ are not stored.
///
/// @param[in] opts
///     Additional options, as map of name = value pairs. Possible options:
///     - Option::Lookahead:
///       Number of panels to overlap with matrix updates.
///       lookahead >= 0. Default 1.
///     - Option::InnerBlocking:
///       Inner blocking to use for panel. Default 16.
///     - Option::Target:
///       Implementation to target. Possible values:
///       - HostTask:  OpenMP tasks on CPU host [default].
///       - HostNest:  nested OpenMP parallel for loop on CPU host.
///       - HostBatch: batched BLAS on CPU host.
///       - Devices:   batched BLAS on GPU device.
///
/// TODO: return value
/// @retval 0 successful exit
/// @retval >0 for return value = $i$, $U(i,i)$ is exactly zero. The
///         factorization has been completed, but the factor $U$ is exactly
///         singular, and division by zero will occur if it is used
///         to solve a system of equations.
///
/// @ingroup gesv_computational
///
template <typename scalar_t>
void getrf_nopiv(Matrix<scalar_t>& A,
           Options const& opts)
{
    Target target;
    try {
        target = Target(opts.at(Option::Target).i_);
    }
    catch (std::out_of_range&) {
        target = Target::HostTask;
    }

    switch (target) {
        case Target::Host:
        case Target::HostTask:
            getrf_nopiv<Target::HostTask>(A, opts);
            break;
        case Target::HostNest:
            getrf_nopiv<Target::HostNest>(A, opts);
            break;
        case Target::HostBatch:
            getrf_nopiv<Target::HostBatch>(A, opts);
            break;
        case Target::Devices:
            getrf_nopiv<Target::Devices>(A, opts);
            break;
    }
    // todo: return value for errors?
}

//------------------------------------------------------------------------------
// Explicit instantiations.
template
void getrf_nopiv<float>(
    Matrix<float>& A,
    Options const& opts);

template
void getrf_nopiv<double>(
    Matrix<double>& A,
    Options const& opts);

template
void getrf_nopiv< std::complex<float> >(
    Matrix< std::complex<float> >& A,
    Options const& opts);

template
void getrf_nopiv< std::complex<double> >(
    Matrix< std::complex<double> >& A,
    Options const& opts);

} // namespace slate

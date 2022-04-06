// Copyright (c) 2017-2020, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#ifndef SLATE_TILE_TPMQRT_HH
#define SLATE_TILE_TPMQRT_HH

#include "slate/Tile.hh"
#include "slate/types.hh"

#include <list>
#include <vector>

#include <blas.hh>
#include <lapack.hh>

namespace slate {

//------------------------------------------------------------------------------
/// Multiply the matrix C by the unitary matrix Q obtained from a
/// "triangular-pentagonal" block reflector H.
/// C consists of two tiles, C1 and C2.
///
/// If side == Left:
///
///     C = [ C1 ]  <- k-by-n
///         [ C2 ]  <- m-by-n
///
/// and on exit, $C = op(Q) C$.
/// C is (k+m)-by-n, C1 is k-by-n, C2 is m-by-n, and V2 is m-by-k.
/// m, l are the same in tpqrt; k = tpqrt's n; n here is different.
///
/// If side == Right:
///
///     C = [ C1  C2 ]
///       m-by-k  m-by-n
///
/// and on exit, $C = C op(Q)$.
/// C is m-by-(k+n), C1 is m-by-k, C2 is m-by-n, and V2 is n-by-k.
/// l is the same in tpqrt; n = tpqrt's m; k = tpqrt's n; m here is different.
///
/// Q is a product of block reflectors,
///
///     Q = \prod_{j = 1, ..., r} I - Vj Tj Vj^H
///
/// where r is the number of blocks, Tj is the j-th block of T,
/// and Vj is the j-th block column of V, with internal blocking size ib.
///
/// See Further Details in tpqrt.
///
/// @param[in] side
///     - Side::Left:  Multiply from the left:  $C = op(Q) C$.
///     - Side::Right: Multiply from the right: $C = C op(Q)$.
///
/// @param[in] op
///     - Op::NoTrans:   Multiply by $op(Q) = Q$.
///     - Op::Trans:     Multiply by $op(Q) = Q^T$ (only in real case).
///     - Op::ConjTrans: Multiply by $op(Q) = Q^H$.
///
/// @param[in] l
///     The number of rows of the upper trapezoidal part of V2.
///     - If side = left,  min(m, k) >= l >= 0.
///     - If side = right, min(n, k) >= l >= 0.
///
/// @param[in] V2
///     - If side == Left,  the m-by-k upper pentagonal tile V2.
///     - If side == Right, the n-by-k upper pentagonal tile V2.
///     The i-th column must contain the vector which defines the
///     elementary reflector H(i), for i = 1, 2, ..., k, as returned by
///     tpqrt in A2. The top (m-l)-by-k or (n-l)-by-k portion is rectangular,
///     the bottom l-by-k portion is upper trapezoidal.
///     See Further Details in tpqrt.
///
/// @param[in] T
///     The upper triangular factors of the block reflectors
///     as returned by tpqrt, stored as an ib-by-k tile.
///
/// @param[in,out] C1
///     - If side == Left,  the k-by-n tile C1.
///       C1 can be k2-by-n for k2 >= k; only the upper k-by-n portion is used.
///     - If side == Right, the m-by-k tile C1.
///       C1 can be m-by-k2 for k2 >= k; only the left m-by-k portion is used.
///     On exit, C1 is overwritten by the corresponding block of
///     $op(Q) C$ or $C op(Q)$.
///
/// @param[in,out] C2
///     The m-by-n tile C2.
///     On exit, C2 is overwritten by the corresponding block of
///     $op(Q) C$ or $C op(Q)$.
///
/// Note in LAPACK, A = C1, B = C2, V = V2.
///
/// @ingroup geqrf_tile
///
template <typename scalar_t>
void tpmqrt(
    Side side, Op op, int64_t l,
    Tile<scalar_t> V2,
    Tile<scalar_t> T,
    Tile<scalar_t> C1,
    Tile<scalar_t> C2)
{
#if LAPACK_VERSION >= 30400
    trace::Block trace_block("lapack::tpmqrt");

    int64_t k = V2.nb();
    int64_t m = C2.mb();
    int64_t n = C2.nb();
    if ((n >= k) && (m > n)) {
        m = std::min( C2.mb(), C2.nb() );
    }

    if (side == Side::Left) {
        assert(C1.mb() >= k);
        assert(C1.nb() == n);
        //assert(V2.mb() == m);
        assert(std::min(m, k) >= l);
    }
    else {
        assert(C1.mb() == m);
        assert(C1.nb() >= k);
        assert(V2.mb() == n);
        assert(std::min(n, k) >= l);
    }
    assert(T.nb() == k);

    // Normally, ib = T.mb, but limit <= k.
    int64_t ib = std::min( T.mb(), k );
    lapack::tpmqrt(side, op, m, n, k, l, ib,
                   V2.data(), V2.stride(),
                   T.data(), T.stride(),
                   C1.data(), C1.stride(),
                   C2.data(), C2.stride());
#else
    slate_not_implemented( "In geqrf: tpmqrt requires LAPACK >= 3.4" );
#endif
}

} // namespace slate

#endif // SLATE_TILE_TPMQRT_HH

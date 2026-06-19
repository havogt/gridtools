/*
 * GridTools
 *
 * Copyright (c) 2014-2023, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <cassert>
#include <type_traits>
#include <utility>

#include "../common/defs.hpp"
#include "../common/functional.hpp"
#include "../common/integral_constant.hpp"
#include "../common/tuple.hpp"
#include "../common/tuple_util.hpp"
#include "../meta/is_instantiation_of.hpp"
#include "../sid/concept.hpp"

namespace gridtools::fn {
    namespace column_stage_impl_ {
        template <class F, class Projector = host_device::identity>
        struct scan_pass {
            F m_f;
            Projector m_p;
            constexpr GT_FUNCTION scan_pass(F f, Projector p = {}) : m_f(f), m_p(p) {}
        };

        template <class T>
        using is_scan_pass = meta::is_instantiation_of<scan_pass, T>;

        template <bool IsBackward>
        struct base : std::bool_constant<IsBackward> {
            static GT_FUNCTION constexpr auto prologue() { return tuple<>(); }
            static GT_FUNCTION constexpr auto epilogue() { return tuple<>(); }
        };

        using fwd = base<false>;
        using bwd = base<true>;

        template <class Vertical, class ScanOrFold, int Out, int... Ins>
        struct column_stage {
            template <class Seed, class MakeIterator, class Ptr, class Strides>
            GT_FUNCTION auto operator()(
                Seed seed, std::size_t size, MakeIterator &&make_iterator, Ptr ptr, Strides const &strides) const {
                constexpr std::size_t prologue_size = std::tuple_size_v<decltype(ScanOrFold::prologue())>;
                constexpr std::size_t epilogue_size = std::tuple_size_v<decltype(ScanOrFold::epilogue())>;
                GT_NVCC_DIAG_PUSH_SUPPRESS(186) // pointless comparison of unsigned with 0
                assert(size >= prologue_size + epilogue_size);
                GT_NVCC_DIAG_POP_SUPPRESS(186)
                using step_t = integral_constant<int, ScanOrFold::value ? -1 : 1>;
                auto const &v_stride = sid::get_stride<Vertical>(strides);
                auto inc = [&] { sid::shift(ptr, v_stride, step_t()); };
                auto next = [&](auto acc, auto pass) {
                    if constexpr (is_scan_pass<decltype(pass)>()) {
                        // scan
                        auto res =
                            pass.m_f(std::move(acc), make_iterator(integral_constant<int, Ins>(), ptr, strides)...);
                        *host_device::at_key<integral_constant<int, Out>>(ptr) = pass.m_p(res);
                        inc();
                        return res;
                    } else {
                        // fold
                        auto res = pass(std::move(acc), make_iterator(integral_constant<int, Ins>(), ptr, strides)...);
                        inc();
                        return res;
                    }
                    // disable incorrect warning "missing return statement at end of non-void function"
                    GT_NVCC_DIAG_PUSH_SUPPRESS(940)
                };
                GT_NVCC_DIAG_POP_SUPPRESS(940)
                if constexpr (ScanOrFold::value)
                    sid::shift(ptr, v_stride, size - 1);
                auto acc = tuple_util::host_device::fold(next, std::move(seed), ScanOrFold::prologue());
                std::size_t n = size - prologue_size - epilogue_size;
                for (std::size_t i = 0; i < n; ++i)
                    acc = next(std::move(acc), ScanOrFold::body());
                return tuple_util::host_device::fold(next, std::move(acc), ScanOrFold::epilogue());
            }
        };

        template <class... ColumnStages>
        struct merged_column_stage {
            template <class Seed, class MakeIterator, class Ptr, class Strides>
            GT_FUNCTION auto operator()(
                Seed seed, std::size_t size, MakeIterator &&make_iterator, Ptr ptr, Strides const &strides) const {
                return tuple_util::host_device::fold(
                    [&](auto acc, auto stage) {
                        return stage(std::move(acc), size, std::forward<MakeIterator>(make_iterator), ptr, strides);
                    },
                    std::move(seed),
                    tuple(ColumnStages()...));
            }
        };

        // A column_stage restricted to a vertical sub-range of the kernel's column:
        // it processes [TopTrim, size - BotTrim) instead of [0, size). This lets scans
        // with different K-extents (e.g. a forward sweep on [1, N) reading Koff[-1] and a
        // back-substitution on [0, N)) share one kernel launched over the union domain,
        // each running over exactly its original range.
        template <class Vertical, class ScanOrFold, int TopTrim, int BotTrim, int Out, int... Ins>
        struct scan_substage {
            template <class Seed, class MakeIterator, class Ptr, class Strides>
            GT_FUNCTION auto operator()(
                Seed seed, std::size_t size, MakeIterator const &make_iterator, Ptr ptr, Strides const &strides) const {
                if constexpr (TopTrim != 0) {
                    auto const &v_stride = sid::get_stride<Vertical>(strides);
                    sid::shift(ptr, v_stride, integral_constant<int, TopTrim>());
                }
                std::size_t sub_size = size - (TopTrim + BotTrim);
                return column_stage<Vertical, ScanOrFold, Out, Ins...>()(
                    std::move(seed), sub_size, make_iterator, std::move(ptr), strides);
            }
        };

        // Codegen-facing descriptor of one substage: direction, vertical trims and the raw
        // (pre-arg-offset) output/input arg indices. The vertical_executor turns it into a
        // scan_substage<Vertical, ...> with the backend arg-offset applied (see executor.hpp).
        template <class ScanOrFold, int TopTrim, int BotTrim, int Out, int... Ins>
        struct scan_substage_raw {};

        // Runs several scan_substages over one column in a single kernel, each with its own
        // seed taken positionally from the `seeds` tuple. Substages execute in order, so a
        // producer substage (e.g. forward sweep writing z_q/w) runs fully before a consumer
        // substage (back-substitution) for the same column; the column intermediates stay
        // resident for the thread (one launch instead of N, no inter-stage global round-trip
        // forced by a kernel boundary). Unlike merged_column_stage it does NOT thread one
        // accumulator across stages — each stage is independently seeded.
        template <class... SubStages>
        struct merged_seeded_column_stage {
            template <class Seeds, class MakeIterator, class Ptr, class Strides, std::size_t... Is>
            GT_FUNCTION void run_(Seeds &seeds, std::size_t size, MakeIterator const &make_iterator, Ptr const &ptr,
                Strides const &strides, std::index_sequence<Is...>) const {
                (SubStages()(tuple_util::host_device::get<Is>(seeds), size, make_iterator, ptr, strides), ...);
            }
            template <class Seeds, class MakeIterator, class Ptr, class Strides>
            GT_FUNCTION auto operator()(
                Seeds seeds, std::size_t size, MakeIterator &&make_iterator, Ptr ptr, Strides const &strides) const {
                run_(seeds, size, make_iterator, ptr, strides, std::index_sequence_for<SubStages...>());
                return tuple<>();
            }
        };

        // Like a column scan, but each level's scan result is consumed in-loop by a cell-local
        // Tail functor instead of being written to a SID — so the scan output (e.g. next_w) is
        // never materialized to global memory; it stays register-resident and only the Tail's
        // own outputs are stored. This folds a cell-local post-scan field-op into the scan's
        // tail. The Tail is invoked per level with:
        //   res     : scan result at this level        (= next_w(K))
        //   acc     : previous scan result             (= next_w(K+1) in a backward scan, since
        //             back-substitution runs K high->low so K+1 is computed before K)
        //   surface : seed / column-top boundary value (= next_w(N), captured once, carried down)
        //   make_iterator, ptr, strides : to read other per-level inputs and write its outputs.
        // The scan body comes from ScanOrFold::body() (a scan_pass); Tail is a stateless functor
        // operator()(res, acc, surface, make_iterator, ptr, strides). Plain scan only (no
        // prologue/epilogue) — the tridiagonal back-substitution case.
        // Trims are in K-coordinate (consistent with scan_substage), independent of scan direction:
        //  - the BODY window K in [BodyTopTrim, size-BodyBotTrim): where the scan recurrence is
        //    applied and `acc`/`res` advance. Outside it the recurrence is frozen (res = acc), so a
        //    substage can avoid reading inputs at levels its scan was never defined on.
        //  - the TAIL window K in [TailTopTrim, size-TailBotTrim): where the folded consumer writes.
        // The consumer may run over a *wider* column than the scan (e.g. it handles the column-top
        // level itself with a scan-free fallback); there the body is trimmed but the tail still
        // fires, and the consumer's own per-level branch discards `res`. The loop still steps in
        // scan order (backward: high-K -> low-K) so the recurrence threads correctly; only the
        // window tests are mapped from loop index i to K = (backward ? size-1-i : i).
        template <class Vertical, class ScanOrFold, class Tail, int BodyTopTrim, int BodyBotTrim, int TailTopTrim,
            int TailBotTrim, int... Ins>
        struct scan_with_tail {
            template <class Seed, class MakeIterator, class Ptr, class Strides>
            GT_FUNCTION auto operator()(
                Seed seed, std::size_t size, MakeIterator &&make_iterator, Ptr ptr, Strides const &strides) const {
                using step_t = integral_constant<int, ScanOrFold::value ? -1 : 1>;
                auto const &v_stride = sid::get_stride<Vertical>(strides);
                auto const pass = ScanOrFold::body();
                Tail const tail{};
                if constexpr (ScanOrFold::value)
                    sid::shift(ptr, v_stride, size - 1);
                auto surface = seed;
                auto acc = std::move(seed);
                for (std::size_t i = 0; i < size; ++i) {
                    std::size_t const k = ScanOrFold::value ? (size - 1 - i) : i;
                    auto in_body = (k >= (std::size_t)BodyTopTrim && k < size - (std::size_t)BodyBotTrim);
                    auto res =
                        in_body ? pass.m_f(acc, make_iterator(integral_constant<int, Ins>(), ptr, strides)...) : acc;
                    if (k >= (std::size_t)TailTopTrim && k < size - (std::size_t)TailBotTrim)
                        tail(res, acc, surface, make_iterator, ptr, strides);
                    sid::shift(ptr, v_stride, step_t());
                    if (in_body)
                        acc = res;
                }
                return acc;
            }
        };

        // Codegen-facing descriptor of a scan-with-tail: ScanOrFold (the scan struct, fwd/bwd +
        // body), Tail (the per-level consumer functor — a template taking an int arg-offset so the
        // executor can rebase its output/input keys), the body + tail vertical trims, and the raw
        // (pre-arg-offset) scan input arg indices. The vertical_executor turns it into a
        // scan_with_tail<Vertical, ...> with the backend arg-offset applied (see executor.hpp).
        template <class ScanOrFold, template <int> class Tail, int BodyTopTrim, int BodyBotTrim, int TailTopTrim,
            int TailBotTrim, int... Ins>
        struct scan_with_tail_raw {};
    } // namespace column_stage_impl_

    using column_stage_impl_::bwd;
    using column_stage_impl_::column_stage;
    using column_stage_impl_::fwd;
    using column_stage_impl_::merged_column_stage;
    using column_stage_impl_::merged_seeded_column_stage;
    using column_stage_impl_::scan_substage;
    using column_stage_impl_::scan_substage_raw;
    using column_stage_impl_::scan_with_tail;
    using column_stage_impl_::scan_with_tail_raw;

#if GT_NVCC_WORKAROUND_1766
    template <class F, class Projector = host_device::identity>
    GT_FUNCTION constexpr auto scan_pass(F &&f, Projector &&p = {}) {
        return column_stage_impl_::scan_pass(std::forward<F>(f), std::forward<Projector>(p));
    }
#else
    using column_stage_impl_::scan_pass;
#endif
} // namespace gridtools::fn

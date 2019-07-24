/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <type_traits>

#include "../meta.hpp"
#include "interval.hpp"

namespace gridtools {
    namespace stage_matrix {
#define DEFINE_GETTER(property) \
    template <class T>          \
    using get_##property = typename T::property##_t

        DEFINE_GETTER(plh);
        DEFINE_GETTER(is_tmp);
        DEFINE_GETTER(data);
        DEFINE_GETTER(extent);
        DEFINE_GETTER(cache);
        DEFINE_GETTER(cache_io_policy);
        DEFINE_GETTER(num_colors);
        DEFINE_GETTER(funs);
        DEFINE_GETTER(interval);
        DEFINE_GETTER(plh_map);
        DEFINE_GETTER(execution);
        DEFINE_GETTER(need_sync);

#undef DEFINE_GETTER

        template <class Plh, class IsTmp, class Data, class Extent, class Cache, class CacheIoPolicy, class NumColors>
        struct plh_info {
            using plh_t = Plh;
            using is_tmp_t = IsTmp;
            using data_t = Data;
            using extent_t = Extent;
            using cache_t = Cache;
            using cache_io_policy_t = CacheIoPolicy;
            using num_colors_t = NumColors;
        };

        template <class>
        struct merge_data_types_impl;

        template <class T>
        struct merge_data_types_impl<meta::list<T>> {
            using type = T;
        };

        template <class T>
        struct merge_data_types_impl<meta::list<T, T const>> {
            using type = T;
        };

        template <class T>
        struct merge_data_types_impl<meta::list<T const, T>> {
            using type = T;
        };

        template <class... Ts>
        using merge_data_types = typename merge_data_types_impl<meta::dedup<meta::list<Ts...>>>::type;

        template <class>
        struct merge_caches_impl {
            using type = void;
        };

        template <class T>
        struct merge_caches_impl<meta::list<T>> {
            using type = T;
        };

        template <class... Ts>
        using merge_caches = typename merge_caches_impl<meta::dedup<meta::list<Ts...>>>::type;

        template <class...>
        struct merge_plh_infos;

        template <class Plh,
            class IsTmp,
            class... Datas,
            class... Extents,
            class... Caches,
            class... CacheIoPolicies,
            class NumColors>
        struct merge_plh_infos<plh_info<Plh, IsTmp, Datas, Extents, Caches, CacheIoPolicies, NumColors>...> {
            using type = plh_info<Plh,
                IsTmp,
                merge_data_types<Datas...>,
                enclosing_extent<Extents...>,
                merge_caches<Caches...>,
                merge_caches<CacheIoPolicies...>,
                NumColors>;
        };

        template <class... Maps>
        using merge_plh_maps = meta::mp_merge<meta::force<merge_plh_infos>::apply, Maps...>;

        template <class Funs, class Interval, class PlhMap, class Extent, class Execution, class NeedSync>
        struct cell {
            using funs_t = Funs;
            using interval_t = Interval;
            using plh_map_t = PlhMap;
            using extent_t = Extent;
            using execution_t = Execution;
            using need_sync_t = NeedSync;

            using plhs_t = meta::transform<get_plh, plh_map_t>;
        };

        template <class... Ts>
        struct can_fuse_intervals : std::false_type {};

        template <class Funs, class... Intervals, class PlhMap, class Extent, class Execution, class NeedSync>
        struct can_fuse_intervals<cell<Funs, Intervals, PlhMap, Extent, Execution, NeedSync>...> : std::true_type {};

        template <class...>
        struct can_fuse_stages : std::false_type {};

        template <class... NeedSyncs>
        struct can_fuse_need_syncs;

        template <class First, class... NeedSyncs>
        struct can_fuse_need_syncs<First, NeedSyncs...> : conjunction<negation<NeedSyncs>...> {};

        // TODO
        template <class... PlhMaps>
        struct caches_are_conflicting;

        template <class... Funs, class Interval, class... PlhMaps, class Extent, class Execution, class... NeedSync>
        struct can_fuse_stages<cell<Funs, Interval, PlhMaps, Extent, Execution, NeedSync>...>
            : can_fuse_need_syncs<NeedSync...> {};

        namespace lazy {
            template <class...>
            struct concat_intervals;

            template <class T>
            struct concat_intervals<T> {
                using type = T;
            };
            template <class From, class Level, class NextLevel, class To>
            struct concat_intervals<interval<From, Level>, interval<NextLevel, To>> {
                GT_STATIC_ASSERT(
                    level_to_index<Level>::value + 1 == level_to_index<NextLevel>::value, GT_INTERNAL_ERROR);
                using type = interval<From, To>;
            };
            template <class... Intervals>
            struct concat_intervals {
                using type = meta::combine<meta::force<concat_intervals>::apply, meta::list<Intervals...>>;
            };

            template <class...>
            struct fuse_intervals;

            template <class Funs,
                class Interval,
                class... Intervals,
                class PlhMap,
                class Extent,
                class Execution,
                class NeedSync>
            struct fuse_intervals<cell<Funs, Interval, PlhMap, Extent, Execution, NeedSync>,
                cell<Funs, Intervals, PlhMap, Extent, Execution, NeedSync>...> {
                using type = cell<Funs,
                    typename concat_intervals<Interval, Intervals...>::type,
                    PlhMap,
                    Extent,
                    Execution,
                    NeedSync>;
            };

            template <class...>
            struct fuse_stages;

            template <class... Funs, class Interval, class... PlhMaps, class Extent, class Execution, class... NeedSync>
            struct fuse_stages<cell<Funs, Interval, PlhMaps, Extent, Execution, NeedSync>...> {
                using type = cell<meta::concat<Funs...>,
                    Interval,
                    merge_plh_maps<PlhMaps...>,
                    Extent,
                    Execution,
                    disjunction<NeedSync...>>;
            };
        } // namespace lazy
        GT_META_DELEGATE_TO_LAZY(fuse_intervals, class... Ts, Ts...);
        GT_META_DELEGATE_TO_LAZY(fuse_stages, class... Ts, Ts...);
        GT_META_DELEGATE_TO_LAZY(concat_intervals, class... Ts, Ts...);

        template <class Cell>
        using is_cell_empty = meta::is_empty<typename Cell::funs_t>;

        template <template <class...> class Pred>
        struct row_predicate_f {
            template <class... Rows>
            using apply = meta::all<meta::transform<Pred, Rows...>>;
        };

        template <template <class...> class F>
        struct row_fuse_f {
            template <class... Rows>
            using apply = meta::transform<F, Rows...>;
        };

        template <class Cells>
        using has_funs = negation<meta::all_of<is_cell_empty, Cells>>;

        template <template <class...> class Pred, template <class...> class F, class Matrix>
        using fuse_rows = meta::group<row_predicate_f<Pred>::template apply, row_fuse_f<F>::template apply, Matrix>;

        template <class TransposedMatrix>
        using fuse_interval_rows = fuse_rows<can_fuse_intervals, fuse_intervals, TransposedMatrix>;

        template <class TransposedMatrix>
        using trim_interval_rows = meta::trim<row_predicate_f<is_cell_empty>::apply, TransposedMatrix>;

        template <class TransposedMatrix>
        using compress_interval_rows = trim_interval_rows<fuse_interval_rows<TransposedMatrix>>;

        template <class...>
        struct interval_info {};

        template <class... Funs,
            class Interval,
            class... PlhMaps,
            class... Extent,
            class... Execution,
            class... NeedSync>
        struct interval_info<cell<Funs, Interval, PlhMaps, Extent, Execution, NeedSync>...> {
            using interval_t = Interval;
            using cells_t = meta::filter<meta::not_<is_cell_empty>::apply, interval_info>;
        };

        template <class... IntervalInfos>
        class fused_view_item {
            GT_STATIC_ASSERT(sizeof...(IntervalInfos) > 0, GT_INTERNAL_ERROR);
            GT_STATIC_ASSERT(meta::are_same<typename meta::length<IntervalInfos>::type...>::value, GT_INTERNAL_ERROR);

            using cells_t = meta::transform<meta::first, fused_view_item>;
            using cell_t = meta::first<cells_t>;

          public:
            using caches_t = typename cell_t::caches_t;
            using execution_t = typename cell_t::execution_t;
            using extent_t = meta::rename<enclosing_extent, meta::transform<get_extent, cells_t>>;
            //            using plhs_t = meta::dedup<meta::flatten<meta::transform<cell_plhs, cells_t>>>;
            using interval_t = concat_intervals<typename IntervalInfos::interval_t...>;
            using interval_infos_t = fused_view_item;
        };

        template <class... Cells>
        class split_view_item {
            GT_STATIC_ASSERT(sizeof...(Cells) > 0, GT_INTERNAL_ERROR);
            using cell_t = meta::first<split_view_item>;

          public:
            using execution_t = typename cell_t::execution_t;
            using extent_t = typename cell_t::extent_t;
            using plh_map_t = typename cell_t::plh_map_t;
            using plhs_t = meta::transform<get_plh, plh_map_t>;
            using interval_t = concat_intervals<typename Cells::interval_t...>;
            using cells_t =
                meta::if_<execute::is_backward<execution_t>, meta::reverse<split_view_item>, split_view_item>;
        };

        template <class... Items>
        struct split_view {
            using plh_map_t = merge_plh_maps<typename Items::plh_map_t...>;
            using tmp_plh_map_t = meta::filter<get_is_tmp, plh_map_t>;
        };

        // TODO: fuse stage rows before transpose
        template <class Matrix>
        using make_fused_view_item = meta::rename<fused_view_item,
            meta::transform<interval_info, compress_interval_rows<meta::transpose<Matrix>>>>;

        template <class Matrices>
        using fused_view = meta::transform<make_fused_view_item, Matrices>;

        template <class RawCells,
            class Cells = meta::trim<is_cell_empty, meta::group<can_fuse_intervals, fuse_intervals, RawCells>>>
        using make_split_view_item = meta::rename<split_view_item, Cells>;

        // TODO: fuse stage rows before flatten
        template <class Matrices>
        using make_split_view =
            meta::rename<split_view, meta::transform<make_split_view_item, meta::flatten<Matrices>>>;
    } // namespace stage_matrix
} // namespace gridtools

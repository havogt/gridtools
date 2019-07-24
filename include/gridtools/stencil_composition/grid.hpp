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

#include <cassert>
#include <initializer_list>
#include <iterator>
#include <type_traits>

#include "../common/defs.hpp"
#include "../common/halo_descriptor.hpp"
#include "../common/host_device.hpp"
#include "../common/integral_constant.hpp"
#include "axis.hpp"
#include "execution_types.hpp"
#include "extent.hpp"
#include "interval.hpp"
#include "level.hpp"

namespace gridtools {
    namespace grid_impl_ {
        constexpr int_t real_offset(int_t val) { return val > 0 ? val - 1 : val; }
    } // namespace grid_impl_

    template <class Interval>
    class grid {
        GT_STATIC_ASSERT(is_interval<Interval>::value, GT_INTERNAL_ERROR);

        static constexpr size_t size = Interval::ToLevel::splitter - Interval::FromLevel::splitter + 1;

        int_t m_i_low_bound;
        int_t m_i_size;
        int_t m_j_low_bound;
        int_t m_j_size;
        int_t m_value_list[size];

      public:
        using interval_t = Interval;

        template <class Intervals = std::initializer_list<int_t>>
        grid(halo_descriptor const &direction_i, halo_descriptor const &direction_j, Intervals const &intervals)
            : m_i_low_bound((int_t)direction_i.begin()),
              m_i_size((int_t)direction_i.end() + 1 - (int_t)direction_i.begin()),
              m_j_low_bound((int_t)direction_j.begin()),
              m_j_size((int_t)direction_j.end() + 1 - (int_t)direction_j.begin()) {
            m_value_list[0] = 0;
            auto src = std::begin(intervals);
            for (size_t i = 1; i < size; ++i, ++src) {
                assert(src != intervals.end());
                m_value_list[i] = m_value_list[i - 1] + *src;
            }
        }

        int_t i_low_bound() const { return m_i_low_bound; }

        int_t j_low_bound() const { return m_j_low_bound; }

        template <class Extent = extent<>>
        int_t i_size(Extent = {}) const {
            return m_i_size + Extent::iplus::value - Extent::iminus::value;
        }

        template <class Extent = extent<>>
        int_t j_size(Extent = {}) const {
            return m_j_size + Extent::jplus::value - Extent::jminus::value;
        }

        template <class From, class To, class Execution>
        auto k_start(interval<From, To>, Execution) const {
            return value_at<From>() - k_min();
        }

        template <class From, class To>
        auto k_start(interval<From, To>, execute::backward) const {
            return value_at<To>() - k_min();
        }

        template <class From = typename Interval::FromLevel, class To = typename Interval::ToLevel>
        auto k_size(interval<From, To> = {}) const {
            return count(From(), To());
        }

        template <class Level, int_t Offset = grid_impl_::real_offset(Level::offset)>
        int_t value_at() const {
            GT_STATIC_ASSERT(is_level<Level>::value, GT_INTERNAL_ERROR);
            return m_value_list[Level::splitter] + Offset;
        }

        template <uint_t FromSplitter,
            int_t FromOffset,
            uint_t ToSplitter,
            int_t ToOffset,
            int_t OffsetLimit,
            int_t Extra = grid_impl_::real_offset(ToOffset) - grid_impl_::real_offset(FromOffset),
            std::enable_if_t<(FromSplitter < ToSplitter), int> = 0>
        int_t count(level<FromSplitter, FromOffset, OffsetLimit>, level<ToSplitter, ToOffset, OffsetLimit>) const {
            return m_value_list[ToSplitter] - m_value_list[FromSplitter] + Extra + 1;
        }

        template <uint_t FromSplitter,
            int_t FromOffset,
            uint_t ToSplitter,
            int_t ToOffset,
            int_t OffsetLimit,
            int_t Extra = grid_impl_::real_offset(FromOffset) - grid_impl_::real_offset(ToOffset),
            std::enable_if_t<(FromSplitter >= ToSplitter), int> = 0>
        int_t count(level<FromSplitter, FromOffset, OffsetLimit>, level<ToSplitter, ToOffset, OffsetLimit>) const {
            return m_value_list[FromSplitter] - m_value_list[ToSplitter] + Extra + 1;
        }

        template <uint_t Splitter,
            int_t FromOffset,
            int_t ToOffset,
            int_t OffsetLimit,
            int_t Delta = grid_impl_::real_offset(ToOffset) - grid_impl_::real_offset(FromOffset),
            int_t Val = (Delta > 0) ? 1 + Delta : 1 - Delta>
        integral_constant<int_t, Val> count(
            level<Splitter, FromOffset, OffsetLimit>, level<Splitter, ToOffset, OffsetLimit>) const {
            return {};
        }

        integral_constant<int_t, grid_impl_::real_offset(Interval::FromLevel::offset)> k_min() const { return {}; }

        /**
         * The total length of the k dimension as defined by the axis.
         */
        int_t k_total_length() const {
            // the axis has to be one level bigger than the largest k interval
            return value_at<typename Interval::ToLevel>() - k_min() + 1;
        }
    };

    template <class T>
    using is_grid = meta::is_instantiation_of<grid, T>;

    template <class Axis>
    grid<typename Axis::axis_interval_t> make_grid(
        halo_descriptor const &direction_i, halo_descriptor const &direction_j, Axis const &axis) {
        return {direction_i, direction_j, axis.interval_sizes()};
    }
    inline grid<axis<1>::axis_interval_t> make_grid(uint_t di, uint_t dj, int_t dk) {
        return make_grid(halo_descriptor(di), halo_descriptor(dj), axis<1>(dk));
    }
    template <class Axis>
    grid<typename Axis::axis_interval_t> make_grid(uint_t di, uint_t dj, Axis const &axis) {
        return {halo_descriptor(di), halo_descriptor(dj), axis.interval_sizes()};
    }
    inline grid<axis<1>::axis_interval_t> make_grid(
        halo_descriptor const &direction_i, halo_descriptor const &direction_j, int_t dk) {
        return make_grid(direction_i, direction_j, axis<1>(dk));
    }
} // namespace gridtools

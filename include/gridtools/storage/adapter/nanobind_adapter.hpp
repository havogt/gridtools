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

#include <algorithm>
#include <stdexcept>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include "../../common/array.hpp"
#include "../../common/integral_constant.hpp"
#include "../../common/tuple.hpp"
#include "../../sid/simple_ptr_holder.hpp"
#include "../../sid/synthetic.hpp"
#include "../../sid/unknown_kind.hpp"

namespace gridtools {
    namespace nanobind_sid_adapter_impl_ {
#if NB_VERSION_MAJOR >= 2
        using array_size_t = nanobind::ssize_t;
#else
        using array_size_t = std::size_t;
#endif

        inline constexpr array_size_t dynamic_size = -1;

        // Use `dynamic_size` for dynamic stride, use an integral value for static
        // stride.
        template <array_size_t... Values>
        using stride_spec = std::integer_sequence<array_size_t, Values...>;

        template <class IndexSequence>
        struct dynamic_strides_helper;

        template <std::size_t... Indices>
        struct dynamic_strides_helper<std::index_sequence<Indices...>> {
            using type = stride_spec<(void(Indices), dynamic_size)...>;
        };

        template <std::size_t N>
        using fully_dynamic_strides = typename dynamic_strides_helper<std::make_index_sequence<N>>::type;

        template <array_size_t SpecValue>
        constexpr auto select_static_stride_value(std::size_t dyn_value) {
            if constexpr (SpecValue == dynamic_size) {
                return dyn_value;
            } else {
                if (SpecValue != dyn_value) {
                    throw std::invalid_argument("static stride in stride specification doesn't match dynamic stride");
                }
                return gridtools::integral_constant<std::size_t, SpecValue>{};
            }
        }

        template <array_size_t... SpecValues, std::size_t... IndexValues>
        constexpr auto select_static_strides_helper(
            stride_spec<SpecValues...>, const std::size_t *dyn_values, std::index_sequence<IndexValues...>) {

            return gridtools::tuple{select_static_stride_value<SpecValues>(dyn_values[IndexValues])...};
        }

        template <array_size_t... SpecValues>
        constexpr auto select_static_strides(stride_spec<SpecValues...> spec, const std::size_t *dyn_values) {
            return select_static_strides_helper(spec, dyn_values, std::make_index_sequence<sizeof...(SpecValues)>{});
        }

        template <class T,
            array_size_t... Sizes,
            class... Args,
            class Strides = fully_dynamic_strides<sizeof...(Sizes)>,
            class StridesKind = sid::unknown_kind>
        auto as_sid(nanobind::ndarray<T, nanobind::shape<Sizes...>, Args...> ndarray,
            Strides stride_spec = {},
            StridesKind = {}) {
            using sid::property;
            const auto ptr = ndarray.data();
            constexpr auto ndim = sizeof...(Sizes);
            assert(ndim == ndarray.ndim());
            gridtools::array<std::size_t, ndim> shape;
            std::copy_n(ndarray.shape_ptr(), ndim, shape.begin());
            gridtools::array<std::size_t, ndim> strides;
            std::copy_n(ndarray.stride_ptr(), ndim, strides.begin());
            const auto static_strides = select_static_strides(stride_spec, strides.data());

            return sid::synthetic()
                .template set<property::origin>(sid::host_device::simple_ptr_holder{ptr})
                .template set<property::strides>(static_strides)
                .template set<property::strides_kind, StridesKind>()
                .template set<property::lower_bounds>(gridtools::array<integral_constant<std::size_t, 0>, ndim>())
                .template set<property::upper_bounds>(shape);
        }
    } // namespace nanobind_sid_adapter_impl_

    namespace nanobind {
        using nanobind_sid_adapter_impl_::as_sid;
        using nanobind_sid_adapter_impl_::dynamic_size;
        using nanobind_sid_adapter_impl_::fully_dynamic_strides;
        using nanobind_sid_adapter_impl_::stride_spec;

    } // namespace nanobind
} // namespace gridtools

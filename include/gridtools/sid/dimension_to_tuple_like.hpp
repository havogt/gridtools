/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include "../common/array.hpp"
#include "../common/hymap.hpp"
#include "../common/tuple.hpp"
#include "../meta.hpp"
#include "composite.hpp"
#include "concept.hpp"
#include "delegate.hpp"
#include "gridtools/common/host_device.hpp"
#include "sid_shift_origin.hpp"

namespace gridtools {
    namespace sid {
        namespace dimension_to_array_impl_ {
            template <class Dim, class Sid>
            struct remove_dimension_sid : sid::delegate<Sid> {
                friend decltype(hymap::canonicalize_and_remove_key<Dim>(std::declval<sid::strides_type<Sid>>()))
                sid_get_strides(remove_dimension_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_strides(obj.m_impl));
                }
                friend decltype(hymap::canonicalize_and_remove_key<Dim>(std::declval<sid::lower_bounds_type<Sid>>()))
                sid_get_lower_bounds(remove_dimension_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_lower_bounds(obj.m_impl));
                }
                friend decltype(hymap::canonicalize_and_remove_key<Dim>(std::declval<sid::upper_bounds_type<Sid>>()))
                sid_get_upper_bounds(remove_dimension_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_upper_bounds(obj.m_impl));
                }

                using sid::delegate<Sid>::delegate;
            };

            template <class Dim, class Sid>
            remove_dimension_sid<Dim, Sid> remove_dimension(Sid &&sid) {
                return {std::forward<Sid>(sid)};
            }

            template <class Dim, class T>
            struct as_tuple_like_helper;

            template <class Dim, size_t... Is>
            struct as_tuple_like_helper<Dim, std::index_sequence<Is...>> {
                template <class Sid>
                static auto apply(Sid &&sid) {
                    using keys = sid::composite::keys<integral_constant<int, Is>...>;
                    return keys::make_values(remove_dimension<Dim>(
                        sid::shift_sid_origin(std::forward<Sid>(sid), hymap::keys<Dim>::make_values(Is)))...);
                }
            };
        } // namespace dimension_to_array_impl_

        template <class Dim, size_t N, class Sid> // N optional?
        auto dimension_to_tuple_like(Sid &&sid) {
            return dimension_to_array_impl_::as_tuple_like_helper<Dim, std::make_index_sequence<N>>::apply(
                std::forward<Sid>(sid));
        }
    } // namespace sid
} // namespace gridtools

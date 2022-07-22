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

#include <type_traits>

#include "../common/integral_constant.hpp"
#include "../common/tuple.hpp"
#include "../meta/push_front.hpp"
#include "tag_invoke_executor.hpp"

namespace gridtools::cid {
    namespace cid_impl_ {
        template <class T, size_t ElemSize = sizeof(std::remove_all_extents_t<T>)>
        struct get_array_strides {
            using type = tuple<>;
        };

        template <class Inner, size_t ElemSize>
        struct get_array_strides<Inner[], ElemSize> {
            using type = meta::push_front<typename get_array_strides<Inner, ElemSize>::type,
                integral_constant<ptrdiff_t, sizeof(Inner) / ElemSize>>;
        };

        template <class Inner, size_t N, size_t ElemSize>
        struct get_array_strides<Inner[N], ElemSize> : get_array_strides<Inner[], ElemSize> {};
    } // namespace cid_impl_

    inline constexpr struct origin_fn {
        template <class Cid>
        auto operator()(Cid &cid) const {
            if constexpr (std::tag_invocable<origin_fn, Cid>) {
                return std::tag_invoke(*this, cid);
            } else if constexpr (std::is_array_v<Cid>) {
                return (std::add_pointer_t<std::remove_all_extents_t<Cid>>)cid;
            }
        }
    } origin{};

    inline constexpr struct strides_fn {
        template <class Cid>
        auto operator()(Cid const &cid) const {
            if constexpr (std::tag_invocable<origin_fn, Cid>) {
                return std::tag_invoke(*this, cid);
            } else if constexpr (std::is_array_v<Cid>) {
                return typename cid_impl_::get_array_strides<Cid>::type{};
            }
        }
    } strides{};

    inline constexpr struct shift_fn {
        template <class Ptr, class Stride, class Offset>
        void operator()(Ptr &&ptr, Stride const &stride, Offset const &offset) const {
            if constexpr (std::tag_invocable<shift_fn, Ptr, Stride, Offset>) {
                std::tag_invoke(*this, std::forward<Ptr>(ptr), stride, offset);
            } else {
                ptr += stride * offset;
            }
        }
    } shift{};
} // namespace gridtools::cid

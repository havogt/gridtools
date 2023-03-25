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
#include <utility>

#include "../common/hymap.hpp"
#include "../common/tuple.hpp"
#include "../meta.hpp"
#include "concept.hpp"
#include "delegate.hpp"
// #include "multi_shift.hpp"

namespace gridtools {
    namespace sid {
        namespace dimension_to_array_impl_ {
            template <class>
            struct assign_helper;
            template <size_t... Is>
            struct assign_helper<std::index_sequence<Is...>> {
                template <class L, class R>
                static GT_FUNCTION void apply(L &&lhs, R &&rhs) {
                    (..., (tuple_util::get<Is>(lhs) = tuple_util::get<Is>(rhs)));
                }
            };

            template <class OrigPtr, class Stride>
            struct array_view {
                OrigPtr m_orig_ptr;
                Stride m_stride;

#if defined(__cpp_deduction_guides) and __cpp_deduction_guides >= 201907L
#else
                // no deduction guide for aggregate prior to c++20
                array_view(OrigPtr orig_ptr, Stride stride) : m_orig_ptr(orig_ptr), m_stride(stride) {}
#endif

                GT_FUNCTION constexpr auto const &operator[](size_t i) const {
                    return *gridtools::sid::shifted(m_orig_ptr, m_stride, i);
                }
                GT_FUNCTION auto &operator[](size_t i) { return *gridtools::sid::shifted(m_orig_ptr, m_stride, i); }

                template <class TupleLike>
                GT_FUNCTION std::enable_if_t<is_tuple_like<TupleLike>::value, void> /*TODO*/ operator=(
                    TupleLike const &t) {
                    assign_helper<std::make_index_sequence<tuple_util::size<TupleLike>::value>>::apply(
                        *this, t); // TODO simplify
                }
            };

            template <size_t I, class Ptr, class Stride>
            GT_FUNCTION auto &get(array_view<Ptr, Stride> &arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return arr[I];
            }

            template <size_t I, class Ptr, class Stride>
            GT_FUNCTION constexpr const auto &get(const array_view<Ptr, Stride> &arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return arr[I];
            }

            template <size_t I, class Ptr, class Stride>
            GT_FUNCTION constexpr auto &&get(array_view<Ptr, Stride> &&arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return std::move(get<I>(arr));
            }

            struct getter {
                template <size_t I, class Ptr, class Stride>
                static GT_FUNCTION auto &get(array_view<Ptr, Stride> &arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return arr[I];
                }

                template <size_t I, class Ptr, class Stride>
                static GT_FUNCTION constexpr const auto &get(const array_view<Ptr, Stride> &arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return arr[I];
                }

                template <size_t I, class Ptr, class Stride>
                static GT_FUNCTION constexpr auto &&get(array_view<Ptr, Stride> &&arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return std::move(get<I>(arr));
                }
            };

            template <class Ptr, class Stride>
            getter tuple_getter(array_view<Ptr, Stride> const &);

            template <class OrigPtr, class Stride>
            struct ptr {
                OrigPtr m_orig_ptr;
                Stride const &m_stride;

                array_view<OrigPtr, Stride> operator*() { return {m_orig_ptr, m_stride}; }

                array_view<OrigPtr, Stride> operator*() const { return {m_orig_ptr, m_stride}; }

                template <class Arg>
                constexpr auto friend operator+(ptr const &obj, Arg &&arg) {
                    return ptr{obj.m_orig_ptr + std::forward<Arg>(arg), obj.m_stride};
                }

                constexpr auto friend operator-(ptr const &lhs, ptr const &rhs) {
                    return lhs.m_orig_ptr - rhs.m_orig_ptr;
                }

                template <class StrideT, class Offset>
                constexpr auto friend sid_shift(ptr &obj, StrideT const &stride, Offset const &o) {
                    return sid::shift(obj.m_orig_ptr, stride, o);
                }
            };

            template <class OrigPtrHolder, class Strides>
            struct ptr_holder {
                OrigPtrHolder m_orig_ptr_holder;
                Strides m_strides;

                auto operator()() {
                    return ptr<std::decay_t<decltype(std::declval<OrigPtrHolder>()())>, Strides>{
                        // return ptr<std::remove_cvref_t<decltype(std::declval<OrigPtrHolder>()())>, Strides>{
                        m_orig_ptr_holder(),
                        m_strides};
                }

                template <class Arg>
                friend constexpr GT_FUNCTION ptr_holder operator+(ptr_holder const &obj, Arg &&offset) {
                    return {obj.m_impl + std::forward<Arg>(offset), obj.m_strides};
                }
            };

            template <class Dim, class Sid>
            struct reinterpreted_sid : sid::delegate<Sid> {
                friend auto sid_get_origin(reinterpreted_sid &obj) {
                    return ptr_holder<sid::ptr_holder_type<Sid>,
                        std::decay_t<decltype(at_key<Dim>(sid::get_strides(obj.m_impl)))>>{
                        sid::get_origin(obj.m_impl), at_key<Dim>(sid::get_strides(obj.m_impl))};
                }

                friend auto sid_get_strides(reinterpreted_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_strides(obj.m_impl));
                }

                using sid::delegate<Sid>::delegate;
            };
        } // namespace dimension_to_array_impl_

        template <class Dim, class Sid>
        dimension_to_array_impl_::reinterpreted_sid<Dim, Sid> dimension_to_array(Sid &&sid) {
            return {std::forward<Sid>(sid)};
        }
    } // namespace sid
} // namespace gridtools

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
#include "concept.hpp"
#include "delegate.hpp"
#include "gridtools/common/host_device.hpp"
// #include "multi_shift.hpp"

namespace gridtools {
    template <class Sid>
    constexpr void is_sid_separate() {
        using PtrHolder = sid::ptr_holder_type<Sid>;
        using StridesType = sid::strides_type<Sid>;
        using PtrDiff = sid::ptr_diff_type<Sid>;
        using Ptr = sid::ptr_type<Sid>;
        using ReferenceType = sid::reference_type<Sid>;
        using StrideTypeList = tuple_util::traits::to_types<std::decay_t<StridesType>>;
        using StridesKind = sid::strides_kind<Sid>;
        using LowerBoundsType = sid::lower_bounds_type<Sid>;
        using UpperBoundsType = sid::upper_bounds_type<Sid>;

        // `is_trivially_copyable` check is applied to the types that are will be
        // passed from host to device
        static_assert(std::is_trivially_copy_constructible<StridesType>{});
        // auto tmp = std::declval<PtrHolder>();
        // using OrigPtrHolder = decltype(tmp.m_orig_ptr_holder);
        // using Stride = decltype(tmp.m_stride);
        // static_assert(std::is_trivially_copy_constructible<OrigPtrHolder>{});
        // static_assert(std::is_trivially_copy_constructible<Stride>{});
        static_assert(std::is_trivially_copy_constructible<PtrHolder>{});

        // verify that `PtrDiff` is sane
        static_assert(std::is_default_constructible<PtrDiff>{});
        static_assert(
            std::is_convertible<decltype(std::declval<Ptr const &>() + std::declval<PtrDiff const &>()), Ptr>{});

        // `PtrHolder` supports `+` as well
        // TODO(anstaf): we can do better here: verify that if we transform
        // PtrHolder that way the result
        //               thing also models SID
        static_assert(
            std::is_same<std::decay_t<decltype(std::declval<PtrHolder const &>() + std::declval<PtrDiff const &>())>,
                PtrHolder>{});

        // verify that `Reference` is sane
        static_assert(std::negation<std::is_void<ReferenceType>>{});

        // static_assert(
        //     meta::all_of<sid::concept_impl_::is_valid_stride<Sid>::template apply,
        //                  StrideTypeList>{});
        // all strides must be applied via `shift` with both `Ptr` and
        // `PtrDiff`
        static_assert(sid::concept_impl_::are_valid_strides<StrideTypeList, Ptr>{});
        static_assert(sid::concept_impl_::are_valid_strides<StrideTypeList, PtrDiff>{});

        static_assert(
            sid::concept_impl_::are_valid_bounds<tuple_util::traits::to_types<std::decay_t<LowerBoundsType>>>{});
        static_assert(
            sid::concept_impl_::are_valid_bounds<tuple_util::traits::to_types<std::decay_t<UpperBoundsType>>>{});
    }
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

            template <class>
            struct init_helper;
            template <size_t... Is>
            struct init_helper<std::index_sequence<Is...>> {
                template <class Fun>
                static GT_FUNCTION auto apply(Fun &&fun) {
                    return array{fun(Is)...};
                }
            };

            template <class T, size_t N>
            struct ref_array {
                array<std::reference_wrapper<T>, N> m_arr;

                GT_FUNCTION constexpr T const &operator[](size_t i) const { return m_arr[i]; }
                GT_FUNCTION T &operator[](size_t i) { return m_arr[i]; }

                template <class TupleLike>
                GT_FUNCTION ref_array &operator=(TupleLike const &t) { // TODO enable only for tuple_like
                    assign_helper<std::make_index_sequence<tuple_util::size<TupleLike>::value>>::apply(
                        *this, t); // TODO simplify
                    return *this;
                }
            };

            template <size_t I, class Ptr, size_t N>
            GT_FUNCTION auto &get(ref_array<Ptr, N> &arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return arr[I];
            }

            template <size_t I, class Ptr, size_t N>
            GT_FUNCTION constexpr const auto &get(const ref_array<Ptr, N> &arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return arr[I];
            }

            template <size_t I, class Ptr, size_t N>
            GT_FUNCTION constexpr auto &&get(ref_array<Ptr, N> &&arr) noexcept {
                // static_assert(I < D, "index is out of bounds");
                return std::move(get<I>(arr));
            }

            struct getter_ref_array {
                template <size_t I, class Ptr, size_t N>
                static GT_FUNCTION auto &get(ref_array<Ptr, N> &arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return arr[I];
                }

                template <size_t I, class Ptr, size_t N>
                static GT_FUNCTION constexpr const auto &get(const ref_array<Ptr, N> &arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return arr[I];
                }

                template <size_t I, class Ptr, size_t N>
                static GT_FUNCTION constexpr auto &&get(ref_array<Ptr, N> &&arr) noexcept {
                    // static_assert(I < D, "index is out of bounds");
                    return std::move(get<I>(arr));
                }
            };

            template <class Ptr, size_t N>
            getter_ref_array tuple_getter(ref_array<Ptr, N> const &);

            template <bool LValue>
            struct ptr_array_deref_helper;
            template <>
            struct ptr_array_deref_helper<true> {
                template <class Ptr, class Stride, size_t N>
                static auto apply(Ptr &ptr, Stride const &stride) {
                    return ref_array<std::remove_pointer_t<Ptr>, N>{
                        init_helper<std::make_index_sequence<N>>::template apply<>(
                            [&](auto i) { return std::ref(*sid::shifted(ptr, stride, i)); })};
                }
            };
            template <>
            struct ptr_array_deref_helper<false> {
                template <class Ptr, class Stride, size_t N>
                static auto apply(Ptr const &ptr, Stride const &stride) {
                    return init_helper<std::make_index_sequence<N>>::template apply<>(
                        [&](auto i) { return *sid::shifted(ptr, stride, i); });
                }
            };

            template <class Ptr, class Stride, size_t N>
            struct ptr_array {
                Ptr m_ptr;
                Stride m_stride;

                // TODO
                // #if defined(__cpp_deduction_guides) and __cpp_deduction_guides >= 201907L
                // #else
                // no deduction guide for aggregate prior to c++20
                // #endif
                ptr_array(Ptr ptr, Stride const &stride) : m_ptr(ptr), m_stride(stride) {}
                ptr_array() = default;

                template <class StrideT, class Offset>
                constexpr void friend sid_shift(ptr_array &obj, StrideT const &stride, Offset const &o) {
                    // static_assert(sizeof(StrideT) < 0);
                    // obj.m_ptr =
                    sid::shift(obj.m_ptr, stride, o);
                    // return ptr_array{sid::shifted(obj.m_ptr, stride, o), obj.m_stride};
                }

                auto operator*() { // if the derefed ptr is an lvalue we return a array of references, otherwise an
                                   // array of values
                    return ptr_array_deref_helper<std::is_lvalue_reference<decltype(*(this->m_ptr))>::value>::
                        template apply<Ptr, Stride, N>(this->m_ptr, this->m_stride);
                }
                auto operator*() const {
                    // return tuple_util::transform([](auto &&elem) { return *elem; }, this->m_arr);
                    return ptr_array_deref_helper<false>::template apply<Ptr, Stride, N>(this->m_ptr, this->m_stride);
                }

                template <class Arg>
                constexpr auto friend operator+(ptr_array const &obj, Arg &&arg) {
                    return ptr_array{obj.m_ptr + std::forward<Arg>(arg), obj.m_stride};
                }

                constexpr auto friend operator-(ptr_array const &lhs, ptr_array const &rhs) {
                    return lhs.m_ptr - rhs.m_ptr;
                }
            };

            // template <class Ptr, size_t N>
            // struct ptr_array {
            //     array<Ptr, N> m_arr;
            //     // OrigPtr m_orig_ptr;
            //     // Stride m_stride;

            //     // #if defined(__cpp_deduction_guides) and __cpp_deduction_guides >= 201907L
            //     // #else
            //     // no deduction guide for aggregate prior to c++20
            //     // #endif

            //     // GT_FUNCTION constexpr auto const &operator[](size_t i) const { return *m_arr[i]; }
            //     // GT_FUNCTION auto &operator[](size_t i) { return *m_arr[i]; }

            //     template <class StrideT, class Offset>
            //     constexpr auto friend sid_shift(ptr_array &obj, StrideT const &stride, Offset const &o) {
            //         return ptr_array{tuple_util::transform(
            //             [stride, o](
            //                 auto &&elem) { return sid::shifted(std::forward<decltype(elem)>(elem), stride, o); },
            //             obj.m_arr)};
            //         // return sid::shift(obj.m_orig_ptr, stride, o);
            //     }

            //     auto operator*() {
            //         // TODO: if the underlying thing is lvalue do this, else do the other
            //         return ref_array<std::remove_pointer_t<Ptr>, N>{
            //             init_helper<std::make_index_sequence<N>>::template apply<
            //                 std::reference_wrapper<std::remove_pointer_t<Ptr>>>(
            //                 [&](auto i) { return std::ref(*((this->m_arr)[i])); })};
            //     }
            //     auto operator*() const {
            //         return tuple_util::transform([](auto &&elem) { return *elem; }, this->m_arr);
            //     }
            //     // ptr_array operator*() const { return *this; }

            //     // template <class Arg>
            //     // constexpr auto friend operator+(ptr_array const &obj, Arg &&arg) {
            //     //     return ptr_array{tuple_util::transform(
            //     //         [arg = std::forward<Arg>(arg)](auto &&elem) { return elem + std::forward<Arg>(arg); },
            //     //         obj.m_arr)};
            //     //     // return ptr{obj.m_orig_ptr + std::forward<Arg>(arg), obj.m_stride};
            //     // }

            //     // constexpr auto friend operator-(ptr_array const &lhs, ptr_array const &rhs) {
            //     //     return ptr_array{
            //     //         tuple_util::transform([](auto &&l, auto &&r) { return l - r; }, lhs.m_arr, rhs.m_arr)};
            //     //     // return lhs.m_orig_ptr - rhs.m_orig_ptr;
            //     // }
            // };

            // template <class Ptr, size_t N, class Stride>
            // ptr_array<Ptr, N> init_ptr_array(Ptr orig_ptr, Stride const &stride) {
            //     return {init_helper<std::make_index_sequence<N>>::template apply<Ptr>(
            //         [orig_ptr, stride](auto i) { return &(*shifted(orig_ptr, stride, i)); })};
            // }

            // template <size_t I, class Ptr, size_t N>
            // GT_FUNCTION auto &get(ptr_array<Ptr, N> &arr) noexcept {
            //     // static_assert(I < D, "index is out of bounds");
            //     return arr[I];
            // }

            // template <size_t I, class Ptr, size_t N>
            // GT_FUNCTION constexpr const auto &get(const ptr_array<Ptr, N> &arr) noexcept {
            //     // static_assert(I < D, "index is out of bounds");
            //     return arr[I];
            // }

            // template <size_t I, class Ptr, size_t N>
            // GT_FUNCTION constexpr auto &&get(ptr_array<Ptr, N> &&arr) noexcept {
            //     // static_assert(I < D, "index is out of bounds");
            //     return std::move(get<I>(arr));
            // }

            // struct getter_ptr_array {
            //     template <size_t I, class Ptr, size_t N>
            //     static GT_FUNCTION auto &get(ptr_array<Ptr, N> &arr) noexcept {
            //         // static_assert(I < D, "index is out of bounds");
            //         return arr[I];
            //     }

            //     template <size_t I, class Ptr, size_t N>
            //     static GT_FUNCTION constexpr const auto &get(const ptr_array<Ptr, N> &arr) noexcept {
            //         // static_assert(I < D, "index is out of bounds");
            //         return arr[I];
            //     }

            //     template <size_t I, class Ptr, size_t N>
            //     static GT_FUNCTION constexpr auto &&get(ptr_array<Ptr, N> &&arr) noexcept {
            //         // static_assert(I < D, "index is out of bounds");
            //         return std::move(get<I>(arr));
            //     }
            // };

            // template <class Ptr, size_t N>
            // getter_ptr_array tuple_getter(ptr_array<Ptr, N> const &);

            // template <class OrigPtr, class Stride, size_t N>
            // struct ptr {
            //     ptr_array<OrigPtr, N> m_ptr_arr;
            //     // OrigPtr m_orig_ptr;
            //     // Stride const &m_stride;

            //     GT_FUNCTION constexpr ptr(OrigPtr orig_ptr, Stride stride)
            //         : m_ptr_arr(init_ptr_array<OrigPtr, N>(orig_ptr, stride)) {}

            //     GT_FUNCTION constexpr ptr(ptr_array<OrigPtr, N> ptr_arr) : m_ptr_arr{ptr_arr} {}
            //     ptr() = default;

            //     // auto operator*() { return init_ptr_array<OrigPtr, N>(m_orig_ptr, m_stride); }
            //     auto operator*() const { return m_ptr_arr; }

            //     // array_view<OrigPtr, Stride> operator*() const { return {m_orig_ptr, m_stride}; }

            //     template <class Arg>
            //     constexpr auto friend operator+(ptr const &obj, Arg &&arg) {
            //         return obj; // ptr{obj.m_ptr_arr + std::forward<Arg>(arg), obj.m_stride};
            //     }

            //     constexpr auto friend operator-(ptr const &lhs, ptr const &rhs) {
            //         return lhs; // ptr{lhs.m_ptr_arr - rhs.m_ptr_arr};
            //     }

            //     template <class StrideT, class Offset>
            //     constexpr auto friend sid_shift(ptr &obj, StrideT const &stride, Offset const &o) {
            //         return sid::shift(obj.m_ptr_arr, stride, o);
            //     }
            // };

            template <class OrigPtrHolder, class Stride, size_t N>
            struct ptr_holder {
                OrigPtrHolder m_orig_ptr_holder;
                Stride m_stride;

                auto operator()() const {
                    return ptr_array<std::decay_t<decltype(std::declval<OrigPtrHolder>()())>, Stride, N>{
                        // return ptr<std::remove_cvref_t<decltype(std::declval<OrigPtrHolder>()())>, Strides>{
                        m_orig_ptr_holder(),
                        m_stride};
                }

                template <class Arg>
                friend constexpr GT_FUNCTION ptr_holder operator+(ptr_holder const &obj, Arg &&offset) {
                    return {obj.m_orig_ptr_holder + std::forward<Arg>(offset), obj.m_stride};
                }
            };

            template <class Dim, class Sid, size_t N> // TODO make optional
                                                      // in case of static sid bounds?
            struct reinterpreted_sid : sid::delegate<Sid> {
                friend auto sid_get_origin(reinterpreted_sid &obj) {
                    return ptr_holder<sid::ptr_holder_type<Sid>,
                        std::decay_t<decltype(at_key<Dim>(sid::get_strides(obj.m_impl)))>,
                        N>{sid::get_origin(obj.m_impl), at_key<Dim>(sid::get_strides(obj.m_impl))};
                }

                friend auto sid_get_strides(reinterpreted_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_strides(obj.m_impl));
                }
                friend auto sid_get_lower_bounds(reinterpreted_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_lower_bounds(obj.m_impl));
                }
                friend auto sid_get_upper_bounds(reinterpreted_sid const &obj) {
                    return hymap::canonicalize_and_remove_key<Dim>(sid::get_upper_bounds(obj.m_impl));
                }

                using sid::delegate<Sid>::delegate;
            };
        } // namespace dimension_to_array_impl_

        template <class Dim, size_t N, class Sid> // N optional?
        dimension_to_array_impl_::reinterpreted_sid<Dim, Sid, N> dimension_to_array(Sid &&sid) {
            return {std::forward<Sid>(sid)};
        }
    } // namespace sid
} // namespace gridtools

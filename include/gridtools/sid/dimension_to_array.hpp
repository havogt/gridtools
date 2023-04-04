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

            template <class>
            struct init_helper;
            template <size_t... Is>
            struct init_helper<std::index_sequence<Is...>> {
                template <class Fun>
                static GT_FUNCTION auto apply(Fun &&fun) {
                    return array{fun(Is)...};
                }
            };

	    namespace detail {
template <class T> constexpr GT_FUNCTION T& FUN(T& t) noexcept { return t; }
template <class T> void FUN(T&&) = delete;
}

template <class T>
class reference_wrapper
{
public:
    // types
    using type = T;

    // construct/copy/destroy
    template <class U, class = decltype(
        detail::FUN<T>(std::declval<U>()),
        std::enable_if_t<!std::is_same_v<reference_wrapper, std::remove_cv_t<std::remove_reference_t<U>>>>()
    )>
    constexpr GT_FUNCTION reference_wrapper(U&& u)
        noexcept(noexcept(detail::FUN<T>(std::forward<U>(u))))
        : _ptr(std::addressof(detail::FUN<T>(std::forward<U>(u)))) {}

    reference_wrapper(const reference_wrapper&) noexcept = default;

    // assignment
    reference_wrapper& operator=(const reference_wrapper& x) noexcept = default;

    // access
    constexpr GT_FUNCTION operator T& () const noexcept { return *_ptr; }
    constexpr GT_FUNCTION T& get() const noexcept { return *_ptr; }

    template< class... ArgTypes >
    constexpr GT_FUNCTION std::invoke_result_t<T&, ArgTypes...>
        operator() ( ArgTypes&&... args ) const
            noexcept(std::is_nothrow_invocable_v<T&, ArgTypes...>)
    {
        return std::invoke(get(), std::forward<ArgTypes>(args)...);
    }

private:
    T* _ptr;
};

// deduction guides
template<class T>
reference_wrapper(T&) -> reference_wrapper<T>;

            template <class T, size_t N>
            struct ref_array {
                array<reference_wrapper<T>, N> m_arr;

                GT_FUNCTION constexpr T const &operator[](size_t i) const { return m_arr[i]; }
                GT_FUNCTION T &operator[](size_t i) { return m_arr[i]; }

                GT_FUNCTION ref_array &operator=(ref_array const &t) {
                    assign_helper<std::make_index_sequence<N>>::apply(*this, t);
                    return *this;
                }

                template <class TupleLike>
                GT_FUNCTION ref_array &operator=(TupleLike const &t) { // TODO enable only for tuple_like
                    assign_helper<std::make_index_sequence<N>>::apply(*this, t);
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
                static GT_FUNCTION auto apply(Ptr &ptr, Stride const &stride) {
                    return ref_array<std::remove_pointer_t<Ptr>, N>{
                        init_helper<std::make_index_sequence<N>>::template apply<>(
                            [&](auto i) { return reference_wrapper(*sid::shifted(ptr, stride, i)); })};
                }
            };
            template <>
            struct ptr_array_deref_helper<false> {
                template <class Ptr, class Stride, size_t N>
                static GT_FUNCTION auto apply(Ptr const &ptr, Stride const &stride) {
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
                GT_FUNCTION ptr_array(Ptr ptr, Stride const &stride) : m_ptr(ptr), m_stride(stride) {}
                ptr_array() = default;

                template <class StrideT, class Offset>
                constexpr GT_FUNCTION void friend sid_shift(ptr_array &obj, StrideT const &stride, Offset const &o) {
                    // static_assert(sizeof(StrideT) < 0);
                    // obj.m_ptr =
                    sid::shift(obj.m_ptr, stride, o);
                    // return ptr_array{sid::shifted(obj.m_ptr, stride, o), obj.m_stride};
                }

                GT_FUNCTION auto operator*() { // if the derefed ptr is an lvalue we return a array of references, otherwise an
                                   // array of values
                    return ptr_array_deref_helper<std::is_lvalue_reference<decltype(*(this->m_ptr))>::value>::
                        template apply<Ptr, Stride, N>(this->m_ptr, this->m_stride);
                }
                GT_FUNCTION auto operator*() const {
                    // return tuple_util::transform([](auto &&elem) { return *elem; }, this->m_arr);
                    return ptr_array_deref_helper<false>::template apply<Ptr, Stride, N>(this->m_ptr, this->m_stride);
                }

                template <class Arg>
                constexpr GT_FUNCTION auto friend operator+(ptr_array const &obj, Arg &&arg) {
                    return ptr_array{obj.m_ptr + std::forward<Arg>(arg), obj.m_stride};
                }

                constexpr GT_FUNCTION auto friend operator-(ptr_array const &lhs, ptr_array const &rhs) {
                    return lhs.m_ptr - rhs.m_ptr;
                }
            };

            template <class OrigPtrHolder, class Stride, size_t N>
            struct ptr_holder {
                OrigPtrHolder m_orig_ptr_holder;
                Stride m_stride;

                GT_FUNCTION auto operator()() const {
                    return ptr_array<std::decay_t<decltype(std::declval<OrigPtrHolder>()())>, Stride, N>{
                        m_orig_ptr_holder(), m_stride};
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

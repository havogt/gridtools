/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "experimental/__p0009_bits/layout_right.hpp"
#include "gridtools/meta/debug.hpp"
#include "gridtools/sid/concept.hpp"
#include <cstddef>
#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <experimental/mdspan>
#include <gridtools/sid/simple_ptr_holder.hpp>
#include <gridtools/sid/unknown_kind.hpp>
#include <gridtools/stencil/cartesian.hpp>
#include <gridtools/stencil/naive.hpp>
#include <utility>

using namespace gridtools;
using namespace stencil;
using namespace cartesian;

struct copy {
    using in = in_accessor<0>;
    using out = inout_accessor<1>;
    using param_list = make_param_list<in, out>;
    template <class Eval>
    GT_FUNCTION static void apply(Eval &&eval) {
        eval(out()) = eval(in());
    }
};

template <class T>
auto grid(T const &) {
    static_assert(std::rank<T>() == 3);
    return make_grid(std::extent<T, 0>(), std::extent<T, 1>(), std::extent<T, 2>());
}

namespace impl {

    template <class T>
    struct construct_strides_helper;

    template <std::size_t... Is>
    struct construct_strides_helper<std::index_sequence<Is...>> {
        template <class T>
        constexpr auto operator()(T const &t) {
            return tuple{t.stride(Is)...};
        }
    };

    template <class T>
    struct construct_strides_helper_right;

    template <std::size_t... Is>
    struct construct_strides_helper_right<std::index_sequence<Is...>> {
        template <class T>
        constexpr auto operator()(T const &t) {
            return tuple{t.stride(Is)..., std::integral_constant<std::size_t, 1>{}};
        }
    };
    // template <class Extents, std::size_t... Is>
    // struct construct_strides<std::experimental::layout_right, Extents, std::index_sequence<Is...>> {
    //     template <class T>
    //     constexpr auto operator()(T const &t) {
    //         return tuple{t.stride(Is)...};
    //     }
    // };

    template <class ElementType, class Extents, class LayoutPolicy, class AccessorPolicy>
    auto construct_strides(std::experimental::mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy> const &t) {
        return construct_strides_helper<std::make_index_sequence<
            std::experimental::mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy>::rank()>>{}(t);
    }
    template <class ElementType, class Extents, class AccessorPolicy>
    auto construct_strides(
        std::experimental::mdspan<ElementType, Extents, std::experimental::layout_right, AccessorPolicy> const &t) {
        return construct_strides_helper_right<std::make_index_sequence<std::experimental::
                mdspan<ElementType, Extents, std::experimental::layout_right, AccessorPolicy>::rank()>>{}(t);
    }
} // namespace impl

namespace std::experimental {
    template <class ElementType, class Extents, class LayoutPolicy, class AccessorPolicy>
    auto sid_get_origin(mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy> &t) {
        return gridtools::sid::make_simple_ptr_holder(t.data());
    }
    template <class ElementType, class Extents, class LayoutPolicy, class AccessorPolicy>
    auto sid_get_strides(std::experimental::mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy> const &t) {
        return impl::construct_strides(t);
    }

    template <class ElementType, class Extents, class LayoutPolicy, class AccessorPolicy>
    typename mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy>::ptrdiff_t sid_get_ptr_diff(
        mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy> const &);

    template <class ElementType, class Extents, class LayoutPolicy, class AccessorPolicy>
    gridtools::sid::unknown_kind sid_get_strides_kind(
        mdspan<ElementType, Extents, LayoutPolicy, AccessorPolicy> const &);

    // TODO bounds
} // namespace std::experimental

TEST(mdspan, smoke) {
    int out[3][4][5];

    auto s = std::experimental::mdspan(&out[0][0][0], std::experimental::extents<3, 4, 5>{});

    auto strides = sid::get_strides(s);

    // GT_META_PRINT_TYPE(decltype(sid::get_strides(s)));
    static_assert(is_sid<decltype(s)>::value);

    // GT_META_PRINT_TYPE(decltype(s));

    decltype(out) in;
    for (auto &&vvv : in)
        for (auto &&vv : vvv)
            for (auto &&v : vv)
                v = 42;
    // TODO
    run_single_stage(copy(), naive(), grid(out), in, s);
    EXPECT_THAT(out, testing::ContainerEq(in));
}

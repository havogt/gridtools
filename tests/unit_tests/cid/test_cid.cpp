
/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gridtools/cid/cid.hpp>

#include <type_traits>

#include <gtest/gtest.h>

#include <gridtools/common/integral_constant.hpp>
#include <gridtools/common/tuple.hpp>
#include <gridtools/common/tuple_util.hpp>

namespace basic {
    struct my_basic_cid {
        double *m_ptr;
        friend double *tag_invoke(std::tag_t<gridtools::cid::origin>, my_basic_cid const &cid) noexcept {
            return cid.m_ptr;
        }
        friend gridtools::tuple<int> tag_invoke(std::tag_t<gridtools::cid::strides>, my_basic_cid const &cid) noexcept {
            return {1};
        }
    };

    TEST(basics, foo) {
        namespace tu = gridtools::tuple_util;

        double a[4] = {1, 2, 3, 4};
        my_basic_cid cid{a};

        ASSERT_EQ(a[0], *gridtools::cid::origin(cid));

        *gridtools::cid::origin(cid) = 5;
        ASSERT_EQ(a[0], 5);

        auto ptr = gridtools::cid::origin(cid);
        auto strides = gridtools::cid::strides(cid);
        gridtools::cid::shift(ptr, tu::get<0>(strides), 1);
        ASSERT_EQ(a[1], *ptr);
    }
} // namespace basic
namespace custom_shift {

    struct custom_shift_cid {
        struct ptr {
            int m_val = 0;

            int operator*() { return m_val; }
        };

        friend ptr tag_invoke(std::tag_t<gridtools::cid::origin>, custom_shift_cid const &cid) noexcept { return {}; }
        friend gridtools::tuple<int> tag_invoke(
            std::tag_t<gridtools::cid::strides>, custom_shift_cid const &cid) noexcept {
            return {};
        }
        template <class Stride, class Offset>
        friend void tag_invoke(std::tag_t<gridtools::cid::shift>, ptr &ptr_, Stride const &, Offset offset) noexcept {
            ptr_.m_val += offset;
        }
    };
    TEST(custom_shift, cid) {
        namespace tu = gridtools::tuple_util;
        custom_shift_cid index{};

        auto ptr = gridtools::cid::origin(index);
        ASSERT_EQ(0, *ptr);

        gridtools::cid::shift(ptr, tu::get<0>(gridtools::cid::strides(index)), 5);
        ASSERT_EQ(5, *ptr);
    }
} // namespace custom_shift

namespace gridtools {
    namespace {
        namespace tu = tuple_util;
        TEST(c_array, bla) {
            int a[2][3] = {{1, 2, 3}, {4, 5, 6}};

            auto ptr = cid::origin(a);
            auto strides = cid::strides(a);

            ASSERT_EQ(a[0][0], *ptr);
            cid::shift(ptr, tu::get<0>(strides), 1);
            ASSERT_EQ(a[1][0], *ptr);
            cid::shift(ptr, tu::get<1>(strides), 2);
            ASSERT_EQ(a[1][2], *ptr);
        }
    } // namespace
} // namespace gridtools

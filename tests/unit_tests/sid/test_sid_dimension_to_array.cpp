/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "gridtools/meta/debug.hpp"
#include <gridtools/sid/multi_shift.hpp>

#include <gtest/gtest.h>

#include <gridtools/common/array.hpp>
#include <gridtools/common/integral_constant.hpp>
#include <gridtools/common/tuple.hpp>
#include <gridtools/sid/dimension_to_array.hpp>

namespace gridtools {
    namespace {
        using namespace literals;
        TEST(ptr_array, smoke) {
            double data[15][42];

            auto testee = sid::dimension_to_array_impl_::init_ptr_array<double *, 15>(
                &data[0][0], std::integral_constant<int, 42>{});
            // ptr_array<double *, 15> testee(
            // );

            EXPECT_EQ(data[1][0], testee[1]);
            EXPECT_EQ(&data[1][0], &testee[1]);
            EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
            using std::get;
            EXPECT_EQ(&data[3][0], &get<3>(testee));

            testee = array<double, 15>{};
        }
        TEST(ptr_array, nested) {
            double data[15][42];

            auto inner = sid::dimension_to_array_impl_::init_ptr_array<double *, 15>(
                &data[0][0], std::integral_constant<int, 42>{});
            auto testee = sid::dimension_to_array_impl_::init_ptr_array<decltype(inner), 42>(
                inner, std::integral_constant<int, 1>{});

            EXPECT_TRUE(&testee);

            // GT_META_PRINT_TYPE(decltype(testee[0]))

            EXPECT_EQ(data[1][0], testee[0][1]);
            EXPECT_EQ(&data[1][0], &testee[0][1]);

            testee[0] = array<double, 15>{};
            testee = array<array<double, 15>, 42>{};
        }

        TEST(dimension_to_array, smoke) {
            double data[15][42];
            auto testee = sid::dimension_to_array<gridtools::integral_constant<int, 0>, 15>(data);

            is_sid_separate<decltype(testee)>();

            auto ptr = sid::get_origin(testee)();
            auto strides = sid::get_strides(testee);

            static_assert(tuple_util::size<decltype(strides)>{} == 1);
            static_assert(std::is_same_v<std::decay_t<decltype(tuple_util::get<0>(strides))>,
                gridtools::integral_constant<long, 1>>);

            EXPECT_EQ(data[1][0], (*ptr)[1]);

            EXPECT_EQ(data[2][3], (*sid::shifted(ptr, tuple_util::get<0>(strides), 3))[2]);

            (*sid::shifted(ptr, tuple_util::get<0>(strides), 4))[3] = 42;
            EXPECT_EQ(42, data[3][4]);
        }

        TEST(dimension_to_array, assignable_from_tuple_like) {
            double data[2][5];
            auto testee = sid::dimension_to_array<gridtools::integral_constant<int, 0>, 2>(data);

            static_assert(is_sid<decltype(testee)>::value);

            auto ptr = sid::get_origin(testee)();

            *ptr = tuple(2., 3.);
            EXPECT_EQ((*ptr)[0], 2.);
            EXPECT_EQ((*ptr)[1], 3.);
        }

        TEST(dimension_to_array, nested) {
            double data[2][3];
            auto testee = sid::dimension_to_array<integral_constant<int, 1>, 3>(
                sid::dimension_to_array<integral_constant<int, 0>, 2>(data));

            static_assert(is_sid<decltype(testee)>::value);

            auto ptr = sid::get_origin(testee)();

            auto derefed = *ptr;
            auto outer = derefed[0];
            GT_META_PRINT_TYPE(decltype(outer));

            // EXPECT_EQ(&data[1][2], &derefed[2][1]);
        }

    } // namespace
} // namespace gridtools

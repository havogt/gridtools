/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gridtools/sid/multi_shift.hpp>

#include <gtest/gtest.h>

#include <gridtools/common/integral_constant.hpp>
#include <gridtools/common/tuple.hpp>
#include <gridtools/sid/dimension_to_array.hpp>

namespace gridtools {
    namespace {
        using namespace literals;

        TEST(array_view, smoke) {
            double data[15][42];

            sid::dimension_to_array_impl_::array_view testee(&data[0][0], std::integral_constant<int, 42>{});

            // static_assert(is_tuple_like<decltype(testee)>::value); // TODO

            EXPECT_EQ(data[1][0], testee[1]);
            EXPECT_EQ(&data[1][0], &testee[1]);
            EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
            using std::get;
            EXPECT_EQ(&data[3][0], &get<3>(testee));
        }
        // TEST(array_view, nested) {
        //     double data[15][42];

        //     sid::dimension_to_array_impl_::array_view inner(&data[0][0], std::integral_constant<int, 42>{});
        //     sid::dimension_to_array_impl_::array_view testee(inner, std::integral_constant<int, 1>{});

        //     // static_assert(is_tuple_like<decltype(testee)>::value); // TODO
        //     EXPECT_EQ(data[1][2], testee[2][1]);

        //     // EXPECT_EQ(data[1][0], testee[1]);
        //     // EXPECT_EQ(&data[1][0], &testee[1]);
        //     // EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
        //     // using std::get;
        //     // EXPECT_EQ(&data[3][0], &get<3>(testee));
        // }
        TEST(dimension_to_array, smoke) {
            double data[15][42];
            auto testee = sid::dimension_to_array<gridtools::integral_constant<int, 0>>(data);

            static_assert(is_sid<decltype(testee)>::value);

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
            auto testee = sid::dimension_to_array<gridtools::integral_constant<int, 0>>(data);

            static_assert(is_sid<decltype(testee)>::value);

            auto ptr = sid::get_origin(testee)();

            *ptr = tuple(2., 3.);
            EXPECT_EQ((*ptr)[0], 2.);
            EXPECT_EQ((*ptr)[1], 3.);
        }

        // TEST(dimension_to_array, nested) {
        //     double data[2][3];
        //     auto testee = sid::dimension_to_array<integral_constant<int, 1>>(
        //         sid::dimension_to_array<integral_constant<int, 0>>(data));

        //     static_assert(is_sid<decltype(testee)>::value);

        //     auto ptr = sid::get_origin(testee)();

        //     auto derefed = *ptr;

        //     EXPECT_EQ(&data[1][2], &derefed[2][1]);
        // }

    } // namespace
} // namespace gridtools

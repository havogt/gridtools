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

            // TODO remove usage of sid here, to highlight that array_view doesn't require sid (it's just a helper)
            auto ptr = sid::get_origin(data)();
            auto strides = sid::get_strides(data);

            sid::dimension_to_array_impl_::array_view testee(ptr, tuple_util::get<0>(strides));

            // static_assert(is_tuple_like<decltype(testee)>::value); // TODO

            EXPECT_EQ(data[1][0], testee[1]);
            EXPECT_EQ(&data[1][0], &testee[1]);
            EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
            using std::get;
            EXPECT_EQ(&data[3][0], &get<3>(testee));
        }
        TEST(dimension_to_array, smoke) {
            double data[15][42];
            auto testee = sid::dimension_to_array(data, gridtools::integral_constant<int, 0>{});

            static_assert(is_sid<decltype(testee)>::value);

            auto ptr = sid::get_origin(testee)();
            auto strides = sid::get_strides(testee);

            // static_assert(std::tuple_size<decltype(strides)>{} == 1); // TODO
            // static_assert(std::is_same_v<std::decay_t<decltype(tuple_util::get<0>(strides))>,
            //     gridtools::integral_constant<long, 1>>); // TODO

            EXPECT_EQ(data[1][0], (*ptr)[1]);

            EXPECT_EQ(data[2][3], (*sid::shifted(ptr, tuple_util::get<1>(strides), 3))[2]); // TODO should be stride[0]

            (*sid::shifted(ptr, tuple_util::get<1>(strides), 4))[3] = 42; // TODO should be stride[0]
            EXPECT_EQ(42, data[3][4]);
        }

    } // namespace
} // namespace gridtools

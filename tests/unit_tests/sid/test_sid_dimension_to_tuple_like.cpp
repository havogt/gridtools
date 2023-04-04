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
#include <gridtools/sid/dimension_to_tuple_like.hpp>

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
    namespace {
        using namespace literals;
        // TEST(ptr_array, smoke) {
        //     double data[15][42];

        //     auto testee = *(sid::dimension_to_array_impl_::init_ptr_array<double *, 15>(
        //         &data[0][0], std::integral_constant<int, 42>{}));
        //     // ptr_array<double *, 15> testee(
        //     // );

        //     EXPECT_EQ(data[1][0], testee[1]);
        //     EXPECT_EQ(&data[1][0], &testee[1]);
        //     EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
        //     using std::get;
        //     EXPECT_EQ(&data[3][0], &get<3>(testee));

        //     testee = array<double, 15>{};
        // }
        // TEST(ptr_array, nested) {
        //     double data[15][42];

        //     auto inner = sid::dimension_to_array_impl_::init_ptr_array<double *, 15>(
        //         &data[0][0], std::integral_constant<int, 42>{});
        //     auto testee = *(sid::dimension_to_array_impl_::init_ptr_array<decltype(inner), 42>(
        //         inner, std::integral_constant<int, 1>{}));

        //     EXPECT_TRUE(&testee);

        //     GT_META_PRINT_TYPE(decltype(testee[0]))

        //     // EXPECT_EQ(data[1][0], testee[0][1]);
        //     // EXPECT_EQ(&data[1][0], &testee[0][1]);

        //     // testee[0] = array<double, 15>{};
        //     // testee = array<array<double, 15>, 42>{};
        // }
        // TEST(ptr_array, smoke) {
        //     double data[15][42];

        //     auto testee = *sid::dimension_to_array_impl_::ptr_array<double *, std::integral_constant<int, 42>, 15>(
        //         &data[0][0], std::integral_constant<int, 42>{});

        //     EXPECT_EQ(data[1][0], testee[1]);
        //     EXPECT_EQ(&data[1][0], &testee[1]);
        //     EXPECT_EQ(&data[2][0], &tuple_util::get<2>(testee));
        //     using std::get;
        //     EXPECT_EQ(&data[3][0], &get<3>(testee));

        //     testee = array<double, 15>{};
        // }
        // TEST(ptr_array, nested) {
        //     double data[15][42];

        //     auto inner = sid::dimension_to_array_impl_::ptr_array<double *, std::integral_constant<int, 42>, 15>(
        //         &data[0][0], std::integral_constant<int, 42>{});
        //     auto testee =
        //         *sid::dimension_to_array_impl_::ptr_array<decltype(inner), std::integral_constant<int, 1>, 42>(
        //             inner, std::integral_constant<int, 1>{});

        //     EXPECT_TRUE(&testee);

        //     // GT_META_PRINT_TYPE(decltype(testee[0]))

        //     EXPECT_EQ(data[1][0], testee[0][1]);
        //     EXPECT_EQ(&data[1][0], &testee[0][1]);

        //     testee[0] = array<double, 15>{};
        //     testee = array<array<double, 15>, 42>{};
        // }

        TEST(dimension_to_tuple_like, smoke) {
            double data[3][4] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
            auto testee = sid::dimension_to_tuple_like<gridtools::integral_constant<int, 0>, 3>(data);

            auto ptr = sid::get_origin(testee)();
            // GT_META_PRINT_TYPE(decltype(ptr));
            auto strides = sid::get_strides(testee);

            static_assert(tuple_util::size<decltype(strides)>{} == 1);
            // EXPECT_EQ(1l, (size_t)tuple_util::get<0>(strides));
            // static_assert(std::is_same_v<std::decay_t<decltype(tuple_util::get<0>(strides))>,
            //     gridtools::integral_constant<long, 1>>);

            EXPECT_EQ(data[1][0], tuple_util::get<1>(*ptr));

            // EXPECT_EQ(data[2][0], (*sid::shifted(ptr, tuple_util::get<0>(strides), 0))[2]);
            // EXPECT_EQ(data[2][0], (*ptr)[2]);
            // EXPECT_EQ(data[2][3], (*sid::shifted(ptr, tuple_util::get<0>(strides), 3))[2]);

            tuple_util::get<1>(*sid::shifted(ptr, tuple_util::get<0>(strides), 2)) = 42;
            EXPECT_EQ(42, data[1][2]);
        }

        TEST(dimension_to_tuple_like, assignable_from_tuple_like) {
            double data[2][5];
            auto testee = sid::dimension_to_tuple_like<gridtools::integral_constant<int, 0>, 2>(data);

            is_sid_separate<decltype(testee)>();

            auto ptr = sid::get_origin(testee)();

            *ptr = tuple(2., 3.);
            EXPECT_EQ(tuple_util::get<0>(*ptr), 2.);
            EXPECT_EQ(data[0][0], 2.);
            EXPECT_EQ(tuple_util::get<1>(*ptr), 3.);
            EXPECT_EQ(data[1][0], 3.);
        }
        // TEST(dimension_to_array, assignable_from_dim_to_array_sid) {
        //     double data_in[3][3] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        //     double data_out[3][3] = {};
        //     auto testee_in = sid::dimension_to_array<gridtools::integral_constant<int, 0>, 3>(data_in);
        //     auto testee_out = sid::dimension_to_array<gridtools::integral_constant<int, 1>, 3>(data_out);

        //     auto ptr_in = sid::get_origin(testee_in)();
        //     auto ptr_out = sid::get_origin(testee_out)();

        //     *ptr_out = *ptr_in;
        //     EXPECT_EQ(data_out[0][0], data_in[0][0]);
        //     EXPECT_EQ(data_out[0][1], data_in[1][0]);
        //     EXPECT_EQ(data_out[0][2], data_in[2][0]);
        // }

        TEST(dimension_to_tuple_like, nested) {
            double data[2][3];
            auto testee = sid::dimension_to_tuple_like<integral_constant<int, 1>, 3>(
                sid::dimension_to_tuple_like<integral_constant<int, 0>, 2>(data));

            static_assert(is_sid<decltype(testee)>::value);

            auto ptr = sid::get_origin(testee)();

            auto derefed = *ptr;
            auto outer = tuple_util::get<2>(derefed);
            // GT_META_PRINT_TYPE(decltype(outer));

            EXPECT_EQ(&data[1][2], &tuple_util::get<1>(tuple_util::get<2>(derefed)));
            derefed = array<array<double, 2>, 3>{};
        }

    } // namespace
} // namespace gridtools

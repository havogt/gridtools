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
#include "gridtools/sid/concept.hpp"
#include <gridtools/sid/rename_dimensions.hpp>

#include <gtest/gtest.h>
#include <sanitizer/asan_interface.h>

#include <gridtools/common/hymap.hpp>
#include <gridtools/common/integral_constant.hpp>
#include <gridtools/common/tuple_util.hpp>
#include <gridtools/sid/composite.hpp>
#include <gridtools/sid/simple_ptr_holder.hpp>
#include <gridtools/sid/synthetic.hpp>

#include <iostream>

namespace gridtools {
    namespace sid {
        struct mask_tag {};
        struct data_tag {};

        template <class Ptr>
        struct protected_ptr {
            Ptr m_ptr;
            double dummy_;

            double &operator*() {
                if (*at_key<mask_tag>(m_ptr))
                    return *at_key<data_tag>(m_ptr);
                else
                    return dummy_;
            }
            // auto /*TODO*/ operator*() const { return *at_key<data_tag>(m_ptr); }

            template <class Stride, class Offset>
            friend auto sid_shift(protected_ptr &ptr, Stride stride, Offset offset) {
                sid_shift(ptr.m_ptr, stride, offset);
            }
        };

        template <class Sid>
        struct ptr_holder {
            ptr_holder_type<Sid> ptr_holder_;
            protected_ptr<ptr_type<Sid>> operator()() { return {ptr_holder_()}; }
        };

        template <class Sid>
        struct mask_wrapper : delegate<Sid> {
            using delegate<Sid>::delegate;

            friend auto sid_get_origin(mask_wrapper &obj) { return ptr_holder<Sid>(sid::get_origin(obj.m_impl)); }
            // }
        };

        template <class MaskSid, class Sid>
        mask_wrapper<composite::keys<mask_tag, data_tag>::values<MaskSid, Sid>> mask(MaskSid &&mask_, Sid &&sid) {
            return {sid::composite::keys<mask_tag, data_tag>::make_values(
                std::forward<MaskSid>(mask_), std::forward<Sid>(sid))};
        };
    } // namespace sid
    namespace {
        using sid::property;
        using namespace literals;

        struct mask {};
        struct data {};

        TEST(simple, smoke) {
            double data_[3] = {40, 41, 42};
            auto data_sid = sid::synthetic()                                                           //
                                .set<property::origin>(sid::host_device::simple_ptr_holder(&data_[0])) //
                                .set<property::strides>(tuple(1_c));
            bool mask_[] = {true, true, true, false, false};
            auto mask_sid = sid::synthetic()                                                           //
                                .set<property::origin>(sid::host_device::simple_ptr_holder(&mask_[0])) //
                                .set<property::strides>(tuple(1_c));

            ASAN_POISON_MEMORY_REGION(data_ + 3, 2 * sizeof(double));

            auto testee = sid::mask(mask_sid, data_sid);
            // auto testee = sid::composite::keys<mask, data>::make_values(mask_sid, data_sid);
            // auto testee = sid::mask(_data, _mask, [](auto *ptr) {
            //     if (*at_key<mask>(ptr))
            //         return at_key<data>(ptr);
            //     else
            //         return noop{};
            // });
            // using testee_t = decltype(testee);

            auto ptr = sid::get_origin(testee)();
            // GT_META_PRINT_VALUE(ptr);
            auto strides = sid::get_strides(testee);

            EXPECT_EQ(40, *ptr);

            sid::shift(ptr, at_key<integral_constant<int, 0>>(strides), 2);
            EXPECT_EQ(42, *ptr);

            sid::shift(ptr, at_key<integral_constant<int, 0>>(strides), 2);
            // EXPECT_EQ(44, *ptr);
            *ptr = 5;
            EXPECT_EQ(5, *ptr);
        }

        // struct a {};
        // struct b {};
        // struct c {};
        // struct d {};

        // TEST(rename_dimensions, smoke) {
        //     double data[3][5][7];

        //     auto src = sid::synthetic()
        //                    .set<property::origin>(sid::simple_ptr_holder(&data[0][0][0]))
        //                    .set<property::strides>(hymap::keys<a, b, c>::make_values(5_c * 7_c, 7_c, 1_c))
        //                    .set<property::upper_bounds>(hymap::keys<a, b>::make_values(3, 5));

        //     auto testee = sid::rename_dimensions<b, d>(src);
        //     using testee_t = decltype(testee);

        //     auto strides = sid::get_strides(testee);
        //     EXPECT_EQ(35, sid::get_stride<a>(strides));
        //     EXPECT_EQ(0, sid::get_stride<b>(strides));
        //     EXPECT_EQ(1, sid::get_stride<c>(strides));
        //     EXPECT_EQ(7, sid::get_stride<d>(strides));

        //     static_assert(meta::is_empty<get_keys<sid::lower_bounds_type<testee_t>>>());
        //     auto u_bound = sid::get_upper_bounds(testee);
        //     EXPECT_EQ(3, at_key<a>(u_bound));
        //     EXPECT_EQ(5, at_key<d>(u_bound));
        // }

        // TEST(rename_dimensions, c_array) {
        //     double data[3][5][7];

        //     auto testee = sid::rename_dimensions<decltype(1_c), d>(data);
        //     using testee_t = decltype(testee);

        //     auto strides = sid::get_strides(testee);
        //     EXPECT_EQ(35, (sid::get_stride<integral_constant<int, 0>>(strides)));
        //     EXPECT_EQ(0, (sid::get_stride<integral_constant<int, 1>>(strides)));
        //     EXPECT_EQ(1, (sid::get_stride<integral_constant<int, 2>>(strides)));
        //     EXPECT_EQ(7, sid::get_stride<d>(strides));

        //     auto l_bound = sid::get_lower_bounds(testee);
        //     EXPECT_EQ(0, (at_key<integral_constant<int, 0>>(l_bound)));
        //     EXPECT_EQ(0, at_key<d>(l_bound));

        //     auto u_bound = sid::get_upper_bounds(testee);
        //     EXPECT_EQ(3, (at_key<integral_constant<int, 0>>(u_bound)));
        //     EXPECT_EQ(5, at_key<d>(u_bound));
        // }

        // TEST(rename_dimensions, rename_twice_and_make_composite) {
        //     double data[3][5][7];
        //     auto src = sid::synthetic()
        //                    .set<property::origin>(sid::host_device::simple_ptr_holder(&data[0][0][0]))
        //                    .set<property::strides>(hymap::keys<a, b, c>::make_values(5_c * 7_c, 7_c, 1_c))
        //                    .set<property::upper_bounds>(hymap::keys<a, b>::make_values(3, 5));
        //     auto testee = sid::rename_dimensions<a, c, b, d>(src);
        //     static_assert(sid::is_sid<decltype(testee)>::value);
        //     auto composite = sid::composite::keys<void>::make_values(testee);
        //     static_assert(sid::is_sid<decltype(composite)>::value);
        //     sid::get_origin(composite);
        // }

        // TEST(rename_dimensions, numbered) {
        //     double data[3][5][7];

        //     auto testee = sid::rename_numbered_dimensions<a, b, c>(data);
        //     using testee_t = decltype(testee);

        //     auto strides = sid::get_strides(testee);
        //     EXPECT_EQ(35, sid::get_stride<a>(strides));
        //     EXPECT_EQ(7, sid::get_stride<b>(strides));
        //     EXPECT_EQ(1, sid::get_stride<c>(strides));

        //     auto l_bound = sid::get_lower_bounds(testee);
        //     EXPECT_EQ(0, at_key<a>(l_bound));
        //     EXPECT_EQ(0, at_key<b>(l_bound));
        //     EXPECT_EQ(0, at_key<c>(l_bound));

        //     auto u_bound = sid::get_upper_bounds(testee);
        //     EXPECT_EQ(3, at_key<a>(u_bound));
        //     EXPECT_EQ(5, at_key<b>(u_bound));
        //     EXPECT_EQ(7, at_key<c>(u_bound));
        // }
    } // namespace
} // namespace gridtools

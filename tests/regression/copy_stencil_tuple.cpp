/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>

#include <gridtools/sid/dimension_to_array.hpp>
#include <gridtools/stencil/cartesian.hpp>

#include <stencil_select.hpp>
#include <test_environment.hpp>

namespace {
    using namespace gridtools;
    using namespace stencil;
    using namespace cartesian;

    struct copy_functor_tuple {
        using in = in_accessor<0>;
        using out = inout_accessor<1>;

        using param_list = make_param_list<in, out>;

        template <class Eval>
        GT_FUNCTION static void apply(Eval &&eval) {
            eval(out()) = eval(in());
        }
    };

    struct copy_functor_unrolled {
        using in0 = in_accessor<0>;
        using in1 = in_accessor<1>;
        using in2 = in_accessor<2>;
        using in3 = in_accessor<3>;
        using out0 = inout_accessor<4>;
        using out1 = inout_accessor<5>;
        using out2 = inout_accessor<6>;
        using out3 = inout_accessor<7>;

        using param_list = make_param_list<in0, in1, in2, in3, out0, out1, out2, out3>;

        template <class Eval>
        GT_FUNCTION static void apply(Eval &&eval) {
            eval(out0()) = eval(in0());
            eval(out1()) = eval(in1());
            eval(out2()) = eval(in2());
            eval(out3()) = eval(in3());
        }
    };

    GT_REGRESSION_TEST(copy_stencil_tuple_unrolled, test_environment<>, stencil_backend_t) {
        auto in = [](int i, int j, int k) { return i + j + k; };
        auto out0 = TypeParam::make_storage();
        auto out1 = TypeParam::make_storage();
        auto out2 = TypeParam::make_storage();
        auto out3 = TypeParam::make_storage();
        auto comp = [&out0,
                        &out1,
                        &out2,
                        &out3,
                        grid = TypeParam::make_grid(),
                        in0 = TypeParam::make_const_storage(in),
                        in1 = TypeParam::make_const_storage(in),
                        in2 = TypeParam::make_const_storage(in),
                        in3 = TypeParam::make_const_storage(in)] {
            run_single_stage(
                copy_functor_unrolled(), stencil_backend_t(), grid, in0, in1, in2, in3, out0, out1, out2, out3);
        };
        comp();
        TypeParam::verify(in, out0);
        TypeParam::verify(in, out1);
        TypeParam::verify(in, out2);
        TypeParam::verify(in, out3);
        TypeParam::benchmark("copy_stencil_tuple_unrolled", comp);
    }
    GT_REGRESSION_TEST(copy_stencil_tuple_tuple, test_environment<>, stencil_backend_t) {
        using float_t = typename TypeParam::float_t;
        auto in = [](int i, int j, int k) { return tuple{i + j + k, i + j + k + 1, i + j + k + 2, i + j + k + 3}; };
        auto out = TypeParam::template make_storage<tuple<float_t, float_t, float_t, float_t>>();
        auto comp = [&out,
                        grid = TypeParam::make_grid(),
                        in = TypeParam::template make_const_storage<tuple<float_t, float_t, float_t, float_t>>(in)] {
            run_single_stage(copy_functor_tuple(), stencil_backend_t(), grid, in, out);
        };
        comp();
        TypeParam::verify(in, out);
        TypeParam::benchmark("copy_stencil_tuple_tuple", comp);
    }

    GT_REGRESSION_TEST(copy_stencil_tuple_dim2array, test_environment<>, stencil_backend_t) {
        using float_t = typename TypeParam::float_t;
        auto in = [](int i, int j, int k, int t) { return i + j + k + t; };
        auto out_ds = TypeParam::template builder<float_t>(integral_constant<int, 4>{}).build();
        static_assert(is_sid<decltype(out_ds)>::value);
        auto out = sid::dimension_to_array<integral_constant<int, 3>, 4>(out_ds);
        static_assert(is_sid<decltype(out)>::value);
        auto in_ds = TypeParam::template builder<float_t const>(integral_constant<int, 4>{}).initializer(in).build();
        static_assert(is_sid<decltype(in_ds)>::value);
        auto in_transformed = sid::dimension_to_array<integral_constant<int, 3>, 4>(in_ds);
        static_assert(is_sid<decltype(in_transformed)>::value);
        auto comp = [&out, grid = TypeParam::make_grid(), &in = in_transformed] {
            run_single_stage(copy_functor_tuple(), stencil_backend_t(), grid, in, out);
        };
        comp();
        TypeParam::verify(in, out_ds);
        TypeParam::benchmark("copy_stencil_tuple_dim2array", comp);
    }
} // namespace

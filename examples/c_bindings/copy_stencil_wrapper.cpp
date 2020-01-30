/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <array>
#include <bits/c++config.h>
#include <cpp_bindgen/export.hpp>
#include <gridtools/interface/fortran_array_adapter.hpp>
#include <gridtools/stencil_composition/stencil_composition.hpp>

namespace gt = gridtools;

// In this example, we demonstrate how the cpp_bindgen library can be used to export functions to C and Fortran. We are
// going to export the functions required to run a simple copy stencil (see also the commented example in
// examples/stencil_composition/copy_stencil.cpp)

namespace custom_array {
    template <class T>
    struct my_array {
        using data_t = T;

        T *data;
        std::array<int, 3> sizes;
        std::array<int, 3> strides;

        const T &operator()(int i, int j, int k) const {
            assert(i < sizes[0] && j < sizes[1] && k < sizes[2] && "out of bounds");
            return data[i * strides[0] + j * strides[1] + k * strides[2]];
        }

        T &operator()(int i, int j, int k) {
            assert(i < sizes[0] && j < sizes[1] && k < sizes[2] && "out of bounds");
            return data[i * strides[0] + j * strides[1] + k * strides[2]];
        }
    };

    template <typename T>
    my_array<T> bindgen_make_fortran_array_view(bindgen_fortran_array_descriptor *descriptor, my_array<T> *) {
        if (descriptor->rank != 3) {
            throw std::runtime_error("only 3-dimensional arrays are supported");
        }
        return my_array<T>{static_cast<T *>(descriptor->data),
            {descriptor->dims[0], descriptor->dims[1], descriptor->dims[2]},
            {1, descriptor->dims[0], descriptor->dims[0] * descriptor->dims[1]}};
    }

    template <typename T>
    bindgen_fortran_array_descriptor get_fortran_view_meta(my_array<T> *) {
        bindgen_fortran_array_descriptor descriptor;
        descriptor.type = cpp_bindgen::fortran_array_element_kind<T>::value;
        descriptor.rank = 3;
        descriptor.is_acc_present = false;
        return descriptor;
    }

    static_assert(cpp_bindgen::is_fortran_array_bindable<my_array<float>>::value, "");
    static_assert(cpp_bindgen::is_fortran_array_wrappable<my_array<float>>::value, "");
} // namespace custom_array

namespace {

    using axis_t = gt::axis<1>::axis_interval_t;
    using grid_t = gt::grid<axis_t>;

#ifdef __CUDACC__
    using backend_t = gt::backend::cuda;
#else
    using backend_t = gt::backend::mc;
#endif

    using storage_traits_t = gt::storage_traits<backend_t>;
    using storage_info_t = storage_traits_t::storage_info_t<0, 3>;
    using data_store_t = storage_traits_t::data_store_t<float, storage_info_t>;

    struct copy_functor {
        using in = gt::in_accessor<0>;
        using out = gt::inout_accessor<1>;
        using param_list = gt::make_param_list<in, out>;

        template <typename Evaluation>
        GT_FUNCTION static void apply(Evaluation &eval) {
            eval(out{}) = eval(in{});
        }
    };

    using p_in = gt::arg<0, data_store_t>;
    using p_out = gt::arg<1, data_store_t>;

    // The following are wrapper functions which will be exported to C/Fortran
    grid_t make_grid_impl(int nx, int ny, int nz) { return {gt::make_grid(nx, ny, nz)}; }
    storage_info_t make_storage_info_impl(int nx, int ny, int nz) { return {nx, ny, nz}; }
    data_store_t make_data_store_impl(storage_info_t storage_info) { return {storage_info}; }

    gt::computation<p_in, p_out> make_copy_stencil_impl(const grid_t &grid) {
        return gt::make_computation<backend_t>(
            grid, gt::make_multistage(gt::execute::parallel(), gt::make_stage<copy_functor>(p_in{}, p_out{})));
    }

    // Note that fortran_array_adapters are "fortran array wrappable".
    static_assert(gt::c_bindings::is_fortran_array_wrappable<gt::fortran_array_adapter<data_store_t>>::value, "");

    void transform_f_to_c_impl(data_store_t data_store, gt::fortran_array_adapter<data_store_t> descriptor) {
        transform(data_store, descriptor);
    }
    void transform_c_to_f_impl(gt::fortran_array_adapter<data_store_t> descriptor, data_store_t data_store) {
        transform(descriptor, data_store);
    }

    void simple_copy_to_gt_impl(data_store_t data_store, custom_array::my_array<float> fortran_array) {
        auto view = gt::make_host_view(data_store);
        for (int k = 0; k < fortran_array.sizes[2]; ++k) {
            for (int j = 0; j < fortran_array.sizes[1]; ++j) {
                for (int i = 0; i < fortran_array.sizes[0]; ++i) {
                    view(i, j, k) = fortran_array(i, j, k);
                }
            }
        }
        data_store.sync();
    }

    void simple_copy_from_gt_impl(custom_array::my_array<float> fortran_array, data_store_t data_store) {
        data_store.sync();
        auto view = gt::make_host_view(data_store);
        for (int k = 0; k < fortran_array.sizes[2]; ++k) {
            for (int j = 0; j < fortran_array.sizes[1]; ++j) {
                for (int i = 0; i < fortran_array.sizes[0]; ++i) {
                    fortran_array(i, j, k) = view(i, j, k);
                }
            }
        }
    }

    // That means that in the generated Fortran code, a wrapper is created that takes a Fortran array, and converts
    // it into the fortran array wrappable type.
    void run_stencil_impl(gt::computation<p_in, p_out> &computation, data_store_t in, data_store_t out) {

        computation.run(p_in() = in, p_out() = out);

#ifdef __CUDACC__
        cudaDeviceSynchronize();
#endif
    }

    // exports `make_grid_impl` (which needs 3 arguments) under the name `make_grid`
    BINDGEN_EXPORT_BINDING_3(make_grid, make_grid_impl);
    BINDGEN_EXPORT_BINDING_3(make_storage_info, make_storage_info_impl);
    BINDGEN_EXPORT_BINDING_1(make_data_store, make_data_store_impl);
    BINDGEN_EXPORT_BINDING_1(make_copy_stencil, make_copy_stencil_impl);
    BINDGEN_EXPORT_BINDING_3(run_stencil, run_stencil_impl);

    // In order to generate the additional wrapper for Fortran array,
    // the *_WRAPPED_* versions need to be used
    BINDGEN_EXPORT_BINDING_WRAPPED_2(transform_c_to_f, transform_c_to_f_impl);
    BINDGEN_EXPORT_BINDING_WRAPPED_2(transform_f_to_c, transform_f_to_c_impl);

    BINDGEN_EXPORT_BINDING_WRAPPED_2(simple_copy_to_gt, simple_copy_to_gt_impl);
    BINDGEN_EXPORT_BINDING_WRAPPED_2(simple_copy_from_gt, simple_copy_from_gt_impl);
} // namespace

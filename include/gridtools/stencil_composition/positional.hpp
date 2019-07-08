/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "../common/defs.hpp"
#include "../common/host_device.hpp"
#include "../common/hymap.hpp"

namespace gridtools {

    // Represents position in the computation space.
    // Models SID concept
    template <class Dim>
    struct positional {
        int_t m_val;

        struct stride {};

        friend GT_FUNCTION positional operator+(positional lhs, positional rhs) { return {lhs.m_val + rhs.m_val}; }

        friend typename hymap::keys<Dim>::template values<stride> sid_get_strides(positional) { return {}; }

        GT_FUNCTION positional(int_t val = 0) : m_val{val} {}

        GT_FUNCTION int operator*() const { return m_val; }
        GT_FUNCTION positional const &operator()() const { return *this; }
    };

    template <class Dim>
    GT_FUNCTION void sid_shift(positional<Dim> &p, typename positional<Dim>::stride, int_t offset) {
        p.m_val += offset;
    }

    template <class Dim>
    positional<Dim> sid_get_ptr_diff(positional<Dim>);

    template <class Dim>
    positional<Dim> sid_get_origin(positional<Dim> obj) {
        return obj;
    }
} // namespace gridtools
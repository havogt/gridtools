/*
 * GridTools
 *
 * Copyright (c) 2014-2021, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <utility>

#include "../../../common/tuple_util.hpp"
#include "../../../meta.hpp"
#include "../../../sid/composite.hpp"
#include "../../cartesian.hpp"
#include "../../strided_iter.hpp"
#include "apply.hpp"
#include "tag.hpp"

namespace gridtools::fn {
    namespace cartesian_naive_impl_ {
        struct iterator_maker {
            GT_FUNCTION consteval auto operator()() const {
                return [](auto tag, auto &&ptr, auto const &strides) { return strided_iter(tag, ptr, strides); };
            }
        };
    } // namespace cartesian_naive_impl_

    template <class Stencil, class Sizes, class Offsets, class Output, class Inputs>
    void fn_apply(naive, cartesian<Sizes, Offsets> const &domain, Stencil, Output &output, Inputs inputs) {
        naive_apply<Stencil, cartesian_naive_impl_::iterator_maker>(domain.sizes,
            domain.offsets,
            output,
            std::move(inputs),
            []<class OutTag, class InTags, class Out, class Ins>(OutTag, InTags, Out &out, Ins &&ins) {
                using keys_t = meta::rename<sid::composite::keys, meta::push_back<InTags, OutTag>>;
                return tuple_util::convert_to<keys_t::template values>(
                    tuple_util::push_back(std::forward<Ins>(ins), out));
            });
    }
} // namespace gridtools::fn

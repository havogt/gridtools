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

#include <tuple>
#include <type_traits>
#include <utility>

#include "gridtools/common/hymap.hpp"
#include "gridtools/common/tuple_util.hpp"
#include "gridtools/meta.hpp"
#include "gridtools/sid/concept.hpp"
#include "gridtools/sid/loop.hpp"
#include "gridtools/sid/multi_shift.hpp"

namespace gridtools::fn {
    namespace naive_apply_impl_ {

        template <class Stencil, class MakeIterator, class Out, class... Ins>
        struct stencil_stage {
            GT_FUNCTION void operator()(auto const &ptr, auto const &strides) const {
                *at_key<Out>(ptr) = Stencil{}()(MakeIterator{}()(Ins{}, ptr, strides)...);
            }
        };

        template <class Stencil, class MakeIterator, class Out, class Ins>
        struct make_stencil_stage;
        template <class Stencil, class MakeIterator, class Out, template <class...> class Ins, class... InsElems>
        struct make_stencil_stage<Stencil, MakeIterator, Out, Ins<InsElems...>> {
            using type = stencil_stage<Stencil, MakeIterator, Out, InsElems...>;
        };
        template <class Stencil, class MakeIterator, class Out, class Ins>
        using make_stencil_stage_t = typename make_stencil_stage<Stencil, MakeIterator, Out, Ins>::type;

        struct out_tag {};

        template <class T>
        struct in_tag : T {};

        template <class Sizes>
        auto make_loops(Sizes const &sizes) {
            return tuple_util::fold(
                [&]<class Outer, class Dim>(Outer outer, Dim) {
                    return [ outer = std::move(outer), inner = sid::make_loop<Dim>(at_key<Dim>(sizes)) ]<class... Args>(
                        Args && ... args) {
                        return outer(inner(std::forward<Args>(args)...));
                    };
                },
                std::identity(),
                meta::rename<std::tuple, get_keys<Sizes>>());
        }

        template <class Stencil,
            class MakeIterator,
            class Sizes,
            class Offsets,
            class Output,
            class Inputs,
            class MakeComposite>
        void naive_apply(Sizes const &sizes,
            Offsets const &offsets,
            Output &output,
            Inputs &&inputs,
            MakeComposite &&make_composite) {
            using in_tags_t = meta::transform<in_tag, meta::make_indices<tuple_util::size<Inputs>, std::tuple>>;
            auto composite = std::forward<MakeComposite>(make_composite)(out_tag(), in_tags_t(), output, inputs);
            auto strides = sid::get_strides(composite);
            auto ptr = sid::get_origin(composite)();
            sid::multi_shift(ptr, strides, offsets);
            make_loops(sizes)([&](auto const &ptr, auto const &strides) {
                make_stencil_stage_t<Stencil, MakeIterator, out_tag, in_tags_t>{}(ptr, strides);
            })(ptr, strides);
        }
    } // namespace naive_apply_impl_
    using naive_apply_impl_::naive_apply;
} // namespace gridtools::fn

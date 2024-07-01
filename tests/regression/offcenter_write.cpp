#include <gridtools/stencil/cartesian.hpp>

#include <stencil_select.hpp>
#include <test_environment.hpp>
#include <cstdint>
#include <gridtools/common/defs.hpp>
#include <gridtools/stencil/cartesian.hpp>
#include <gridtools/common/array.hpp>
#include <gridtools/stencil/global_parameter.hpp>
#include <gridtools/stencil/positional.hpp>

namespace column_physics_conditional_impl_ {
using Domain = std::array<gridtools::uint_t, 3>;
using namespace gridtools::stencil;
using namespace gridtools::stencil::cartesian;
using gridtools::array;

struct HorizontalExecution140733197435520 {
  using A = inout_accessor<0, extent<0, 0, 0, 0, -1000, 1000>, 3>;
  using B = inout_accessor<1, extent<0, 0, 0, 0, 0, 1>, 3>;
  using scalar = in_accessor<2, extent<0, 0, 0, 0, 0, 0>, 3>;

  using param_list = make_param_list<A, B, scalar>;

  template <typename Evaluation>
  GT_FUNCTION static void
  apply(Evaluation eval, gridtools::stencil::core::interval<
                             gridtools::stencil::core::level<0, 2, 3>,
                             gridtools::stencil::core::level<1, -1, 3>>) {
    bool mask_140733230918592_gen_0;
    std::int64_t lev_gen_0;
    mask_140733230918592_gen_0 =
        ((eval(A()) > static_cast<double>(static_cast<std::int64_t>(0))) and
         (eval(B()) > static_cast<double>(static_cast<std::int64_t>(0))));
    if (mask_140733230918592_gen_0) {
      eval(A(0, 0, -1)) = eval(scalar());
      eval(B(0, 0, 1)) = eval(A());
    }

    lev_gen_0 = static_cast<std::int64_t>(1);
    while (((eval(A()) >= static_cast<double>(static_cast<std::int64_t>(0))) and
            (eval(B()) >= static_cast<double>(static_cast<std::int64_t>(0))))) {
      eval(A(0, 0, lev_gen_0)) =
          static_cast<double>((-static_cast<std::int64_t>(1)));
      eval(B()) = static_cast<double>((-static_cast<std::int64_t>(1)));
      lev_gen_0 = (lev_gen_0 + static_cast<std::int64_t>(1));
    }
  }
};

auto column_physics_conditional(Domain domain) {
  return [domain](auto &&A, auto &&B, auto &&scalar) {
    {
      auto grid = make_grid(domain[0], domain[1],
                            axis<1, axis_config::offset_limit<3>>{domain[2]});

      auto GTComputationCall140733197491056 = [](auto A, auto B, auto scalar) {
        return multi_pass(
            execute_backward()
                .k_cached(cache_io_policy::fill(), cache_io_policy::flush(), B)
                .stage(HorizontalExecution140733197435520(), A, B, scalar));
      };

      run(GTComputationCall140733197491056, stencil_backend_t(), grid,
          std::forward<decltype(A)>(A), std::forward<decltype(B)>(B),
          std::forward<decltype(scalar)>(scalar));
    }
  };
}
} // namespace column_physics_conditional_impl_

auto column_physics_conditional(
    column_physics_conditional_impl_::Domain domain) {
  return column_physics_conditional_impl_::column_physics_conditional(domain);
}



namespace{
        using namespace gridtools;
    using namespace stencil;
    using namespace cartesian;
    GT_REGRESSION_TEST(offcenter, test_environment<1>, stencil_backend_t) {
        auto builder = storage::builder<storage_traits_t>            //
                        .dimensions(1,1,4) //
                     .template type<double>();
        auto in = [](int_t, int_t, int_t k) { return 40.+k; };
        auto a = builder.initializer(in).build();
        auto b = builder.value(1.).build();
        
        double scalar = 2.0;
        
        std::array<gridtools::uint_t, 3> domain = {1,1,3};
        std::array<gridtools::uint_t, 3> A_origin = {0,0,0};
        std::array<gridtools::uint_t, 3> B_origin = {0,0,0};

                column_physics_conditional(domain)(
            sid::shift_sid_origin(
                a,
                A_origin),
            sid::shift_sid_origin(
                b,
                B_origin),
            stencil::global_parameter(scalar));

        //run_single_stage(lap(), stencil_backend_t(), TypeParam::make_grid(), out, TypeParam::make_storage(in));
        auto ref = [](int_t, int_t, int_t k){
            if(k<2)return 2;
            else return -1;
        };
        auto v = a->host_view();
        std::cout << v(0,0,0) << "/" << v(0,0,1) << "/" << v(0,0,2) << "/" << v(0,0,3) << "\n";

        //TypeParam::verify(ref, a);
    }
}

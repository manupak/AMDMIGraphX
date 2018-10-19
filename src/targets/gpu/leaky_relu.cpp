#include <migraph/gpu/leaky_relu.hpp>
#include <migraph/operators.hpp>
#include <migraph/manage_ptr.hpp>
#include <migraph/gpu/miopen.hpp>
#include <utility>

namespace migraph {
namespace gpu {

shape miopen_leaky_relu::compute_shape(const std::vector<shape>& inputs) const
{
    check_shapes{inputs, *this}.has(2).not_broadcasted();
    return inputs.at(1);
}

argument miopen_leaky_relu::compute(context& ctx,
                              const shape& output_shape,
                              const std::vector<argument>& args) const
{
    float alpha = 1, beta = 0;
    auto x_desc = make_tensor(args[0].get_shape());
    auto y_desc = make_tensor(output_shape);
    miopenActivationForward(ctx.handle.get(),
                            ad.get(),
                            &alpha,
                            x_desc.get(),
                            args[0].implicit(),
                            &beta,
                            y_desc.get(),
                            args[1].implicit());

    return args[1];
}

} // namespace gpu

} // namespace migraph

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <migraphx/adjust_allocation.hpp>
#include <migraphx/auto_contiguous.hpp>
#include <migraphx/check_context.hpp>
#include <migraphx/dead_code_elimination.hpp>
#include <migraphx/eliminate_allocation.hpp>
#include <migraphx/eliminate_common_subexpression.hpp>
#include <migraphx/eliminate_concat.hpp>
#include <migraphx/eliminate_contiguous.hpp>
#include <migraphx/eliminate_data_type.hpp>
#include <migraphx/eliminate_identity.hpp>
#include <migraphx/eliminate_pad.hpp>
#include <migraphx/fuse_pointwise.hpp>
#include <migraphx/inline_module.hpp>
#include <migraphx/insert_pad.hpp>
#include <migraphx/layout_nhwc.hpp>
#include <migraphx/memory_coloring.hpp>
#include <migraphx/normalize_ops.hpp>
#include <migraphx/optimize_module.hpp>
#include <migraphx/preallocate_param.hpp>
#include <migraphx/propagate_constant.hpp>
#include <migraphx/register_target.hpp>
#include <migraphx/replace_allocate.hpp>
#include <migraphx/rewrite_gelu.hpp>
#include <migraphx/rewrite_pooling.hpp>
#include <migraphx/rewrite_quantization.hpp>
#include <migraphx/rewrite_rnn.hpp>
#include <migraphx/schedule.hpp>
#include <migraphx/simplify_algebra.hpp>
#include <migraphx/simplify_qdq.hpp>
#include <migraphx/simplify_reshapes.hpp>
#include <migraphx/gpu/allocation_model.hpp>
#include <migraphx/gpu/compile_miopen.hpp>
#include <migraphx/gpu/compile_ops.hpp>
#include <migraphx/gpu/concat_gpu_opt.hpp>
#include <migraphx/gpu/context.hpp>
#include <migraphx/gpu/device_name.hpp>
#include <migraphx/gpu/fuse_mlir.hpp>
#include <migraphx/gpu/fuse_ops.hpp>
#include <migraphx/gpu/prefuse_ops.hpp>
#include <migraphx/gpu/lowering.hpp>
#include <migraphx/gpu/pack_int8_args.hpp>
#include <migraphx/gpu/schedule_model.hpp>
#include <migraphx/gpu/sync_device.hpp>
#include <migraphx/gpu/target.hpp>
#include <migraphx/gpu/write_literals.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

MIGRAPHX_DECLARE_ENV_VAR(MIGRAPHX_DISABLE_SCHEDULE_PASS)
MIGRAPHX_DECLARE_ENV_VAR(MIGRAPHX_DISABLE_POINTWISE_FUSION)
MIGRAPHX_DECLARE_ENV_VAR(MIGRAPHX_ENABLE_NHWC)
struct id_pass
{
    std::string name() const { return "id"; }
    void apple(const module&) const {}
};

pass enable_pass(bool enabled, pass p)
{
    if(enabled)
        return p;
    return id_pass{};
}

std::vector<pass> target::get_passes(migraphx::context& gctx, const compile_options& options) const
{
    auto& ctx = any_cast<context>(gctx);
    ctx.set_exhaustive_tune_flag(options.exhaustive_tune);
    std::set<shape::type_t> unsupported_types(shape::types().begin(), shape::types().end());
    unsupported_types.erase(shape::type_t::float_type);
    unsupported_types.erase(shape::type_t::half_type);
    unsupported_types.erase(shape::type_t::bool_type);
    unsupported_types.erase(shape::type_t::int8_type);
    unsupported_types.erase(shape::type_t::uint8_type);
    unsupported_types.erase(shape::type_t::tuple_type);
    // clang-format off
    return
    {
        normalize_ops{},
        dead_code_elimination{},
        simplify_qdq{},
        rewrite_quantization{},
        dead_code_elimination{},
        eliminate_data_type{unsupported_types, shape::type_t::float_type},
        simplify_reshapes{},
        eliminate_identity{},
        eliminate_pad{},
        dead_code_elimination{},
        insert_pad{},
        dead_code_elimination{},
        rewrite_rnn{},
        dead_code_elimination{},
        inline_module{},
        rewrite_pooling{},
        dead_code_elimination{},
        rewrite_gelu{},
        optimize_module{},
        enable_pass(enabled(MIGRAPHX_ENABLE_NHWC{}), layout_nhwc{}),
        dead_code_elimination{},
        prefuse_ops{},
        dead_code_elimination{},
        auto_contiguous{},
        optimize_module{},
        enable_pass(not enabled(MIGRAPHX_DISABLE_POINTWISE_FUSION{}), fuse_pointwise{}),
        dead_code_elimination{},
        fuse_mlir{&ctx},
        dead_code_elimination{},
        lowering{&ctx, options.offload_copy},
        eliminate_contiguous{"gpu::contiguous"},
        dead_code_elimination{},
        eliminate_concat{concat_gpu_optimization{}},
        dead_code_elimination{},
        compile_miopen{&gctx},
        dead_code_elimination{},
        pack_int8_args{},
        dead_code_elimination{},
        fuse_ops{&ctx, options.fast_math},
        dead_code_elimination{},
        replace_allocate{gpu_allocation_model{}, options.offload_copy},
        dead_code_elimination{},
        adjust_allocation{gpu_allocation_model{}},
        dead_code_elimination{},
        compile_ops{&ctx},
        dead_code_elimination{},
        write_literals{&ctx},
        schedule{gpu::schedule_model{ctx.get_current_device().nstreams()}, not enabled(MIGRAPHX_DISABLE_SCHEDULE_PASS{})},
        memory_coloring{"hip::allocate"},
        sync_device{},
        preallocate_param{"scratch", gpu_allocation_model{}},
        dead_code_elimination{},
        eliminate_allocation{"hip::allocate"},
        check_context<context>{},
        normalize_ops{},
        dead_code_elimination{},
        eliminate_identity{}
    };
    // clang-format on
}

std::string target::name() const { return "gpu"; }

migraphx::context target::get_context() const { return context(gpu::get_device_id()); }

argument target::copy_to(const argument& arg) const { return gpu::to_gpu(arg); }

argument target::copy_from(const argument& arg) const { return gpu::from_gpu(arg); }

argument target::allocate(const shape& s) const { return gpu::allocate_gpu(s); }

MIGRAPHX_REGISTER_TARGET(target);

} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

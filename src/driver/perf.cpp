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
#include "perf.hpp"

#include <migraphx/generate.hpp>
#include <migraphx/register_target.hpp>
#ifdef HAVE_GPU
#include <migraphx/gpu/hip.hpp>
#endif

namespace migraphx {
namespace driver {
inline namespace MIGRAPHX_INLINE_NS {

template <class T>
auto get_hash(const T& x)
{
    return std::hash<T>{}(x);
}

parameter_map fill_param_map(parameter_map& m, const program& p, const target& t, bool offload)
{
    for(auto&& x : p.get_parameter_shapes())
    {
        argument& arg = m[x.first];
        if(arg.empty())
            arg = generate_argument(x.second, get_hash(x.first));
        if(not offload)
            arg = t.copy_to(arg);
    }
    return m;
}

parameter_map fill_param_map(parameter_map& m, const program& p, bool gpu)
{
    for(auto&& x : p.get_parameter_shapes())
    {
        argument& arg = m[x.first];
        if(arg.empty())
            arg = generate_argument(x.second, get_hash(x.first));
#ifdef HAVE_GPU
        if(gpu)
            arg = gpu::to_gpu(arg);
#else
        (void)gpu;
#endif
    }
    return m;
}

parameter_map create_param_map(const program& p, const target& t, bool offload)
{
    parameter_map m;
    for(auto&& x : p.get_parameter_shapes())
    {
        auto arg = generate_argument(x.second, get_hash(x.first));
        if(offload)
            m[x.first] = arg;
        else
            m[x.first] = t.copy_to(arg);
    }
    return m;
}

parameter_map create_param_map(const program& p, bool gpu)
{
    parameter_map m;
    for(auto&& x : p.get_parameter_shapes())
    {
#ifdef HAVE_GPU
        if(gpu)
            m[x.first] = gpu::to_gpu(generate_argument(x.second, get_hash(x.first)));
        else
#else
        (void)gpu;
#endif
            m[x.first] = generate_argument(x.second, get_hash(x.first));
    }
    return m;
}

target get_target(bool gpu)
{
    if(gpu)
        return make_target("gpu");
    else
        return make_target("cpu");
}

} // namespace  MIGRAPHX_INLINE_NS
} // namespace driver
} // namespace migraphx

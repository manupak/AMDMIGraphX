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
#ifndef MIGRAPHX_GUARD_OPERATORS_FLATTEN_HPP
#define MIGRAPHX_GUARD_OPERATORS_FLATTEN_HPP

#include <migraphx/check_shapes.hpp>
#include <migraphx/argument.hpp>
#include <migraphx/config.hpp>
#include <migraphx/value.hpp>
#include <migraphx/op/normalize_attribute.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace op {

struct flatten
{
    int64_t axis = 1;

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.axis, "axis"));
    }

    value attributes() const
    {
        value normalize;
        normalize["axis"] =
            value::array{normalize_attribute::include_min, normalize_attribute::include_max};
        return {{"normalize_axes", normalize}};
    }

    std::string name() const { return "flatten"; }
    shape normalize_compute_shape(std::vector<shape> inputs) const
    {
        check_shapes{inputs, *this, true}.has(1);
        auto s = inputs[0];
        if(s.dynamic())
        {
            auto min_lens = s.min_lens();
            auto max_lens = s.max_lens();
            auto opt_lens = s.opt_lens();
            // If any of the opt values is 0, output opt will be 0
            shape::dynamic_dimension x = {
                std::accumulate(
                    min_lens.begin(), min_lens.begin() + axis, std::size_t{1}, std::multiplies<>{}),
                std::accumulate(
                    max_lens.begin(), max_lens.begin() + axis, std::size_t{1}, std::multiplies<>{}),
                std::accumulate(opt_lens.begin(),
                                opt_lens.begin() + axis,
                                std::size_t{1},
                                std::multiplies<>{})};
            shape::dynamic_dimension y = {
                std::accumulate(
                    min_lens.begin() + axis, min_lens.end(), std::size_t{1}, std::multiplies<>{}),
                std::accumulate(
                    max_lens.begin() + axis, max_lens.end(), std::size_t{1}, std::multiplies<>{}),
                std::accumulate(
                    opt_lens.begin() + axis, opt_lens.end(), std::size_t{1}, std::multiplies<>{}),
            };
            return {s.type(), {x, y}};
        }
        else
        {
            check_shapes{inputs, *this}.standard();
            auto&& lens = s.lens();
            auto x      = std::accumulate(
                lens.begin(), lens.begin() + axis, std::size_t{1}, std::multiplies<>{});
            auto y = std::accumulate(
                lens.begin() + axis, lens.end(), std::size_t{1}, std::multiplies<>{});
            return {s.type(), {x, y}};
        }
    }
    argument compute(const dyn_output& dyn_out, std::vector<argument> args) const
    {
        return args[0].reshape(dyn_out.computed_shape);
    }
    std::ptrdiff_t output_alias(const std::vector<shape>&) const { return 0; }
};

} // namespace op
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif

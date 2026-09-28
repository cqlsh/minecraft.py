/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026-present cqlsh
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <limits>

#include <minecraft/lang/math.hpp>

namespace minecraft::util {

/// Utils for casting number types to other number types.
///
/// This class only consists of static functions and cannot be instantiated.
///
/// Every comparison that may see NaN is written with ``>``, MSVC evaluates
/// ``NaN < x`` to true in constant expressions.
class NumberConversions final {
public:
    NumberConversions() = delete;

    /// Rounds a number down to the nearest integer.
    ///
    /// NaN becomes 0, numbers outside of the 64 bit range saturate.
    [[nodiscard]] static constexpr std::int64_t floor(double num) noexcept {
        if (!is_castable(num)) {
            return saturate(num);
        }

        const std::int64_t truncated = static_cast<std::int64_t>(num);
        return static_cast<double>(truncated) == num ? truncated : truncated - static_cast<std::int64_t>(0.0 > num);
    }

    /// Rounds a number up to the nearest integer.
    ///
    /// NaN becomes 0, numbers outside of the 64 bit range saturate.
    [[nodiscard]] static constexpr std::int64_t ceil(double num) noexcept {
        if (!is_castable(num)) {
            return saturate(num);
        }

        const std::int64_t truncated = static_cast<std::int64_t>(num);
        return static_cast<double>(truncated) == num ? truncated : truncated + static_cast<std::int64_t>(num > 0.0);
    }

    /// Rounds a number to the nearest integer, with ties rounding up.
    ///
    /// NaN becomes 0, numbers outside of the 64 bit range saturate.
    [[nodiscard]] static constexpr std::int64_t round(double num) noexcept {
        return floor(num + 0.5);
    }

    /// Multiplies a number with itself.
    [[nodiscard]] static constexpr double square(double num) noexcept {
        return num * num;
    }

    /// Checks whether a number is neither infinite nor NaN.
    [[nodiscard]] static constexpr bool is_finite(double d) noexcept {
        return (std::numeric_limits<double>::max)() >= lang::Math::abs(d);
    }

private:
    static constexpr double LIMIT = 9223372036854775808.0;

    [[nodiscard]] static constexpr bool is_castable(double num) noexcept {
        return LIMIT > lang::Math::abs(num);
    }

    [[nodiscard]] static constexpr std::int64_t saturate(double num) noexcept {
        if (num != num) {
            return 0;
        }

        return 0.0 > num ? (std::numeric_limits<std::int64_t>::min)() : (std::numeric_limits<std::int64_t>::max)();
    }
};

}
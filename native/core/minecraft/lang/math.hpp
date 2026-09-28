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

#include <bit>
#include <cstdint>

namespace minecraft::lang {

/// The functions of java.lang.Math that the API relies on, with the semantics of Java.
///
/// This class only consists of static functions and cannot be instantiated.
///
/// Every comparison that may see NaN is written with ``>`` or ``>=``, MSVC
/// evaluates ``NaN < x`` and ``NaN <= x`` to true in constant expressions.
///
/// The names min and max are written in parentheses, which keeps the macros
/// of the same name that windows.h defines from replacing them.
class Math final {
public:
    Math() = delete;

    /// Returns the absolute value of a number.
    ///
    /// The absolute value of -0.0 is 0.0, the one of NaN is NaN.
    [[nodiscard]] static constexpr double abs(double a) noexcept {
        return 0.0 >= a ? 0.0 - a : a;
    }

    /// Returns the smaller of two numbers.
    ///
    /// NaN is returned if either number is NaN, -0.0 is smaller than 0.0.
    [[nodiscard]] static constexpr double (min)(double a, double b) noexcept {
        if (a != a) {
            return a;
        }
        if (a == 0.0 && b == 0.0) {
            return is_negative(a) ? a : b;
        }

        return b > a ? a : b;
    }

    /// Returns the greater of two numbers.
    ///
    /// NaN is returned if either number is NaN, 0.0 is greater than -0.0.
    [[nodiscard]] static constexpr double (max)(double a, double b) noexcept {
        if (a != a) {
            return a;
        }
        if (a == 0.0 && b == 0.0) {
            return is_negative(a) ? b : a;
        }

        return a > b ? a : b;
    }

private:
    [[nodiscard]] static constexpr bool is_negative(double a) noexcept {
        return (std::bit_cast<std::uint64_t>(a) >> 63) != 0;
    }
};

}
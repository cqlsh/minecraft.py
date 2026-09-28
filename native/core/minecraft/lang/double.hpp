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

/// The functions of java.lang.Double that the API relies on, with the semantics of Java.
///
/// This class only consists of static functions and cannot be instantiated.
class Double final {
public:
    Double() = delete;

    /// Returns the bits that represent a number, every NaN is represented by the same bits.
    [[nodiscard]] static constexpr std::uint64_t to_long_bits(double value) noexcept {
        return value != value ? 0x7ff8000000000000ULL : std::bit_cast<std::uint64_t>(value);
    }

    /// Returns the hash code of a number, identical to the one of Java.
    [[nodiscard]] static constexpr std::uint32_t hash_code(double value) noexcept {
        const std::uint64_t bits = to_long_bits(value);
        return static_cast<std::uint32_t>(bits ^ (bits >> 32));
    }
};

}
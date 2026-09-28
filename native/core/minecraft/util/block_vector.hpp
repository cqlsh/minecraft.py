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
#include <minecraft/util/vector.hpp>

namespace minecraft::util {

/// A vector with a hash function that truncates the X, Y, Z components, a la
/// BlockVector in WorldEdit.
///
/// BlockVectors can be used in hash sets and hash maps. Be aware that
/// BlockVectors are mutable, but it is important that BlockVectors are never
/// changed once put into a hash set or hash map.
///
/// equals() and hash_code() hide the ones of Vector and are selected by the
/// static type, there is no virtual dispatch.
class BlockVector : public Vector {
public:
    /// Constructs the vector with all components as 0.
    constexpr BlockVector() noexcept = default;

    /// Constructs the vector with another vector.
    constexpr explicit BlockVector(const Vector& vec) noexcept : Vector(vec) {
    }

    /// Constructs the vector with the provided components.
    constexpr BlockVector(double x, double y, double z) noexcept : Vector(x, y, z) {
    }

    /// Checks if another block vector is equivalent.
    ///
    /// Two block vectors are equivalent if their components are equal after
    /// they have been truncated towards zero.
    [[nodiscard]] constexpr bool equals(const BlockVector& other) const noexcept {
        return truncate(other.x_) == truncate(x_) && truncate(other.y_) == truncate(y_)
            && truncate(other.z_) == truncate(z_);
    }

    /// Returns a hash code for this vector, identical to the one of Java.
    [[nodiscard]] constexpr std::int32_t hash_code() const noexcept {
        return (truncate(x_) >> 13) ^ (truncate(y_) >> 7) ^ truncate(z_);
    }

private:
    static constexpr double LIMIT = 2147483648.0;

    [[nodiscard]] static constexpr std::int32_t truncate(double num) noexcept {
        if (!(LIMIT > lang::Math::abs(num))) {
            return saturate(num);
        }

        return static_cast<std::int32_t>(num);
    }

    [[nodiscard]] static constexpr std::int32_t saturate(double num) noexcept {
        if (num != num) {
            return 0;
        }

        return 0.0 > num ? (std::numeric_limits<std::int32_t>::min)() : (std::numeric_limits<std::int32_t>::max)();
    }
};

}
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

#include <cmath>
#include <cstdint>
#include <random>

#include <minecraft/lang/double.hpp>
#include <minecraft/lang/math.hpp>
#include <minecraft/util/number_conversions.hpp>

namespace minecraft::util {

/// Represents a mutable vector.
///
/// Because the components of vectors are mutable, storing vectors long term
/// may be dangerous if passing code modifies the vector later. If you want to
/// keep around a vector, it may be wise to copy it.
///
/// Every comparison that may see NaN is written with ``>`` or ``>=``, MSVC
/// evaluates ``NaN < x`` and ``NaN <= x`` to true in constant expressions.
class Vector {
public:
    /// Constructs the vector with all components as 0.
    constexpr Vector() noexcept = default;

    /// Constructs the vector with the provided components.
    constexpr Vector(double x, double y, double z) noexcept : x_(x), y_(y), z_(z) {
    }

    /// Adds a vector to this one.
    constexpr Vector& add(const Vector& vec) noexcept {
        x_ += vec.x_;
        y_ += vec.y_;
        z_ += vec.z_;
        return *this;
    }

    /// Subtracts a vector from this one.
    constexpr Vector& subtract(const Vector& vec) noexcept {
        x_ -= vec.x_;
        y_ -= vec.y_;
        z_ -= vec.z_;
        return *this;
    }

    /// Multiplies the vector by another.
    constexpr Vector& multiply(const Vector& vec) noexcept {
        x_ *= vec.x_;
        y_ *= vec.y_;
        z_ *= vec.z_;
        return *this;
    }

    /// Performs scalar multiplication, multiplying all components with a scalar.
    constexpr Vector& multiply(double m) noexcept {
        x_ *= m;
        y_ *= m;
        z_ *= m;
        return *this;
    }

    /// Divides the vector by another.
    constexpr Vector& divide(const Vector& vec) noexcept {
        x_ /= vec.x_;
        y_ /= vec.y_;
        z_ /= vec.z_;
        return *this;
    }

    /// Copies another vector.
    constexpr Vector& copy(const Vector& vec) noexcept {
        x_ = vec.x_;
        y_ = vec.y_;
        z_ = vec.z_;
        return *this;
    }

    /// Gets the magnitude of the vector, defined as sqrt(x^2 + y^2 + z^2).
    ///
    /// The value is not cached and uses a costly square-root function, so do
    /// not repeatedly call this function to get the vector's magnitude.
    /// Infinity will be returned if the inner result overflows.
    [[nodiscard]] double length() const noexcept {
        return std::sqrt(length_squared());
    }

    /// Gets the magnitude of the vector squared.
    [[nodiscard]] constexpr double length_squared() const noexcept {
        return NumberConversions::square(x_) + NumberConversions::square(y_) + NumberConversions::square(z_);
    }

    /// Gets the distance between this vector and another.
    ///
    /// The value is not cached and uses a costly square-root function, so do
    /// not repeatedly call this function to get the distance.
    [[nodiscard]] double distance(const Vector& o) const noexcept {
        return std::sqrt(distance_squared(o));
    }

    /// Gets the squared distance between this vector and another.
    [[nodiscard]] constexpr double distance_squared(const Vector& o) const noexcept {
        return NumberConversions::square(x_ - o.x_) + NumberConversions::square(y_ - o.y_)
            + NumberConversions::square(z_ - o.z_);
    }

    /// Gets the angle between this vector and another in radians.
    ///
    /// Unlike Java, which narrows the result to a 32 bit float, the full
    /// precision is returned.
    [[nodiscard]] double angle(const Vector& other) const noexcept {
        const double cosine = dot(other) / (length() * other.length());
        return std::acos((lang::Math::min)((lang::Math::max)(cosine, -1.0), 1.0));
    }

    /// Sets this vector to the midpoint between this vector and another.
    constexpr Vector& midpoint(const Vector& other) noexcept {
        x_ = (x_ + other.x_) / 2;
        y_ = (y_ + other.y_) / 2;
        z_ = (z_ + other.z_) / 2;
        return *this;
    }

    /// Gets a new midpoint vector between this vector and another.
    [[nodiscard]] constexpr Vector get_midpoint(const Vector& other) const noexcept {
        return Vector((x_ + other.x_) / 2, (y_ + other.y_) / 2, (z_ + other.z_) / 2);
    }

    /// Calculates the dot product of this vector with another.
    ///
    /// The dot product is defined as x1 * x2 + y1 * y2 + z1 * z2.
    [[nodiscard]] constexpr double dot(const Vector& other) const noexcept {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

    /// Calculates the cross product of this vector with another.
    ///
    /// The cross product is defined as:
    ///
    /// - x = y1 * z2 - y2 * z1
    /// - y = z1 * x2 - z2 * x1
    /// - z = x1 * y2 - x2 * y1
    constexpr Vector& cross_product(const Vector& o) noexcept {
        return copy(get_cross_product(o));
    }

    /// Calculates the cross product of this vector with another without mutating the original.
    [[nodiscard]] constexpr Vector get_cross_product(const Vector& o) const noexcept {
        return Vector(y_ * o.z_ - o.y_ * z_, z_ * o.x_ - o.z_ * x_, x_ * o.y_ - o.x_ * y_);
    }

    /// Converts this vector to a unit vector (a vector with length of 1).
    ///
    /// The components are multiplied with the reciprocal of the length. This
    /// is faster than the three divisions of Java, the result may differ from
    /// the one of Java in the last bit.
    Vector& normalize() noexcept {
        return multiply(1.0 / length());
    }

    /// Zeroes this vector's components.
    constexpr Vector& zero() noexcept {
        x_ = 0.0;
        y_ = 0.0;
        z_ = 0.0;
        return *this;
    }

    /// Checks whether each component of this vector is equal to 0.
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return x_ == 0.0 && y_ == 0.0 && z_ == 0.0;
    }

    /// Converts each component of value -0.0 to 0.0.
    constexpr Vector& normalize_zeros() noexcept {
        if (x_ == 0.0) {
            x_ = 0.0;
        }
        if (y_ == 0.0) {
            y_ = 0.0;
        }
        if (z_ == 0.0) {
            z_ = 0.0;
        }

        return *this;
    }

    /// Returns whether this vector is in an axis-aligned bounding box.
    ///
    /// The minimum and maximum vectors given must be truly the minimum and
    /// maximum X, Y and Z components.
    [[nodiscard]] constexpr bool is_in_aabb(const Vector& min, const Vector& max) const noexcept {
        return x_ >= min.x_ && max.x_ >= x_ && y_ >= min.y_ && max.y_ >= y_ && z_ >= min.z_ && max.z_ >= z_;
    }

    /// Returns whether this vector is within a sphere.
    [[nodiscard]] constexpr bool is_in_sphere(const Vector& origin, double radius) const noexcept {
        return NumberConversions::square(radius) >= origin.distance_squared(*this);
    }

    /// Returns whether this vector is normalized.
    [[nodiscard]] constexpr bool is_normalized() const noexcept {
        return EPSILON > lang::Math::abs(length_squared() - 1);
    }

    /// Rotates the vector around the x axis.
    ///
    /// The angle is passed in radians.
    Vector& rotate_around_x(double angle) noexcept {
        const double angle_cos = std::cos(angle);
        const double angle_sin = std::sin(angle);
        const double y = angle_cos * y_ - angle_sin * z_;
        const double z = angle_sin * y_ + angle_cos * z_;
        return set_y(y).set_z(z);
    }

    /// Rotates the vector around the y axis.
    ///
    /// The angle is passed in radians.
    Vector& rotate_around_y(double angle) noexcept {
        const double angle_cos = std::cos(angle);
        const double angle_sin = std::sin(angle);
        const double x = angle_cos * x_ + angle_sin * z_;
        const double z = -angle_sin * x_ + angle_cos * z_;
        return set_x(x).set_z(z);
    }

    /// Rotates the vector around the z axis.
    ///
    /// The angle is passed in radians.
    Vector& rotate_around_z(double angle) noexcept {
        const double angle_cos = std::cos(angle);
        const double angle_sin = std::sin(angle);
        const double x = angle_cos * x_ - angle_sin * y_;
        const double y = angle_sin * x_ + angle_cos * y_;
        return set_x(x).set_y(y);
    }

    /// Rotates the vector around a given arbitrary axis in 3 dimensional space.
    ///
    /// Rotation will follow the general Right-Hand-Rule, which means rotation
    /// will be counterclockwise when the axis is pointing towards the observer.
    ///
    /// This function will always make sure the provided axis is a unit vector,
    /// to not modify the length of the vector when rotating.
    Vector& rotate_around_axis(const Vector& axis, double angle) noexcept {
        return rotate_around_non_unit_axis(axis.is_normalized() ? axis : Vector(axis).normalize(), angle);
    }

    /// Rotates the vector around a given arbitrary axis in 3 dimensional space.
    ///
    /// Rotation will follow the general Right-Hand-Rule, which means rotation
    /// will be counterclockwise when the axis is pointing towards the observer.
    ///
    /// Note that the vector length will change accordingly to the axis vector
    /// length. If the provided axis is not a unit vector, the rotated vector
    /// will not have its previous length.
    Vector& rotate_around_non_unit_axis(const Vector& axis, double angle) noexcept {
        const double x = x_;
        const double y = y_;
        const double z = z_;
        const double x2 = axis.x_;
        const double y2 = axis.y_;
        const double z2 = axis.z_;
        const double cos_theta = std::cos(angle);
        const double sin_theta = std::sin(angle);
        const double dot_product = dot(axis);
        const double x_prime = x2 * dot_product * (1.0 - cos_theta) + x * cos_theta + (-z2 * y + y2 * z) * sin_theta;
        const double y_prime = y2 * dot_product * (1.0 - cos_theta) + y * cos_theta + (z2 * x - x2 * z) * sin_theta;
        const double z_prime = z2 * dot_product * (1.0 - cos_theta) + z * cos_theta + (-y2 * x + x2 * y) * sin_theta;
        return set_x(x_prime).set_y(y_prime).set_z(z_prime);
    }

    /// Gets the X component.
    [[nodiscard]] constexpr double x() const noexcept {
        return x_;
    }

    /// Gets the floored value of the X component, indicating the block that this vector is contained with.
    [[nodiscard]] constexpr std::int64_t block_x() const noexcept {
        return NumberConversions::floor(x_);
    }

    /// Gets the Y component.
    [[nodiscard]] constexpr double y() const noexcept {
        return y_;
    }

    /// Gets the floored value of the Y component, indicating the block that this vector is contained with.
    [[nodiscard]] constexpr std::int64_t block_y() const noexcept {
        return NumberConversions::floor(y_);
    }

    /// Gets the Z component.
    [[nodiscard]] constexpr double z() const noexcept {
        return z_;
    }

    /// Gets the floored value of the Z component, indicating the block that this vector is contained with.
    [[nodiscard]] constexpr std::int64_t block_z() const noexcept {
        return NumberConversions::floor(z_);
    }

    /// Sets the X component.
    constexpr Vector& set_x(double x) noexcept {
        x_ = x;
        return *this;
    }

    /// Sets the Y component.
    constexpr Vector& set_y(double y) noexcept {
        y_ = y;
        return *this;
    }

    /// Sets the Z component.
    constexpr Vector& set_z(double z) noexcept {
        z_ = z;
        return *this;
    }

    /// Checks to see if two vectors are equal.
    ///
    /// This uses a fuzzy match to account for floating point errors, the
    /// threshold can be retrieved with get_epsilon().
    [[nodiscard]] constexpr bool equals(const Vector& other) const noexcept {
        return EPSILON > lang::Math::abs(x_ - other.x_) && EPSILON > lang::Math::abs(y_ - other.y_)
            && EPSILON > lang::Math::abs(z_ - other.z_);
    }

    /// Returns a hash code for this vector, identical to the one of Java.
    [[nodiscard]] constexpr std::int32_t hash_code() const noexcept {
        std::uint32_t hash = 7;
        hash = 79 * hash + lang::Double::hash_code(x_);
        hash = 79 * hash + lang::Double::hash_code(y_);
        hash = 79 * hash + lang::Double::hash_code(z_);
        return static_cast<std::int32_t>(hash);
    }

    /// Checks whether each component of this vector is finite.
    [[nodiscard]] constexpr bool is_finite() const noexcept {
        return NumberConversions::is_finite(x_) && NumberConversions::is_finite(y_) && NumberConversions::is_finite(z_);
    }

    /// Gets the threshold used for equals().
    [[nodiscard]] static constexpr double get_epsilon() noexcept {
        return EPSILON;
    }

    /// Gets the minimum components of two vectors.
    [[nodiscard]] static constexpr Vector get_minimum(const Vector& v1, const Vector& v2) noexcept {
        return Vector((lang::Math::min)(v1.x_, v2.x_), (lang::Math::min)(v1.y_, v2.y_), (lang::Math::min)(v1.z_, v2.z_));
    }

    /// Gets the maximum components of two vectors.
    [[nodiscard]] static constexpr Vector get_maximum(const Vector& v1, const Vector& v2) noexcept {
        return Vector((lang::Math::max)(v1.x_, v2.x_), (lang::Math::max)(v1.y_, v2.y_), (lang::Math::max)(v1.z_, v2.z_));
    }

    /// Gets a random vector with components having a random value between 0 and 1.
    template <typename Generator>
    [[nodiscard]] static Vector get_random(Generator& generator) {
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        const double x = distribution(generator);
        const double y = distribution(generator);
        const double z = distribution(generator);
        return Vector(x, y, z);
    }

protected:
    static constexpr double EPSILON = 0.000001;

    double x_ = 0.0;
    double y_ = 0.0;
    double z_ = 0.0;
};

}
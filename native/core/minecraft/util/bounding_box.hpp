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
#include <stdexcept>

#include <minecraft/lang/double.hpp>
#include <minecraft/lang/math.hpp>
#include <minecraft/util/number_conversions.hpp>
#include <minecraft/util/vector.hpp>

namespace minecraft::util {

/// A mutable axis aligned bounding box (AABB).
///
/// This basically represents a rectangular box (specified by minimum and
/// maximum corners) that can for example be used to describe the position and
/// extents of an object (such as an entity, block, or rectangular region) in
/// 3D space. Its edges and faces are parallel to the axes of the cartesian
/// coordinate system.
///
/// The bounding box may be degenerate (one or more sides having the length 0).
///
/// Because bounding boxes are mutable, storing them long term may be dangerous
/// if they get modified later. If you want to keep around a bounding box, it
/// may be wise to copy it.
///
/// Every function that changes the bounding box throws std::invalid_argument
/// if a corner coordinate would not be finite, and leaves it unchanged.
///
/// The names min and max are written in parentheses, which keeps the macros
/// of the same name that windows.h defines from replacing them.
class BoundingBox {
public:
    /// Creates a new (degenerate) bounding box with all corner coordinates at 0.
    constexpr BoundingBox() noexcept = default;

    /// Creates a new bounding box from the given corner coordinates.
    constexpr BoundingBox(double x1, double y1, double z1, double x2, double y2, double z2) {
        resize(x1, y1, z1, x2, y2, z2);
    }

    /// Creates a new bounding box using the coordinates of the given vectors as corners.
    [[nodiscard]] static constexpr BoundingBox of(const Vector& corner1, const Vector& corner2) {
        return BoundingBox(corner1.x(), corner1.y(), corner1.z(), corner2.x(), corner2.y(), corner2.z());
    }

    /// Creates a new bounding box using the given center and extents.
    ///
    /// The extents are half of the size of the bounding box along the axes.
    [[nodiscard]] static constexpr BoundingBox of(const Vector& center, double x, double y, double z) {
        return BoundingBox(
            center.x() - x, center.y() - y, center.z() - z, center.x() + x, center.y() + y, center.z() + z
        );
    }

    /// Resizes this bounding box.
    constexpr BoundingBox& resize(double x1, double y1, double z1, double x2, double y2, double z2) {
        if ((x1 - x1) + (y1 - y1) + (z1 - z1) + (x2 - x2) + (y2 - y2) + (z2 - z2) != 0.0) {
            check_finite(x1, "x1 not finite");
            check_finite(y1, "y1 not finite");
            check_finite(z1, "z1 not finite");
            check_finite(x2, "x2 not finite");
            check_finite(y2, "y2 not finite");
            check_finite(z2, "z2 not finite");
        }

        min_x_ = (lang::Math::min)(x1, x2);
        min_y_ = (lang::Math::min)(y1, y2);
        min_z_ = (lang::Math::min)(z1, z2);
        max_x_ = (lang::Math::max)(x1, x2);
        max_y_ = (lang::Math::max)(y1, y2);
        max_z_ = (lang::Math::max)(z1, z2);
        return *this;
    }

    /// Gets the minimum x value.
    [[nodiscard]] constexpr double min_x() const noexcept {
        return min_x_;
    }

    /// Gets the minimum y value.
    [[nodiscard]] constexpr double min_y() const noexcept {
        return min_y_;
    }

    /// Gets the minimum z value.
    [[nodiscard]] constexpr double min_z() const noexcept {
        return min_z_;
    }

    /// Gets the minimum corner as vector.
    [[nodiscard]] constexpr Vector (min)() const noexcept {
        return Vector(min_x_, min_y_, min_z_);
    }

    /// Gets the maximum x value.
    [[nodiscard]] constexpr double max_x() const noexcept {
        return max_x_;
    }

    /// Gets the maximum y value.
    [[nodiscard]] constexpr double max_y() const noexcept {
        return max_y_;
    }

    /// Gets the maximum z value.
    [[nodiscard]] constexpr double max_z() const noexcept {
        return max_z_;
    }

    /// Gets the maximum corner as vector.
    [[nodiscard]] constexpr Vector (max)() const noexcept {
        return Vector(max_x_, max_y_, max_z_);
    }

    /// Gets the width of the bounding box in the x direction.
    [[nodiscard]] constexpr double width_x() const noexcept {
        return max_x_ - min_x_;
    }

    /// Gets the width of the bounding box in the z direction.
    [[nodiscard]] constexpr double width_z() const noexcept {
        return max_z_ - min_z_;
    }

    /// Gets the height of the bounding box.
    [[nodiscard]] constexpr double height() const noexcept {
        return max_y_ - min_y_;
    }

    /// Gets the volume of the bounding box.
    [[nodiscard]] constexpr double volume() const noexcept {
        return height() * width_x() * width_z();
    }

    /// Gets the x coordinate of the center of the bounding box.
    [[nodiscard]] constexpr double center_x() const noexcept {
        return min_x_ + width_x() * 0.5;
    }

    /// Gets the y coordinate of the center of the bounding box.
    [[nodiscard]] constexpr double center_y() const noexcept {
        return min_y_ + height() * 0.5;
    }

    /// Gets the z coordinate of the center of the bounding box.
    [[nodiscard]] constexpr double center_z() const noexcept {
        return min_z_ + width_z() * 0.5;
    }

    /// Gets the center of the bounding box.
    [[nodiscard]] constexpr Vector center() const noexcept {
        return Vector(center_x(), center_y(), center_z());
    }

    /// Copies another bounding box.
    constexpr BoundingBox& copy(const BoundingBox& other) {
        return resize(other.min_x_, other.min_y_, other.min_z_, other.max_x_, other.max_y_, other.max_z_);
    }

    /// Expands this bounding box by the given values in the corresponding directions.
    ///
    /// Negative values will shrink the bounding box in the corresponding
    /// direction. Shrinking will be limited to the point where the affected
    /// opposite faces would meet if they shrank at uniform speeds.
    constexpr BoundingBox& expand(
        double negative_x, double negative_y, double negative_z, double positive_x, double positive_y, double positive_z
    ) {
        if (
            negative_x == 0.0 && negative_y == 0.0 && negative_z == 0.0
            && positive_x == 0.0 && positive_y == 0.0 && positive_z == 0.0
        ) {
            return *this;
        }

        double new_min_x = min_x_ - negative_x;
        double new_min_y = min_y_ - negative_y;
        double new_min_z = min_z_ - negative_z;
        double new_max_x = max_x_ + positive_x;
        double new_max_y = max_y_ + positive_y;
        double new_max_z = max_z_ + positive_z;
        limit(new_min_x, new_max_x, center_x());
        limit(new_min_y, new_max_y, center_y());
        limit(new_min_z, new_max_z, center_z());
        return resize(new_min_x, new_min_y, new_min_z, new_max_x, new_max_y, new_max_z);
    }

    /// Expands this bounding box uniformly by the given values in both positive and negative directions.
    ///
    /// Negative values will shrink the bounding box. Shrinking will be limited
    /// to the bounding box's current size.
    constexpr BoundingBox& expand(double x, double y, double z) {
        return expand(x, y, z, x, y, z);
    }

    /// Expands this bounding box uniformly by the given values in both positive and negative directions.
    ///
    /// Negative values will shrink the bounding box. Shrinking will be limited
    /// to the bounding box's current size.
    constexpr BoundingBox& expand(const Vector& expansion) {
        return expand(expansion.x(), expansion.y(), expansion.z());
    }

    /// Expands this bounding box uniformly by the given value in all directions.
    ///
    /// A negative value will shrink the bounding box. Shrinking will be limited
    /// to the bounding box's current size.
    constexpr BoundingBox& expand(double expansion) {
        return expand(expansion, expansion, expansion, expansion, expansion, expansion);
    }

    /// Expands this bounding box in the specified direction.
    ///
    /// The magnitude of the direction will scale the expansion. A negative
    /// expansion value will shrink the bounding box in this direction.
    /// Shrinking will be limited to the bounding box's current size.
    constexpr BoundingBox& expand(double dir_x, double dir_y, double dir_z, double expansion) {
        if (expansion == 0.0 || (dir_x == 0.0 && dir_y == 0.0 && dir_z == 0.0)) {
            return *this;
        }

        const double negative_x = 0.0 > dir_x ? -dir_x * expansion : 0.0;
        const double negative_y = 0.0 > dir_y ? -dir_y * expansion : 0.0;
        const double negative_z = 0.0 > dir_z ? -dir_z * expansion : 0.0;
        const double positive_x = dir_x > 0.0 ? dir_x * expansion : 0.0;
        const double positive_y = dir_y > 0.0 ? dir_y * expansion : 0.0;
        const double positive_z = dir_z > 0.0 ? dir_z * expansion : 0.0;
        return expand(negative_x, negative_y, negative_z, positive_x, positive_y, positive_z);
    }

    /// Expands this bounding box in the specified direction.
    ///
    /// The magnitude of the direction will scale the expansion. A negative
    /// expansion value will shrink the bounding box in this direction.
    /// Shrinking will be limited to the bounding box's current size.
    constexpr BoundingBox& expand(const Vector& direction, double expansion) {
        return expand(direction.x(), direction.y(), direction.z(), expansion);
    }

    /// Expands this bounding box in the specified direction.
    ///
    /// The magnitude of the direction vector is used as the expansion.
    constexpr BoundingBox& expand_directional(double dir_x, double dir_y, double dir_z) {
        return expand(dir_x, dir_y, dir_z, 1.0);
    }

    /// Expands this bounding box in the specified direction.
    ///
    /// The magnitude of the direction vector is used as the expansion.
    constexpr BoundingBox& expand_directional(const Vector& direction) {
        return expand(direction.x(), direction.y(), direction.z(), 1.0);
    }

    /// Expands this bounding box to contain (or border) the specified position.
    ///
    /// This is the union of Java, which is a reserved word in C++.
    constexpr BoundingBox& unite(double pos_x, double pos_y, double pos_z) {
        const double new_min_x = (lang::Math::min)(min_x_, pos_x);
        const double new_min_y = (lang::Math::min)(min_y_, pos_y);
        const double new_min_z = (lang::Math::min)(min_z_, pos_z);
        const double new_max_x = (lang::Math::max)(max_x_, pos_x);
        const double new_max_y = (lang::Math::max)(max_y_, pos_y);
        const double new_max_z = (lang::Math::max)(max_z_, pos_z);
        if (
            new_min_x == min_x_ && new_min_y == min_y_ && new_min_z == min_z_
            && new_max_x == max_x_ && new_max_y == max_y_ && new_max_z == max_z_
        ) {
            return *this;
        }

        return resize(new_min_x, new_min_y, new_min_z, new_max_x, new_max_y, new_max_z);
    }

    /// Expands this bounding box to contain (or border) the specified position.
    ///
    /// This is the union of Java, which is a reserved word in C++.
    constexpr BoundingBox& unite(const Vector& position) {
        return unite(position.x(), position.y(), position.z());
    }

    /// Expands this bounding box to contain both this and the given bounding box.
    ///
    /// This is the union of Java, which is a reserved word in C++.
    constexpr BoundingBox& unite(const BoundingBox& other) {
        if (contains(other)) {
            return *this;
        }

        return resize(
            (lang::Math::min)(min_x_, other.min_x_),
            (lang::Math::min)(min_y_, other.min_y_),
            (lang::Math::min)(min_z_, other.min_z_),
            (lang::Math::max)(max_x_, other.max_x_),
            (lang::Math::max)(max_y_, other.max_y_),
            (lang::Math::max)(max_z_, other.max_z_)
        );
    }

    /// Resizes this bounding box to represent the intersection of this and the given bounding box.
    ///
    /// Throws std::invalid_argument if the bounding boxes do not overlap.
    constexpr BoundingBox& intersection(const BoundingBox& other) {
        if (!overlaps(other)) {
            throw std::invalid_argument("The bounding boxes do not overlap!");
        }

        return resize(
            (lang::Math::max)(min_x_, other.min_x_),
            (lang::Math::max)(min_y_, other.min_y_),
            (lang::Math::max)(min_z_, other.min_z_),
            (lang::Math::min)(max_x_, other.max_x_),
            (lang::Math::min)(max_y_, other.max_y_),
            (lang::Math::min)(max_z_, other.max_z_)
        );
    }

    /// Shifts this bounding box by the given amounts.
    constexpr BoundingBox& shift(double shift_x, double shift_y, double shift_z) {
        if (shift_x == 0.0 && shift_y == 0.0 && shift_z == 0.0) {
            return *this;
        }

        return resize(
            min_x_ + shift_x, min_y_ + shift_y, min_z_ + shift_z, max_x_ + shift_x, max_y_ + shift_y, max_z_ + shift_z
        );
    }

    /// Shifts this bounding box by the given amounts.
    constexpr BoundingBox& shift(const Vector& shift) {
        return this->shift(shift.x(), shift.y(), shift.z());
    }

    /// Checks if this bounding box overlaps with the given bounding box.
    ///
    /// Bounding boxes that are only intersecting at the borders are not
    /// considered overlapping.
    [[nodiscard]] constexpr bool overlaps(const BoundingBox& other) const noexcept {
        return overlaps(other.min_x_, other.min_y_, other.min_z_, other.max_x_, other.max_y_, other.max_z_);
    }

    /// Checks if this bounding box overlaps with the bounding box that is defined by the given corners.
    ///
    /// Bounding boxes that are only intersecting at the borders are not
    /// considered overlapping.
    [[nodiscard]] constexpr bool overlaps(const Vector& min, const Vector& max) const noexcept {
        return overlaps(
            (lang::Math::min)(min.x(), max.x()),
            (lang::Math::min)(min.y(), max.y()),
            (lang::Math::min)(min.z(), max.z()),
            (lang::Math::max)(min.x(), max.x()),
            (lang::Math::max)(min.y(), max.y()),
            (lang::Math::max)(min.z(), max.z())
        );
    }

    /// Checks if this bounding box contains the specified position.
    ///
    /// Positions exactly on the minimum borders of the bounding box are
    /// considered to be inside the bounding box, while positions exactly on
    /// the maximum borders are considered to be outside. This allows bounding
    /// boxes to reside directly next to each other with positions always only
    /// residing in exactly one of them.
    [[nodiscard]] constexpr bool contains(double x, double y, double z) const noexcept {
        return x >= min_x_ && max_x_ > x && y >= min_y_ && max_y_ > y && z >= min_z_ && max_z_ > z;
    }

    /// Checks if this bounding box contains the specified position.
    [[nodiscard]] constexpr bool contains(const Vector& position) const noexcept {
        return contains(position.x(), position.y(), position.z());
    }

    /// Checks if this bounding box fully contains the given bounding box.
    [[nodiscard]] constexpr bool contains(const BoundingBox& other) const noexcept {
        return contains(other.min_x_, other.min_y_, other.min_z_, other.max_x_, other.max_y_, other.max_z_);
    }

    /// Checks if this bounding box fully contains the bounding box that is defined by the given corners.
    [[nodiscard]] constexpr bool contains(const Vector& min, const Vector& max) const noexcept {
        return contains(
            (lang::Math::min)(min.x(), max.x()),
            (lang::Math::min)(min.y(), max.y()),
            (lang::Math::min)(min.z(), max.z()),
            (lang::Math::max)(min.x(), max.x()),
            (lang::Math::max)(min.y(), max.y()),
            (lang::Math::max)(min.z(), max.z())
        );
    }

    /// Checks to see if two bounding boxes are equal, which compares the corner coordinates exactly.
    [[nodiscard]] constexpr bool equals(const BoundingBox& other) const noexcept {
        return lang::Double::to_long_bits(max_x_) == lang::Double::to_long_bits(other.max_x_)
            && lang::Double::to_long_bits(max_y_) == lang::Double::to_long_bits(other.max_y_)
            && lang::Double::to_long_bits(max_z_) == lang::Double::to_long_bits(other.max_z_)
            && lang::Double::to_long_bits(min_x_) == lang::Double::to_long_bits(other.min_x_)
            && lang::Double::to_long_bits(min_y_) == lang::Double::to_long_bits(other.min_y_)
            && lang::Double::to_long_bits(min_z_) == lang::Double::to_long_bits(other.min_z_);
    }

    /// Returns a hash code for this bounding box, identical to the one of Java.
    [[nodiscard]] constexpr std::int32_t hash_code() const noexcept {
        std::uint32_t result = 1;
        result = 31 * result + lang::Double::hash_code(max_x_);
        result = 31 * result + lang::Double::hash_code(max_y_);
        result = 31 * result + lang::Double::hash_code(max_z_);
        result = 31 * result + lang::Double::hash_code(min_x_);
        result = 31 * result + lang::Double::hash_code(min_y_);
        result = 31 * result + lang::Double::hash_code(min_z_);
        return static_cast<std::int32_t>(result);
    }

private:
    double min_x_ = 0.0;
    double min_y_ = 0.0;
    double min_z_ = 0.0;
    double max_x_ = 0.0;
    double max_y_ = 0.0;
    double max_z_ = 0.0;

    static constexpr void check_finite(double d, const char* message) {
        if (!NumberConversions::is_finite(d)) {
            throw std::invalid_argument(message);
        }
    }

    static constexpr void limit(double& new_min, double& new_max, double center) noexcept {
        if (!(new_min > new_max)) {
            return;
        }

        if (new_max >= center) {
            new_min = new_max;
        } else if (center >= new_min) {
            new_max = new_min;
        } else {
            new_min = center;
            new_max = center;
        }
    }

    [[nodiscard]] constexpr bool overlaps(
        double min_x, double min_y, double min_z, double max_x, double max_y, double max_z
    ) const noexcept {
        return max_x > min_x_ && max_x_ > min_x && max_y > min_y_ && max_y_ > min_y && max_z > min_z_ && max_z_ > min_z;
    }

    [[nodiscard]] constexpr bool contains(
        double min_x, double min_y, double min_z, double max_x, double max_y, double max_z
    ) const noexcept {
        return min_x >= min_x_ && max_x_ >= max_x && min_y >= min_y_ && max_y_ >= max_y
            && min_z >= min_z_ && max_z_ >= max_z;
    }
};

}
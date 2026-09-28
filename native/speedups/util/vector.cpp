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

#include <speedups/util/vector.hpp>

#include <array>
#include <cmath>
#include <random>

#include <minecraft/lang/math.hpp>
#include <minecraft/python/arguments.hpp>
#include <minecraft/python/object.hpp>
#include <minecraft/python/type.hpp>
#include <minecraft/util/number_conversions.hpp>

namespace minecraft::speedups::util {

using minecraft::lang::Math;
using minecraft::python::Arguments;
using minecraft::python::Object;
using minecraft::python::Type;
using minecraft::util::NumberConversions;
using minecraft::util::Vector;

static constexpr std::array<const char*, 3> NAMES = {"x", "y", "z"};

bool VectorType::add_to(PyObject* module, State& state) noexcept {
    state.vector_type = Type::create(module, spec);
    return state.vector_type != nullptr;
}

bool VectorType::check(PyObject* object) noexcept {
    return Type::is_instance(object, spec);
}

PyObject* VectorType::create(PyTypeObject* type, const Vector& vector) noexcept {
    PyObject* self = type->tp_alloc(type, 0);
    if (self != nullptr) {
        value(self) = vector;
    }

    return self;
}

bool VectorType::parse(
    const char* function, PyObject* const* args, Py_ssize_t count, PyObject* kwnames, Vector& result
) noexcept {
    std::array<double, 3> components = {0.0, 0.0, 0.0};
    if (!Arguments::to_doubles(function, args, count, kwnames, NAMES, components)) {
        return false;
    }

    result = Vector(components[0], components[1], components[2]);
    return true;
}

bool VectorType::parse(const char* function, PyObject* args, PyObject* kwargs, Vector& result) noexcept {
    std::array<double, 3> components = {0.0, 0.0, 0.0};
    if (!Arguments::to_doubles(function, args, kwargs, NAMES, components)) {
        return false;
    }

    result = Vector(components[0], components[1], components[2]);
    return true;
}

const Vector* VectorType::operand(PyObject* self, PyObject* object, const char* function) noexcept {
    if (Py_TYPE(object) == Py_TYPE(self) || check(object)) {
        return &value(object);
    }

    PyErr_Format(PyExc_TypeError, "%s() argument must be Vector, not %T", function, object);
    return nullptr;
}

const Vector* VectorType::operand(PyObject* object, const char* function) noexcept {
    if (check(object)) {
        return &value(object);
    }

    PyErr_Format(PyExc_TypeError, "%s() argument must be Vector, not %T", function, object);
    return nullptr;
}

PyObject* VectorType::create_exact(PyTypeObject* origin, const Vector& vector) noexcept {
    const State* state = Module::state_of(origin);
    if (state == nullptr) {
        return nullptr;
    }

    return create(state->vector_type, vector);
}

PyObject* VectorType::block(double component) noexcept {
    if (9223372036854775808.0 > Math::abs(component)) {
        return PyLong_FromLongLong(NumberConversions::floor(component));
    }

    return PyLong_FromDouble(std::floor(component));
}

int VectorType::set(PyObject* component, const char* name, double& result) noexcept {
    if (component == nullptr) {
        PyErr_Format(PyExc_AttributeError, "cannot delete attribute '%s'", name);
        return -1;
    }

    return Arguments::to_double(component, result) ? 0 : -1;
}

PyDoc_STRVAR(
    type_doc,
    "Vector(x=0.0, y=0.0, z=0.0)\n"
    "--\n"
    "\n"
    "Represents a mutable vector.\n"
    "\n"
    "Because the components of vectors are mutable, storing vectors long term\n"
    "may be dangerous if passing code modifies the vector later. If you want to\n"
    "keep around a vector, it may be wise to call :meth:`clone` in order to get\n"
    "a copy.\n"
    "\n"
    ".. container:: operations\n"
    "\n"
    "    .. describe:: x == y\n"
    "\n"
    "        Checks if two vectors are equal. Only two vectors of the same class\n"
    "        can ever be equal. This uses a fuzzy match to account for floating\n"
    "        point errors, the threshold can be retrieved with :meth:`get_epsilon`.\n"
    "\n"
    "    .. describe:: x != y\n"
    "\n"
    "        Checks if two vectors are not equal.\n"
    "\n"
    "    .. describe:: hash(x)\n"
    "\n"
    "        Returns the hash code of the vector, identical to the one of Java.\n"
    "\n"
    "    .. describe:: str(x)\n"
    "\n"
    "        Returns the components of the vector as ``x,y,z``.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "x: :class:`float`\n"
    "    The X component.\n"
    "y: :class:`float`\n"
    "    The Y component.\n"
    "z: :class:`float`\n"
    "    The Z component."
);

int VectorType::init(PyObject* self, PyObject* args, PyObject* kwargs) noexcept {
    return parse("Vector", args, kwargs, value(self)) ? 0 : -1;
}

PyObject* VectorType::vectorcall(PyObject* type, PyObject* const* args, std::size_t nargsf, PyObject* kwnames) noexcept {
    Vector vector;
    if (!parse("Vector", args, PyVectorcall_NARGS(nargsf), kwnames, vector)) {
        return nullptr;
    }

    return create(reinterpret_cast<PyTypeObject*>(type), vector);
}

PyObject* VectorType::repr(PyObject* self) noexcept {
    const Vector& vector = value(self);
    Object name(PyType_GetName(Py_TYPE(self)));
    Object x(PyFloat_FromDouble(vector.x()));
    Object y(PyFloat_FromDouble(vector.y()));
    Object z(PyFloat_FromDouble(vector.z()));
    if (!name || !x || !y || !z) {
        return nullptr;
    }

    return PyUnicode_FromFormat("<%U x=%R y=%R z=%R>", name.get(), x.get(), y.get(), z.get());
}

PyObject* VectorType::str(PyObject* self) noexcept {
    const Vector& vector = value(self);
    Object x(PyFloat_FromDouble(vector.x()));
    Object y(PyFloat_FromDouble(vector.y()));
    Object z(PyFloat_FromDouble(vector.z()));
    if (!x || !y || !z) {
        return nullptr;
    }

    return PyUnicode_FromFormat("%R,%R,%R", x.get(), y.get(), z.get());
}

Py_hash_t VectorType::hash(PyObject* self) noexcept {
    const Py_hash_t code = value(self).hash_code();
    return code == -1 ? -2 : code;
}

PyObject* VectorType::richcompare(PyObject* self, PyObject* other, int operation) noexcept {
    if ((operation != Py_EQ && operation != Py_NE) || !check(other)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    const bool equal = Py_TYPE(self) == Py_TYPE(other) && value(self).equals(value(other));
    return PyBool_FromLong(equal == (operation == Py_EQ));
}

PyDoc_STRVAR(
    add_doc,
    "add($self, vec, /)\n"
    "--\n"
    "\n"
    "Adds a vector to this one.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "vec: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::add(PyObject* self, PyObject* vec) noexcept {
    const Vector* other = operand(self, vec, "add");
    if (other == nullptr) {
        return nullptr;
    }

    value(self).add(*other);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    subtract_doc,
    "subtract($self, vec, /)\n"
    "--\n"
    "\n"
    "Subtracts a vector from this one.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "vec: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::subtract(PyObject* self, PyObject* vec) noexcept {
    const Vector* other = operand(self, vec, "subtract");
    if (other == nullptr) {
        return nullptr;
    }

    value(self).subtract(*other);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    multiply_doc,
    "multiply($self, m, /)\n"
    "--\n"
    "\n"
    "Multiplies the vector by another, or performs scalar multiplication,\n"
    "multiplying all components with a scalar.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "m: Union[:class:`Vector`, :class:`float`]\n"
    "    The other vector or the factor.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::multiply(PyObject* self, PyObject* m) noexcept {
    if (PyFloat_CheckExact(m)) {
        value(self).multiply(PyFloat_AS_DOUBLE(m));
        return Py_NewRef(self);
    }
    if (Py_TYPE(m) == Py_TYPE(self) || check(m)) {
        value(self).multiply(value(m));
        return Py_NewRef(self);
    }

    double factor = 0.0;
    if (!Arguments::to_double(m, factor)) {
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Format(PyExc_TypeError, "multiply() argument must be Vector or a real number, not %T", m);
        }

        return nullptr;
    }

    value(self).multiply(factor);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    divide_doc,
    "divide($self, vec, /)\n"
    "--\n"
    "\n"
    "Divides the vector by another.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "vec: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::divide(PyObject* self, PyObject* vec) noexcept {
    const Vector* other = operand(self, vec, "divide");
    if (other == nullptr) {
        return nullptr;
    }

    value(self).divide(*other);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    copy_doc,
    "copy($self, vec, /)\n"
    "--\n"
    "\n"
    "Copies another vector.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "vec: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::copy(PyObject* self, PyObject* vec) noexcept {
    const Vector* other = operand(self, vec, "copy");
    if (other == nullptr) {
        return nullptr;
    }

    value(self).copy(*other);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    length_doc,
    "length($self, /)\n"
    "--\n"
    "\n"
    "Gets the magnitude of the vector, defined as sqrt(x^2 + y^2 + z^2).\n"
    "\n"
    "The value of this method is not cached and uses a costly square-root\n"
    "function, so do not repeatedly call this method to get the vector's\n"
    "magnitude. Infinity will be returned if the inner result overflows, which\n"
    "will be caused if the length is too long.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The magnitude."
);

PyObject* VectorType::length(PyObject* self, PyObject*) noexcept {
    return PyFloat_FromDouble(value(self).length());
}

PyDoc_STRVAR(
    length_squared_doc,
    "length_squared($self, /)\n"
    "--\n"
    "\n"
    "Gets the magnitude of the vector squared.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The magnitude."
);

PyObject* VectorType::length_squared(PyObject* self, PyObject*) noexcept {
    return PyFloat_FromDouble(value(self).length_squared());
}

PyDoc_STRVAR(
    distance_doc,
    "distance($self, o, /)\n"
    "--\n"
    "\n"
    "Gets the distance between this vector and another.\n"
    "\n"
    "The value of this method is not cached and uses a costly square-root\n"
    "function, so do not repeatedly call this method to get the distance.\n"
    "Infinity will be returned if the inner result overflows, which will be\n"
    "caused if the distance is too long.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "o: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The distance."
);

PyObject* VectorType::distance(PyObject* self, PyObject* o) noexcept {
    const Vector* other = operand(self, o, "distance");
    if (other == nullptr) {
        return nullptr;
    }

    return PyFloat_FromDouble(value(self).distance(*other));
}

PyDoc_STRVAR(
    distance_squared_doc,
    "distance_squared($self, o, /)\n"
    "--\n"
    "\n"
    "Gets the squared distance between this vector and another.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "o: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The distance."
);

PyObject* VectorType::distance_squared(PyObject* self, PyObject* o) noexcept {
    const Vector* other = operand(self, o, "distance_squared");
    if (other == nullptr) {
        return nullptr;
    }

    return PyFloat_FromDouble(value(self).distance_squared(*other));
}

PyDoc_STRVAR(
    angle_doc,
    "angle($self, other, /)\n"
    "--\n"
    "\n"
    "Gets the angle between this vector and another in radians.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "other: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The angle in radians."
);

PyObject* VectorType::angle(PyObject* self, PyObject* other) noexcept {
    const Vector* vector = operand(self, other, "angle");
    if (vector == nullptr) {
        return nullptr;
    }

    return PyFloat_FromDouble(value(self).angle(*vector));
}

PyDoc_STRVAR(
    midpoint_doc,
    "midpoint($self, other, /)\n"
    "--\n"
    "\n"
    "Sets this vector to the midpoint between this vector and another.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "other: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    This same vector (now a midpoint)."
);

PyObject* VectorType::midpoint(PyObject* self, PyObject* other) noexcept {
    const Vector* vector = operand(self, other, "midpoint");
    if (vector == nullptr) {
        return nullptr;
    }

    value(self).midpoint(*vector);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    get_midpoint_doc,
    "get_midpoint($self, other, /)\n"
    "--\n"
    "\n"
    "Gets a new midpoint vector between this vector and another.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "other: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    A new midpoint vector."
);

PyObject* VectorType::get_midpoint(PyObject* self, PyObject* other) noexcept {
    const Vector* vector = operand(self, other, "get_midpoint");
    if (vector == nullptr) {
        return nullptr;
    }

    return create_exact(Py_TYPE(self), value(self).get_midpoint(*vector));
}

PyDoc_STRVAR(
    dot_doc,
    "dot($self, other, /)\n"
    "--\n"
    "\n"
    "Calculates the dot product of this vector with another.\n"
    "\n"
    "The dot product is defined as x1 * x2 + y1 * y2 + z1 * z2. The returned\n"
    "value is a scalar.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "other: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The dot product."
);

PyObject* VectorType::dot(PyObject* self, PyObject* other) noexcept {
    const Vector* vector = operand(self, other, "dot");
    if (vector == nullptr) {
        return nullptr;
    }

    return PyFloat_FromDouble(value(self).dot(*vector));
}

PyDoc_STRVAR(
    cross_product_doc,
    "cross_product($self, o, /)\n"
    "--\n"
    "\n"
    "Calculates the cross product of this vector with another.\n"
    "\n"
    "The cross product is defined as:\n"
    "\n"
    "- x = y1 * z2 - y2 * z1\n"
    "- y = z1 * x2 - z2 * x1\n"
    "- z = x1 * y2 - x2 * y1\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "o: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::cross_product(PyObject* self, PyObject* o) noexcept {
    const Vector* other = operand(self, o, "cross_product");
    if (other == nullptr) {
        return nullptr;
    }

    value(self).cross_product(*other);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    get_cross_product_doc,
    "get_cross_product($self, o, /)\n"
    "--\n"
    "\n"
    "Calculates the cross product of this vector with another without mutating\n"
    "the original.\n"
    "\n"
    "The cross product is defined as:\n"
    "\n"
    "- x = y1 * z2 - y2 * z1\n"
    "- y = z1 * x2 - z2 * x1\n"
    "- z = x1 * y2 - x2 * y1\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "o: :class:`Vector`\n"
    "    The other vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    A new vector."
);

PyObject* VectorType::get_cross_product(PyObject* self, PyObject* o) noexcept {
    const Vector* other = operand(self, o, "get_cross_product");
    if (other == nullptr) {
        return nullptr;
    }

    return create_exact(Py_TYPE(self), value(self).get_cross_product(*other));
}

PyDoc_STRVAR(
    normalize_doc,
    "normalize($self, /)\n"
    "--\n"
    "\n"
    "Converts this vector to a unit vector (a vector with length of 1).\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::normalize(PyObject* self, PyObject*) noexcept {
    value(self).normalize();
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    zero_doc,
    "zero($self, /)\n"
    "--\n"
    "\n"
    "Zero this vector's components.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::zero(PyObject* self, PyObject*) noexcept {
    value(self).zero();
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    is_zero_doc,
    "is_zero($self, /)\n"
    "--\n"
    "\n"
    "Check whether or not each component of this vector is equal to 0.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`bool`\n"
    "    ``True`` if equal to zero, ``False`` if at least one component is non-zero."
);

PyObject* VectorType::is_zero(PyObject* self, PyObject*) noexcept {
    return PyBool_FromLong(value(self).is_zero());
}

PyDoc_STRVAR(
    is_in_aabb_doc,
    "is_in_aabb($self, min, max, /)\n"
    "--\n"
    "\n"
    "Returns whether this vector is in an axis-aligned bounding box.\n"
    "\n"
    "The minimum and maximum vectors given must be truly the minimum and\n"
    "maximum X, Y and Z components.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "min: :class:`Vector`\n"
    "    The minimum vector.\n"
    "max: :class:`Vector`\n"
    "    The maximum vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`bool`\n"
    "    Whether this vector is in the AABB."
);

PyObject* VectorType::is_in_aabb(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("is_in_aabb", count, 2, 2)) {
        return nullptr;
    }

    const Vector* min = operand(self, args[0], "is_in_aabb");
    if (min == nullptr) {
        return nullptr;
    }

    const Vector* max = operand(self, args[1], "is_in_aabb");
    if (max == nullptr) {
        return nullptr;
    }

    return PyBool_FromLong(value(self).is_in_aabb(*min, *max));
}

PyDoc_STRVAR(
    is_in_sphere_doc,
    "is_in_sphere($self, origin, radius, /)\n"
    "--\n"
    "\n"
    "Returns whether this vector is within a sphere.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "origin: :class:`Vector`\n"
    "    The sphere origin.\n"
    "radius: :class:`float`\n"
    "    The sphere radius.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`bool`\n"
    "    Whether this vector is in the sphere."
);

PyObject* VectorType::is_in_sphere(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("is_in_sphere", count, 2, 2)) {
        return nullptr;
    }

    const Vector* origin = operand(self, args[0], "is_in_sphere");
    if (origin == nullptr) {
        return nullptr;
    }

    double radius = 0.0;
    if (!Arguments::to_double(args[1], radius)) {
        return nullptr;
    }

    return PyBool_FromLong(value(self).is_in_sphere(*origin, radius));
}

PyDoc_STRVAR(
    is_normalized_doc,
    "is_normalized($self, /)\n"
    "--\n"
    "\n"
    "Returns if a vector is normalized.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`bool`\n"
    "    Whether the vector is normalised."
);

PyObject* VectorType::is_normalized(PyObject* self, PyObject*) noexcept {
    return PyBool_FromLong(value(self).is_normalized());
}

PyDoc_STRVAR(
    rotate_around_x_doc,
    "rotate_around_x($self, angle, /)\n"
    "--\n"
    "\n"
    "Rotates the vector around the x axis.\n"
    "\n"
    "This piece of math is based on the standard rotation matrix for vectors\n"
    "in three dimensional space.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "angle: :class:`float`\n"
    "    The angle to rotate the vector about. This angle is passed in radians.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::rotate_around_x(PyObject* self, PyObject* angle) noexcept {
    double radians = 0.0;
    if (!Arguments::to_double(angle, radians)) {
        return nullptr;
    }

    value(self).rotate_around_x(radians);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    rotate_around_y_doc,
    "rotate_around_y($self, angle, /)\n"
    "--\n"
    "\n"
    "Rotates the vector around the y axis.\n"
    "\n"
    "This piece of math is based on the standard rotation matrix for vectors\n"
    "in three dimensional space.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "angle: :class:`float`\n"
    "    The angle to rotate the vector about. This angle is passed in radians.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::rotate_around_y(PyObject* self, PyObject* angle) noexcept {
    double radians = 0.0;
    if (!Arguments::to_double(angle, radians)) {
        return nullptr;
    }

    value(self).rotate_around_y(radians);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    rotate_around_z_doc,
    "rotate_around_z($self, angle, /)\n"
    "--\n"
    "\n"
    "Rotates the vector around the z axis.\n"
    "\n"
    "This piece of math is based on the standard rotation matrix for vectors\n"
    "in three dimensional space.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "angle: :class:`float`\n"
    "    The angle to rotate the vector about. This angle is passed in radians.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::rotate_around_z(PyObject* self, PyObject* angle) noexcept {
    double radians = 0.0;
    if (!Arguments::to_double(angle, radians)) {
        return nullptr;
    }

    value(self).rotate_around_z(radians);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    rotate_around_axis_doc,
    "rotate_around_axis($self, axis, angle, /)\n"
    "--\n"
    "\n"
    "Rotates the vector around a given arbitrary axis in 3 dimensional space.\n"
    "\n"
    "Rotation will follow the general Right-Hand-Rule, which means rotation\n"
    "will be counterclockwise when the axis is pointing towards the observer.\n"
    "\n"
    "This method will always make sure the provided axis is a unit vector, to\n"
    "not modify the length of the vector when rotating. If you are experienced\n"
    "with the scaling of a non-unit axis vector, you can use\n"
    ":meth:`rotate_around_non_unit_axis`.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "axis: :class:`Vector`\n"
    "    The axis to rotate the vector around. If the passed vector is not of\n"
    "    length 1, it gets copied and normalized before using it for the\n"
    "    rotation.\n"
    "angle: :class:`float`\n"
    "    The angle to rotate the vector around the axis.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::rotate_around_axis(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("rotate_around_axis", count, 2, 2)) {
        return nullptr;
    }

    const Vector* axis = operand(self, args[0], "rotate_around_axis");
    if (axis == nullptr) {
        return nullptr;
    }

    double radians = 0.0;
    if (!Arguments::to_double(args[1], radians)) {
        return nullptr;
    }

    const Vector copy = *axis;
    value(self).rotate_around_axis(copy, radians);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    rotate_around_non_unit_axis_doc,
    "rotate_around_non_unit_axis($self, axis, angle, /)\n"
    "--\n"
    "\n"
    "Rotates the vector around a given arbitrary axis in 3 dimensional space.\n"
    "\n"
    "Rotation will follow the general Right-Hand-Rule, which means rotation\n"
    "will be counterclockwise when the axis is pointing towards the observer.\n"
    "\n"
    "Note that the vector length will change accordingly to the axis vector\n"
    "length. If the provided axis is not a unit vector, the rotated vector\n"
    "will not have its previous length. The scaled length of the resulting\n"
    "vector will be related to the axis vector. If you are not perfectly sure\n"
    "about the scaling of the vector, use :meth:`rotate_around_axis`.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "axis: :class:`Vector`\n"
    "    The axis to rotate the vector around.\n"
    "angle: :class:`float`\n"
    "    The angle to rotate the vector around the axis.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The same vector."
);

PyObject* VectorType::rotate_around_non_unit_axis(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("rotate_around_non_unit_axis", count, 2, 2)) {
        return nullptr;
    }

    const Vector* axis = operand(self, args[0], "rotate_around_non_unit_axis");
    if (axis == nullptr) {
        return nullptr;
    }

    double radians = 0.0;
    if (!Arguments::to_double(args[1], radians)) {
        return nullptr;
    }

    const Vector copy = *axis;
    value(self).rotate_around_non_unit_axis(copy, radians);
    return Py_NewRef(self);
}

PyDoc_STRVAR(
    clone_doc,
    "clone($self, /)\n"
    "--\n"
    "\n"
    "Get a new vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    A new vector of the same class with the same components."
);

PyObject* VectorType::clone(PyObject* self, PyObject*) noexcept {
    return create(Py_TYPE(self), value(self));
}

PyDoc_STRVAR(
    to_block_vector_doc,
    "to_block_vector($self, /)\n"
    "--\n"
    "\n"
    "Get the block vector of this vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`BlockVector`\n"
    "    A block vector."
);

PyObject* VectorType::to_block_vector(PyObject* self, PyObject*) noexcept {
    const State* state = Module::state_of(Py_TYPE(self));
    if (state == nullptr) {
        return nullptr;
    }

    return create(state->block_vector_type, value(self));
}

PyDoc_STRVAR(
    check_finite_doc,
    "check_finite($self, /)\n"
    "--\n"
    "\n"
    "Check if each component of this vector is finite.\n"
    "\n"
    "Raises\n"
    "-------\n"
    "ValueError\n"
    "    Any component is not finite."
);

PyObject* VectorType::check_finite(PyObject* self, PyObject*) noexcept {
    const Vector& vector = value(self);
    const char* message = nullptr;
    if (!NumberConversions::is_finite(vector.x())) {
        message = "x not finite";
    } else if (!NumberConversions::is_finite(vector.y())) {
        message = "y not finite";
    } else if (!NumberConversions::is_finite(vector.z())) {
        message = "z not finite";
    }

    if (message != nullptr) {
        PyErr_SetString(PyExc_ValueError, message);
        return nullptr;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    serialize_doc,
    "serialize($self, /)\n"
    "--\n"
    "\n"
    "Creates a :class:`dict` representation of this class.\n"
    "\n"
    "Returns\n"
    "--------\n"
    "Dict[:class:`str`, Any]\n"
    "    The components under the keys ``x``, ``y`` and ``z``."
);

PyObject* VectorType::serialize(PyObject* self, PyObject*) noexcept {
    Object result(PyDict_New());
    if (!result) {
        return nullptr;
    }

    const Vector& vector = value(self);
    const std::array<double, 3> components = {vector.x(), vector.y(), vector.z()};
    for (std::size_t index = 0; index < components.size(); ++index) {
        Object component(PyFloat_FromDouble(components[index]));
        if (!component || PyDict_SetItemString(result.get(), NAMES[index], component.get()) < 0) {
            return nullptr;
        }
    }

    return result.release();
}

PyDoc_STRVAR(
    reduce_doc,
    "__reduce__($self, /)\n"
    "--\n"
    "\n"
    "Returns the class and the components, used by :mod:`copy` and :mod:`pickle`."
);

PyObject* VectorType::reduce(PyObject* self, PyObject*) noexcept {
    const Vector& vector = value(self);
    return Py_BuildValue("O(ddd)", Py_TYPE(self), vector.x(), vector.y(), vector.z());
}

PyDoc_STRVAR(
    get_epsilon_doc,
    "get_epsilon()\n"
    "--\n"
    "\n"
    "Get the threshold used for ``==``.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`float`\n"
    "    The epsilon."
);

PyObject* VectorType::get_epsilon(PyObject*, PyObject*) noexcept {
    return PyFloat_FromDouble(Vector::get_epsilon());
}

PyDoc_STRVAR(
    get_minimum_doc,
    "get_minimum(v1, v2, /)\n"
    "--\n"
    "\n"
    "Gets the minimum components of two vectors.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "v1: :class:`Vector`\n"
    "    The first vector.\n"
    "v2: :class:`Vector`\n"
    "    The second vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The minimum."
);

PyObject* VectorType::get_minimum(PyObject*, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("get_minimum", count, 2, 2)) {
        return nullptr;
    }

    const Vector* v1 = operand(args[0], "get_minimum");
    if (v1 == nullptr) {
        return nullptr;
    }

    const Vector* v2 = operand(args[1], "get_minimum");
    if (v2 == nullptr) {
        return nullptr;
    }

    return create_exact(Py_TYPE(args[0]), Vector::get_minimum(*v1, *v2));
}

PyDoc_STRVAR(
    get_maximum_doc,
    "get_maximum(v1, v2, /)\n"
    "--\n"
    "\n"
    "Gets the maximum components of two vectors.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "v1: :class:`Vector`\n"
    "    The first vector.\n"
    "v2: :class:`Vector`\n"
    "    The second vector.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    The maximum."
);

PyObject* VectorType::get_maximum(PyObject*, PyObject* const* args, Py_ssize_t count) noexcept {
    if (!Arguments::check_count("get_maximum", count, 2, 2)) {
        return nullptr;
    }

    const Vector* v1 = operand(args[0], "get_maximum");
    if (v1 == nullptr) {
        return nullptr;
    }

    const Vector* v2 = operand(args[1], "get_maximum");
    if (v2 == nullptr) {
        return nullptr;
    }

    return create_exact(Py_TYPE(args[0]), Vector::get_maximum(*v1, *v2));
}

PyDoc_STRVAR(
    get_random_doc,
    "get_random($type, /)\n"
    "--\n"
    "\n"
    "Gets a random vector with components having a random value between 0 and 1.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    A random vector."
);

PyObject* VectorType::get_random(PyObject* cls, PyObject*) noexcept {
    static thread_local std::mt19937_64 generator{std::random_device{}()};
    return create_exact(reinterpret_cast<PyTypeObject*>(cls), Vector::get_random(generator));
}

PyDoc_STRVAR(
    deserialize_doc,
    "deserialize($type, args, /)\n"
    "--\n"
    "\n"
    "Creates a vector from its :class:`dict` representation.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "args: Mapping[:class:`str`, Any]\n"
    "    The components under the keys ``x``, ``y`` and ``z``, missing components are 0.\n"
    "\n"
    "Returns\n"
    "--------\n"
    ":class:`Vector`\n"
    "    A new vector of the class the method was called on."
);

PyObject* VectorType::deserialize(PyObject* cls, PyObject* args) noexcept {
    std::array<double, 3> components = {0.0, 0.0, 0.0};
    for (std::size_t index = 0; index < components.size(); ++index) {
        PyObject* item = nullptr;
        const int found = PyMapping_GetOptionalItemString(args, NAMES[index], &item);
        if (found < 0) {
            return nullptr;
        }

        Object component(item);
        if (found == 1 && !Arguments::to_double(component.get(), components[index])) {
            return nullptr;
        }
    }

    return create(reinterpret_cast<PyTypeObject*>(cls), Vector(components[0], components[1], components[2]));
}

PyDoc_STRVAR(x_doc, ":class:`float`: The X component.");

PyObject* VectorType::get_x(PyObject* self, void*) noexcept {
    return PyFloat_FromDouble(value(self).x());
}

int VectorType::set_x(PyObject* self, PyObject* x, void*) noexcept {
    double component = 0.0;
    if (set(x, "x", component) < 0) {
        return -1;
    }

    value(self).set_x(component);
    return 0;
}

PyDoc_STRVAR(y_doc, ":class:`float`: The Y component.");

PyObject* VectorType::get_y(PyObject* self, void*) noexcept {
    return PyFloat_FromDouble(value(self).y());
}

int VectorType::set_y(PyObject* self, PyObject* y, void*) noexcept {
    double component = 0.0;
    if (set(y, "y", component) < 0) {
        return -1;
    }

    value(self).set_y(component);
    return 0;
}

PyDoc_STRVAR(z_doc, ":class:`float`: The Z component.");

PyObject* VectorType::get_z(PyObject* self, void*) noexcept {
    return PyFloat_FromDouble(value(self).z());
}

int VectorType::set_z(PyObject* self, PyObject* z, void*) noexcept {
    double component = 0.0;
    if (set(z, "z", component) < 0) {
        return -1;
    }

    value(self).set_z(component);
    return 0;
}

PyDoc_STRVAR(
    block_x_doc,
    ":class:`int`: The floored value of the X component, indicating the block that this vector is contained with."
);

PyObject* VectorType::get_block_x(PyObject* self, void*) noexcept {
    return block(value(self).x());
}

PyDoc_STRVAR(
    block_y_doc,
    ":class:`int`: The floored value of the Y component, indicating the block that this vector is contained with."
);

PyObject* VectorType::get_block_y(PyObject* self, void*) noexcept {
    return block(value(self).y());
}

PyDoc_STRVAR(
    block_z_doc,
    ":class:`int`: The floored value of the Z component, indicating the block that this vector is contained with."
);

PyObject* VectorType::get_block_z(PyObject* self, void*) noexcept {
    return block(value(self).z());
}

PyMethodDef VectorType::methods[] = {
    {"add", add, METH_O, add_doc},
    {"subtract", subtract, METH_O, subtract_doc},
    {"multiply", multiply, METH_O, multiply_doc},
    {"divide", divide, METH_O, divide_doc},
    {"copy", copy, METH_O, copy_doc},
    {"length", length, METH_NOARGS, length_doc},
    {"length_squared", length_squared, METH_NOARGS, length_squared_doc},
    {"distance", distance, METH_O, distance_doc},
    {"distance_squared", distance_squared, METH_O, distance_squared_doc},
    {"angle", angle, METH_O, angle_doc},
    {"midpoint", midpoint, METH_O, midpoint_doc},
    {"get_midpoint", get_midpoint, METH_O, get_midpoint_doc},
    {"dot", dot, METH_O, dot_doc},
    {"cross_product", cross_product, METH_O, cross_product_doc},
    {"get_cross_product", get_cross_product, METH_O, get_cross_product_doc},
    {"normalize", normalize, METH_NOARGS, normalize_doc},
    {"zero", zero, METH_NOARGS, zero_doc},
    {"is_zero", is_zero, METH_NOARGS, is_zero_doc},
    {"is_in_aabb", Type::function(is_in_aabb), METH_FASTCALL, is_in_aabb_doc},
    {"is_in_sphere", Type::function(is_in_sphere), METH_FASTCALL, is_in_sphere_doc},
    {"is_normalized", is_normalized, METH_NOARGS, is_normalized_doc},
    {"rotate_around_x", rotate_around_x, METH_O, rotate_around_x_doc},
    {"rotate_around_y", rotate_around_y, METH_O, rotate_around_y_doc},
    {"rotate_around_z", rotate_around_z, METH_O, rotate_around_z_doc},
    {"rotate_around_axis", Type::function(rotate_around_axis), METH_FASTCALL, rotate_around_axis_doc},
    {
        "rotate_around_non_unit_axis",
        Type::function(rotate_around_non_unit_axis),
        METH_FASTCALL,
        rotate_around_non_unit_axis_doc
    },
    {"clone", clone, METH_NOARGS, clone_doc},
    {"to_block_vector", to_block_vector, METH_NOARGS, to_block_vector_doc},
    {"check_finite", check_finite, METH_NOARGS, check_finite_doc},
    {"serialize", serialize, METH_NOARGS, serialize_doc},
    {"__reduce__", reduce, METH_NOARGS, reduce_doc},
    {"get_epsilon", get_epsilon, METH_STATIC | METH_NOARGS, get_epsilon_doc},
    {"get_minimum", Type::function(get_minimum), METH_STATIC | METH_FASTCALL, get_minimum_doc},
    {"get_maximum", Type::function(get_maximum), METH_STATIC | METH_FASTCALL, get_maximum_doc},
    {"get_random", get_random, METH_CLASS | METH_NOARGS, get_random_doc},
    {"deserialize", deserialize, METH_CLASS | METH_O, deserialize_doc},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef VectorType::getset[] = {
    {"x", get_x, set_x, x_doc, nullptr},
    {"y", get_y, set_y, y_doc, nullptr},
    {"z", get_z, set_z, z_doc, nullptr},
    {"block_x", get_block_x, nullptr, block_x_doc, nullptr},
    {"block_y", get_block_y, nullptr, block_y_doc, nullptr},
    {"block_z", get_block_z, nullptr, block_z_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot VectorType::slots[] = {
    {Py_tp_token, nullptr},
    {Py_tp_doc, const_cast<char*>(type_doc)},
    {Py_tp_dealloc, Type::slot(Type::deallocate)},
    {Py_tp_init, Type::slot(init)},
    {Py_tp_vectorcall, Type::slot(vectorcall)},
    {Py_tp_repr, Type::slot(repr)},
    {Py_tp_str, Type::slot(str)},
    {Py_tp_hash, Type::slot(hash)},
    {Py_tp_richcompare, Type::slot(richcompare)},
    {Py_tp_methods, methods},
    {Py_tp_getset, getset},
    {0, nullptr}
};

PyType_Spec VectorType::spec = {
    "minecraft.util.Vector", sizeof(VectorObject), 0, Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, slots
};

}
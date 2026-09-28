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

#include <Python.h>

#include <cstddef>

#include <minecraft/util/vector.hpp>
#include <speedups/module.hpp>

namespace minecraft::speedups::util {

/// The memory layout of the instances of minecraft.util.Vector and its subclasses.
struct VectorObject {
    PyObject_HEAD
    minecraft::util::Vector value;
};

/// Binds minecraft::util::Vector as minecraft.util.Vector.
///
/// This class only consists of static members and cannot be instantiated.
/// Every function that can fail returns false, -1 or NULL with an exception set.
class VectorType final {
public:
    VectorType() = delete;

    /// The spec the type is created from, its address is the token of the type.
    static PyType_Spec spec;

    /// Creates the type and stores it in the module and its state.
    [[nodiscard]] static bool add_to(PyObject* module, State& state) noexcept;

    /// Returns whether an object is a vector.
    [[nodiscard]] static bool check(PyObject* object) noexcept;

    /// Returns the vector of an object, which must be a vector.
    [[nodiscard]] static minecraft::util::Vector& value(PyObject* self) noexcept {
        return reinterpret_cast<VectorObject*>(self)->value;
    }

    /// Creates an instance of a type, which must be the type of vectors or a subclass of it.
    [[nodiscard]] static PyObject* create(PyTypeObject* type, const minecraft::util::Vector& vector) noexcept;

    /// Reads the components from the arguments of a vectorcall, missing components are 0.
    [[nodiscard]] static bool parse(
        const char* function, PyObject* const* args, Py_ssize_t count, PyObject* kwnames, minecraft::util::Vector& result
    ) noexcept;

    /// Reads the components from the arguments of tp_init, missing components are 0.
    [[nodiscard]] static bool parse(
        const char* function, PyObject* args, PyObject* kwargs, minecraft::util::Vector& result
    ) noexcept;

private:
    [[nodiscard]] static const minecraft::util::Vector* operand(
        PyObject* self, PyObject* object, const char* function
    ) noexcept;
    [[nodiscard]] static const minecraft::util::Vector* operand(PyObject* object, const char* function) noexcept;
    [[nodiscard]] static PyObject* create_exact(PyTypeObject* origin, const minecraft::util::Vector& vector) noexcept;
    [[nodiscard]] static PyObject* block(double component) noexcept;
    [[nodiscard]] static int set(PyObject* component, const char* name, double& result) noexcept;

    static int init(PyObject* self, PyObject* args, PyObject* kwargs) noexcept;
    static PyObject* vectorcall(PyObject* type, PyObject* const* args, std::size_t nargsf, PyObject* kwnames) noexcept;
    static PyObject* repr(PyObject* self) noexcept;
    static PyObject* str(PyObject* self) noexcept;
    static Py_hash_t hash(PyObject* self) noexcept;
    static PyObject* richcompare(PyObject* self, PyObject* other, int operation) noexcept;

    static PyObject* add(PyObject* self, PyObject* vec) noexcept;
    static PyObject* subtract(PyObject* self, PyObject* vec) noexcept;
    static PyObject* multiply(PyObject* self, PyObject* m) noexcept;
    static PyObject* divide(PyObject* self, PyObject* vec) noexcept;
    static PyObject* copy(PyObject* self, PyObject* vec) noexcept;
    static PyObject* length(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* length_squared(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* distance(PyObject* self, PyObject* o) noexcept;
    static PyObject* distance_squared(PyObject* self, PyObject* o) noexcept;
    static PyObject* angle(PyObject* self, PyObject* other) noexcept;
    static PyObject* midpoint(PyObject* self, PyObject* other) noexcept;
    static PyObject* get_midpoint(PyObject* self, PyObject* other) noexcept;
    static PyObject* dot(PyObject* self, PyObject* other) noexcept;
    static PyObject* cross_product(PyObject* self, PyObject* o) noexcept;
    static PyObject* get_cross_product(PyObject* self, PyObject* o) noexcept;
    static PyObject* normalize(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* zero(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* is_zero(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* is_in_aabb(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* is_in_sphere(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* is_normalized(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* rotate_around_x(PyObject* self, PyObject* angle) noexcept;
    static PyObject* rotate_around_y(PyObject* self, PyObject* angle) noexcept;
    static PyObject* rotate_around_z(PyObject* self, PyObject* angle) noexcept;
    static PyObject* rotate_around_axis(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* rotate_around_non_unit_axis(PyObject* self, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* clone(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* to_block_vector(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* check_finite(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* serialize(PyObject* self, PyObject* ignored) noexcept;
    static PyObject* reduce(PyObject* self, PyObject* ignored) noexcept;

    static PyObject* get_epsilon(PyObject* ignored, PyObject* unused) noexcept;
    static PyObject* get_minimum(PyObject* ignored, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* get_maximum(PyObject* ignored, PyObject* const* args, Py_ssize_t count) noexcept;
    static PyObject* get_random(PyObject* cls, PyObject* ignored) noexcept;
    static PyObject* deserialize(PyObject* cls, PyObject* args) noexcept;

    static PyObject* get_x(PyObject* self, void* closure) noexcept;
    static PyObject* get_y(PyObject* self, void* closure) noexcept;
    static PyObject* get_z(PyObject* self, void* closure) noexcept;
    static int set_x(PyObject* self, PyObject* x, void* closure) noexcept;
    static int set_y(PyObject* self, PyObject* y, void* closure) noexcept;
    static int set_z(PyObject* self, PyObject* z, void* closure) noexcept;
    static PyObject* get_block_x(PyObject* self, void* closure) noexcept;
    static PyObject* get_block_y(PyObject* self, void* closure) noexcept;
    static PyObject* get_block_z(PyObject* self, void* closure) noexcept;

    static PyMethodDef methods[];
    static PyGetSetDef getset[];
    static PyType_Slot slots[];
};

}
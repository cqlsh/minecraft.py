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

#include <speedups/module.hpp>

namespace minecraft::speedups::util {

/// Binds minecraft::util::BlockVector as minecraft.util.BlockVector, a subclass of minecraft.util.Vector.
///
/// The instances have the memory layout of VectorObject. This class only
/// consists of static members and cannot be instantiated. Every function that
/// can fail returns false, -1 or NULL with an exception set.
class BlockVectorType final {
public:
    BlockVectorType() = delete;

    /// The spec the type is created from, its address is the token of the type.
    static PyType_Spec spec;

    /// Creates the type and stores it in the module and its state, the type of vectors must have been added before.
    [[nodiscard]] static bool add_to(PyObject* module, State& state) noexcept;

    /// Returns whether an object is a block vector.
    [[nodiscard]] static bool check(PyObject* object) noexcept;

private:
    static int init(PyObject* self, PyObject* args, PyObject* kwargs) noexcept;
    static PyObject* vectorcall(PyObject* type, PyObject* const* args, std::size_t nargsf, PyObject* kwnames) noexcept;
    static Py_hash_t hash(PyObject* self) noexcept;
    static PyObject* richcompare(PyObject* self, PyObject* other, int operation) noexcept;

    static PyType_Slot slots[];
};

}
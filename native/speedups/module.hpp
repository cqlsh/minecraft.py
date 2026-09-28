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

namespace minecraft::speedups {

/// The state of the extension module, every interpreter has its own.
///
/// The types are strong references that are owned by the state.
struct State {
    PyTypeObject* vector_type = nullptr;
    PyTypeObject* block_vector_type = nullptr;
    PyTypeObject* bounding_box_type = nullptr;
};

/// Gives access to the extension module minecraft._speedups and its state.
///
/// This class only consists of static members and cannot be instantiated.
class Module final {
public:
    Module() = delete;

    /// The definition of the extension module.
    static PyModuleDef definition;

    /// Returns the state of the module.
    [[nodiscard]] static State& state(PyObject* module) noexcept {
        return *static_cast<State*>(PyModule_GetState(module));
    }

    /// Returns the state of the module that defined a type or one of its bases.
    ///
    /// Returns NULL with an exception set if the module defined none of them.
    [[nodiscard]] static State* state_of(PyTypeObject* type) noexcept {
        PyObject* module = PyType_GetModuleByDef(type, &definition);
        if (module == nullptr) {
            return nullptr;
        }

        return &state(module);
    }

private:
    static int exec(PyObject* module) noexcept;
    static int traverse(PyObject* module, visitproc visit, void* arg) noexcept;
    static int clear(PyObject* module) noexcept;
    static void free(void* module) noexcept;

    static PyModuleDef_Slot slots[];
};

}
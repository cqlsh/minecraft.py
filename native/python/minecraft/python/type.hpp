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

#include <minecraft/python/object.hpp>

namespace minecraft::python {

/// Creates heap types and inspects their instances.
///
/// This class only consists of static functions and cannot be instantiated.
class Type final {
public:
    Type() = delete;

    /// Creates a heap type from a spec and adds it to a module under the last component of its name.
    ///
    /// Returns a new reference, or NULL with an exception set.
    [[nodiscard]] static PyTypeObject* create(PyObject* module, PyType_Spec& spec, PyTypeObject* base = nullptr) noexcept {
        Object type(PyType_FromModuleAndSpec(module, &spec, reinterpret_cast<PyObject*>(base)));
        if (!type) {
            return nullptr;
        }
        if (PyModule_AddType(module, reinterpret_cast<PyTypeObject*>(type.get())) < 0) {
            return nullptr;
        }

        return reinterpret_cast<PyTypeObject*>(type.release());
    }

    /// Returns whether an object is an instance of the type that was created from a spec, or of a subclass of it.
    ///
    /// The spec must contain the slot Py_tp_token with the value NULL.
    [[nodiscard]] static bool is_instance(PyObject* object, PyType_Spec& spec) noexcept {
        return PyType_GetBaseByToken(Py_TYPE(object), &spec, nullptr) == 1;
    }

    /// Releases an instance of a heap type that holds no references to other objects, to be used as tp_dealloc.
    static void deallocate(PyObject* self) noexcept {
        PyTypeObject* type = Py_TYPE(self);
        type->tp_free(self);
        Py_DECREF(type);
    }

    /// Converts a function of any calling convention to the type that is stored in method tables.
    template <typename Function>
    [[nodiscard]] static PyCFunction function(Function* function) noexcept {
        return reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(function));
    }

    /// Converts a function to the type that is stored in slot tables.
    template <typename Function>
    [[nodiscard]] static void* slot(Function* function) noexcept {
        return reinterpret_cast<void*>(function);
    }
};

}
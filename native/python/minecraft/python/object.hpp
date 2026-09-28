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

#include <utility>

namespace minecraft::python {

/// Owns a strong reference to a Python object and releases it on destruction.
///
/// An empty object holds no reference. This is also how the result of a call
/// that failed with an exception set is represented.
class Object final {
public:
    /// Constructs an empty object.
    constexpr Object() noexcept = default;

    /// Takes ownership of a new reference, which may be NULL.
    explicit Object(PyObject* reference) noexcept : reference_(reference) {
    }

    Object(const Object&) = delete;

    /// Takes over the reference of another object and leaves it empty.
    Object(Object&& other) noexcept : reference_(std::exchange(other.reference_, nullptr)) {
    }

    ~Object() {
        Py_XDECREF(reference_);
    }

    Object& operator=(const Object&) = delete;

    /// Releases the held reference and takes over the one of another object.
    Object& operator=(Object&& other) noexcept {
        if (this != &other) {
            Py_XSETREF(reference_, std::exchange(other.reference_, nullptr));
        }

        return *this;
    }

    /// Returns whether a reference is held.
    explicit operator bool() const noexcept {
        return reference_ != nullptr;
    }

    /// Creates an object that owns a new reference to a borrowed one.
    [[nodiscard]] static Object borrow(PyObject* reference) noexcept {
        return Object(Py_XNewRef(reference));
    }

    /// Returns the reference without giving up the ownership.
    [[nodiscard]] PyObject* get() const noexcept {
        return reference_;
    }

    /// Gives up the ownership and returns the reference, for example to return it to Python.
    [[nodiscard]] PyObject* release() noexcept {
        return std::exchange(reference_, nullptr);
    }

private:
    PyObject* reference_ = nullptr;
};

}
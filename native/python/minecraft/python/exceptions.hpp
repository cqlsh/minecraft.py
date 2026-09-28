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

#include <exception>
#include <new>
#include <stdexcept>

namespace minecraft::python {

/// Converts C++ exceptions to Python exceptions.
///
/// This class only consists of static functions and cannot be instantiated.
class Exceptions final {
public:
    Exceptions() = delete;

    /// Calls a function and sets the Python exception that corresponds to the C++ exception it throws.
    ///
    /// Returns false with an exception set if the function threw.
    /// std::invalid_argument becomes ValueError, which is what
    /// IllegalArgumentException of Java is mapped to, std::bad_alloc becomes
    /// MemoryError and everything else RuntimeError.
    template <typename Function>
    [[nodiscard]] static bool guard(Function function) noexcept {
        try {
            function();
            return true;
        } catch (const std::invalid_argument& error) {
            PyErr_SetString(PyExc_ValueError, error.what());
        } catch (const std::bad_alloc&) {
            PyErr_NoMemory();
        } catch (const std::exception& error) {
            PyErr_SetString(PyExc_RuntimeError, error.what());
        } catch (...) {
            PyErr_SetString(PyExc_RuntimeError, "unknown C++ exception");
        }

        return false;
    }
};

}
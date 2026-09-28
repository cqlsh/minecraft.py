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
#include <span>

namespace minecraft::python {

/// Validates and converts the arguments of functions that are called from Python.
///
/// This class only consists of static functions and cannot be instantiated.
/// Every function returns false with an exception set on failure.
class Arguments final {
public:
    Arguments() = delete;

    /// Ensures that a function received an accepted number of positional arguments.
    [[nodiscard]] static bool check_count(
        const char* function, Py_ssize_t count, Py_ssize_t minimum, Py_ssize_t maximum
    ) noexcept {
        if (count >= minimum && maximum >= count) {
            return true;
        }

        const char* quantifier = minimum == maximum ? "exactly" : minimum > count ? "at least" : "at most";
        const Py_ssize_t expected = minimum > count ? minimum : maximum;
        PyErr_Format(
            PyExc_TypeError,
            "%s() takes %s %zd positional argument%s (%zd given)",
            function,
            quantifier,
            expected,
            expected == 1 ? "" : "s",
            count
        );
        return false;
    }

    /// Converts a Python number to a double.
    [[nodiscard]] static bool to_double(PyObject* object, double& result) noexcept {
        if (PyFloat_CheckExact(object)) {
            result = PyFloat_AS_DOUBLE(object);
            return true;
        }

        result = PyFloat_AsDouble(object);
        return result != -1.0 || PyErr_Occurred() == nullptr;
    }

    /// Converts the first Python numbers of an array to doubles, one for every element of the result.
    [[nodiscard]] static bool to_doubles(PyObject* const* args, std::span<double> result) noexcept {
        for (std::size_t index = 0; index < result.size(); ++index) {
            if (!to_double(args[index], result[index])) {
                return false;
            }
        }

        return true;
    }

    /// Reads numbers from the arguments of a vectorcall, which can be passed by position or by name.
    ///
    /// The result has an element for every name, the elements of numbers that
    /// were not passed are left unchanged.
    [[nodiscard]] static bool to_doubles(
        const char* function,
        PyObject* const* args,
        Py_ssize_t count,
        PyObject* kwnames,
        std::span<const char* const> names,
        std::span<double> result
    ) noexcept {
        const Py_ssize_t size = static_cast<Py_ssize_t>(names.size());
        if (!check_count(function, count, 0, size)) {
            return false;
        }

        for (Py_ssize_t index = 0; index < count; ++index) {
            if (!to_double(args[index], result[static_cast<std::size_t>(index)])) {
                return false;
            }
        }

        const Py_ssize_t keywords = kwnames == nullptr ? 0 : PyTuple_GET_SIZE(kwnames);
        for (Py_ssize_t keyword = 0; keyword < keywords; ++keyword) {
            PyObject* name = PyTuple_GET_ITEM(kwnames, keyword);
            const Py_ssize_t index = find(name, names);
            if (index == size) {
                PyErr_Format(PyExc_TypeError, "%s() got an unexpected keyword argument '%U'", function, name);
                return false;
            }
            if (index < count) {
                PyErr_Format(PyExc_TypeError, "%s() got multiple values for argument '%U'", function, name);
                return false;
            }
            if (!to_double(args[count + keyword], result[static_cast<std::size_t>(index)])) {
                return false;
            }
        }

        return true;
    }

    /// Reads numbers from the arguments of tp_init, which can be passed by position or by name.
    ///
    /// The result has an element for every name, the elements of numbers that
    /// were not passed are left unchanged.
    [[nodiscard]] static bool to_doubles(
        const char* function,
        PyObject* args,
        PyObject* kwargs,
        std::span<const char* const> names,
        std::span<double> result
    ) noexcept {
        const Py_ssize_t size = static_cast<Py_ssize_t>(names.size());
        const Py_ssize_t count = PyTuple_GET_SIZE(args);
        if (!check_count(function, count, 0, size)) {
            return false;
        }

        for (Py_ssize_t index = 0; index < count; ++index) {
            if (!to_double(PyTuple_GET_ITEM(args, index), result[static_cast<std::size_t>(index)])) {
                return false;
            }
        }

        if (kwargs == nullptr) {
            return true;
        }

        PyObject* name = nullptr;
        PyObject* number = nullptr;
        Py_ssize_t position = 0;
        while (PyDict_Next(kwargs, &position, &name, &number)) {
            if (!PyUnicode_Check(name)) {
                PyErr_SetString(PyExc_TypeError, "keywords must be strings");
                return false;
            }

            const Py_ssize_t index = find(name, names);
            if (index == size) {
                PyErr_Format(PyExc_TypeError, "%s() got an unexpected keyword argument '%U'", function, name);
                return false;
            }
            if (index < count) {
                PyErr_Format(PyExc_TypeError, "%s() got multiple values for argument '%U'", function, name);
                return false;
            }
            if (!to_double(number, result[static_cast<std::size_t>(index)])) {
                return false;
            }
        }

        return true;
    }

private:
    [[nodiscard]] static Py_ssize_t find(PyObject* name, std::span<const char* const> names) noexcept {
        const Py_ssize_t size = static_cast<Py_ssize_t>(names.size());
        if (!PyUnicode_IS_ASCII(name)) {
            return size;
        }

        const Py_UCS1* text = PyUnicode_1BYTE_DATA(name);
        const Py_ssize_t length = PyUnicode_GET_LENGTH(name);
        Py_ssize_t index = 0;
        while (index < size && !equals(names[static_cast<std::size_t>(index)], text, length)) {
            ++index;
        }

        return index;
    }

    [[nodiscard]] static bool equals(const char* name, const Py_UCS1* text, Py_ssize_t length) noexcept {
        for (Py_ssize_t index = 0; index < length; ++index) {
            if (name[index] == '\0' || static_cast<Py_UCS1>(name[index]) != text[index]) {
                return false;
            }
        }

        return name[length] == '\0';
    }
};

}
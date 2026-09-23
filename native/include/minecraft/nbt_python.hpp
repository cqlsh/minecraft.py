/*
The MIT License (MIT)

Copyright (c) 2026-present cqlsh

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <minecraft/nbt.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace minecraft {

PyObject *nbt_load(PyObject *nbt_module, nbt::Format format, const uint8_t *begin, const uint8_t *end, bool named, PyObject **name, size_t &consumed);

bool nbt_dump(PyObject *nbt_module, nbt::Format format, PyObject *value, PyObject *name, std::vector<uint8_t> &out);

extern PyModuleDef nbt_definition;

}
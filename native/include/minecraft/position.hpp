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
#include <cstdint>

namespace minecraft {

struct BlockPosition
{
    PyObject_HEAD
    int x;
    int y;
    int z;
    PyObject *boxed_x;
    PyObject *boxed_y;
    PyObject *boxed_z;
};

inline int64_t pack_position(int x, int y, int z)
{
    uint64_t packed = static_cast<uint64_t>(static_cast<uint32_t>(x) & 0x3FFFFFF) << 38;
    packed |= static_cast<uint64_t>(static_cast<uint32_t>(z) & 0x3FFFFFF) << 12;
    packed |= static_cast<uint64_t>(static_cast<uint32_t>(y) & 0xFFF);

    return static_cast<int64_t>(packed);
}

inline void unpack_position(int64_t packed, int &x, int &y, int &z)
{
    uint64_t bits = static_cast<uint64_t>(packed);
    x = static_cast<int>(static_cast<int64_t>(bits) >> 38);
    y = static_cast<int>(static_cast<int64_t>(bits << 52) >> 52);
    z = static_cast<int>(static_cast<int64_t>(bits << 26) >> 38);
}

inline PyObject *make_position(PyTypeObject *type, int x, int y, int z, PyObject *boxed_x, PyObject *boxed_y, PyObject *boxed_z)
{
    BlockPosition *self = reinterpret_cast<BlockPosition *>(type->tp_alloc(type, 0));
    if (self == nullptr) {
        return nullptr;
    }

    self->x = x;
    self->y = y;
    self->z = z;
    self->boxed_x = Py_NewRef(boxed_x);
    self->boxed_y = Py_NewRef(boxed_y);
    self->boxed_z = Py_NewRef(boxed_z);

    return reinterpret_cast<PyObject *>(self);
}

inline PyObject *make_position(PyTypeObject *type, int x, int y, int z)
{
    PyObject *boxed_x = PyLong_FromLong(x);
    PyObject *boxed_y = PyLong_FromLong(y);
    PyObject *boxed_z = PyLong_FromLong(z);
    PyObject *result = nullptr;
    if (boxed_x != nullptr && boxed_y != nullptr && boxed_z != nullptr) {
        result = make_position(type, x, y, z, boxed_x, boxed_y, boxed_z);
    }

    Py_XDECREF(boxed_x);
    Py_XDECREF(boxed_y);
    Py_XDECREF(boxed_z);

    return result;
}

extern PyModuleDef position_definition;

}
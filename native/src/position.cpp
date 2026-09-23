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

#include <minecraft/position.hpp>

namespace {

using minecraft::BlockPosition;

PyDoc_STRVAR(type_doc,
"BlockPosition(x, y, z, /)\n"
"--\n"
"\n"
"An immutable position of a block in a world, made of three integer\n"
"coordinates.\n"
"\n"
"Two positions compare equal when all three coordinates match and they\n"
"hash the same, so they work as dictionary keys and in sets. Unpacking\n"
"``x, y, z = position`` works as well.\n"
"\n"
"Attributes\n"
"-----------\n"
"x: :class:`int`\n"
"    The east-west coordinate.\n"
"y: :class:`int`\n"
"    The height. The world decides which values lie inside its build\n"
"    limits.\n"
"z: :class:`int`\n"
"    The north-south coordinate.");

PyDoc_STRVAR(offset_doc,
"offset(x, y, z, /)\n"
"--\n"
"\n"
"Returns a new position moved by the given amounts.\n"
"\n"
"The position itself never changes. Every method that looks like a move\n"
"hands back a new object.");

PyDoc_STRVAR(unpack_doc,
"unpack(value, /)\n"
"--\n"
"\n"
"Builds a position from the packed 64-bit form the protocol uses.\n"
"\n"
"Only values that came out of :attr:`packed` round-trip. Anything else is\n"
"read as 26 bits of x, 12 bits of y and 26 bits of z without complaint.");

PyDoc_STRVAR(packed_doc,
":class:`int`: The position as the signed 64-bit long the protocol sends.\n"
"\n"
"x and z take 26 bits each and y takes 12, so coordinates beyond\n"
"33554432 blocks or 2048 in height wrap around instead of failing.");

PyDoc_STRVAR(x_doc, "The east-west coordinate.");
PyDoc_STRVAR(y_doc, "The height.");
PyDoc_STRVAR(z_doc, "The north-south coordinate.");

bool read_int(PyObject *object, int &out)
{
    int value = PyLong_AsInt(object);
    if (value == -1 && PyErr_Occurred()) {
        return false;
    }

    out = value;

    return true;
}

bool read_ints(PyObject *const *args, int &x, int &y, int &z)
{
    return read_int(args[0], x) && read_int(args[1], y) && read_int(args[2], z);
}

PyObject *build(PyTypeObject *type, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_Format(PyExc_TypeError, "BlockPosition() takes exactly 3 positional arguments (%zd given)", nargs);
        return nullptr;
    }

    int x, y, z;
    if (!read_ints(args, x, y, z)) {
        return nullptr;
    }

    return minecraft::make_position(type, x, y, z, args[0], args[1], args[2]);
}

PyObject *vectorcall(PyObject *callable, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    if (kwnames != nullptr && PyTuple_GET_SIZE(kwnames) != 0) {
        PyErr_SetString(PyExc_TypeError, "BlockPosition() takes no keyword arguments");
        return nullptr;
    }

    return build(reinterpret_cast<PyTypeObject *>(callable), args, PyVectorcall_NARGS(nargsf));
}

PyObject *tp_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    if (kwargs != nullptr && PyDict_GET_SIZE(kwargs) != 0) {
        PyErr_SetString(PyExc_TypeError, "BlockPosition() takes no keyword arguments");
        return nullptr;
    }

    return build(type, &PyTuple_GET_ITEM(args, 0), PyTuple_GET_SIZE(args));
}

void dealloc(PyObject *object)
{
    BlockPosition *self = reinterpret_cast<BlockPosition *>(object);
    PyTypeObject *type = Py_TYPE(object);
    Py_DECREF(self->boxed_x);
    Py_DECREF(self->boxed_y);
    Py_DECREF(self->boxed_z);
    type->tp_free(object);
    Py_DECREF(type);
}

Py_hash_t hash(PyObject *object)
{
    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);
    uint64_t mixed = static_cast<uint64_t>(static_cast<uint32_t>(self->x)) * 0x9E3779B97F4A7C15ull;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(self->y)) * 0xC2B2AE3D27D4EB4Full;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(self->z)) * 0x165667B19E3779F9ull;
    mixed ^= mixed >> 29;

    Py_hash_t result = static_cast<Py_hash_t>(mixed);

    return result == -1 ? -2 : result;
}

PyObject *richcompare(PyObject *object, PyObject *other, int op)
{
    if ((op != Py_EQ && op != Py_NE) || !PyObject_TypeCheck(other, Py_TYPE(object))) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);
    const BlockPosition *that = reinterpret_cast<BlockPosition *>(other);
    bool equal = self->x == that->x && self->y == that->y && self->z == that->z;

    return PyBool_FromLong(equal == (op == Py_EQ));
}

PyObject *repr(PyObject *object)
{
    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);

    return PyUnicode_FromFormat("BlockPosition(x=%d, y=%d, z=%d)", self->x, self->y, self->z);
}

PyObject *iter(PyObject *object)
{
    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);
    PyObject *components = PyTuple_Pack(3, self->boxed_x, self->boxed_y, self->boxed_z);
    if (components == nullptr) {
        return nullptr;
    }

    PyObject *iterator = PyObject_GetIter(components);
    Py_DECREF(components);

    return iterator;
}

PyObject *offset(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_Format(PyExc_TypeError, "offset() takes exactly 3 positional arguments (%zd given)", nargs);
        return nullptr;
    }

    int x, y, z;
    if (!read_ints(args, x, y, z)) {
        return nullptr;
    }

    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);

    return minecraft::make_position(Py_TYPE(object), self->x + x, self->y + y, self->z + z);
}

PyObject *unpack(PyObject *type, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 1) {
        PyErr_Format(PyExc_TypeError, "unpack() takes exactly 1 positional argument (%zd given)", nargs);
        return nullptr;
    }

    long long value = PyLong_AsLongLong(args[0]);
    if (value == -1 && PyErr_Occurred()) {
        return nullptr;
    }

    int x, y, z;
    minecraft::unpack_position(value, x, y, z);

    return minecraft::make_position(reinterpret_cast<PyTypeObject *>(type), x, y, z);
}

PyObject *reduce(PyObject *object, PyObject *Py_UNUSED(ignored))
{
    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);

    return Py_BuildValue("(O(OOO))", Py_TYPE(object), self->boxed_x, self->boxed_y, self->boxed_z);
}

PyObject *packed(PyObject *object, void *Py_UNUSED(closure))
{
    const BlockPosition *self = reinterpret_cast<BlockPosition *>(object);

    return PyLong_FromLongLong(minecraft::pack_position(self->x, self->y, self->z));
}

PyMemberDef members[] = {
    {"x", Py_T_OBJECT_EX, offsetof(BlockPosition, boxed_x), Py_READONLY, x_doc},
    {"y", Py_T_OBJECT_EX, offsetof(BlockPosition, boxed_y), Py_READONLY, y_doc},
    {"z", Py_T_OBJECT_EX, offsetof(BlockPosition, boxed_z), Py_READONLY, z_doc},
    {nullptr, 0, 0, 0, nullptr}
};

PyMethodDef methods[] = {
    {"offset", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(offset)), METH_FASTCALL, offset_doc},
    {"unpack", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(unpack)), METH_FASTCALL | METH_CLASS, unpack_doc},
    {"__reduce__", reduce, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef getset[] = {
    {"packed", packed, nullptr, packed_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot type_slots[] = {
    {Py_tp_doc, const_cast<char *>(type_doc)},
    {Py_tp_new, reinterpret_cast<void *>(tp_new)},
    {Py_tp_vectorcall, reinterpret_cast<void *>(vectorcall)},
    {Py_tp_dealloc, reinterpret_cast<void *>(dealloc)},
    {Py_tp_hash, reinterpret_cast<void *>(hash)},
    {Py_tp_richcompare, reinterpret_cast<void *>(richcompare)},
    {Py_tp_repr, reinterpret_cast<void *>(repr)},
    {Py_tp_iter, reinterpret_cast<void *>(iter)},
    {Py_tp_members, members},
    {Py_tp_methods, methods},
    {Py_tp_getset, getset},
    {0, nullptr}
};

PyType_Spec type_spec = {
    "minecraft.world.position.BlockPosition",
    sizeof(BlockPosition),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    type_slots
};

int exec(PyObject *module)
{
    PyObject *type = PyType_FromModuleAndSpec(module, &type_spec, nullptr);
    if (type == nullptr) {
        return -1;
    }

    int status = PyModule_AddObjectRef(module, "BlockPosition", type);
    Py_DECREF(type);

    return status;
}

PyDoc_STRVAR(module_doc, "The block position type behind minecraft.world.position.");

PyModuleDef_Slot slots[] = {
    {Py_mod_exec, reinterpret_cast<void *>(exec)},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, nullptr}
};

}

namespace minecraft {

PyModuleDef position_definition = {
    PyModuleDef_HEAD_INIT,
    "minecraft._native.position",
    module_doc,
    0,
    nullptr,
    slots,
    nullptr,
    nullptr,
    nullptr
};

}
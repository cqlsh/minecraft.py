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

#include <speedups/util/block_vector.hpp>

#include <minecraft/python/type.hpp>
#include <minecraft/util/block_vector.hpp>
#include <speedups/util/vector.hpp>

namespace minecraft::speedups::util {

using minecraft::python::Type;
using minecraft::util::BlockVector;
using minecraft::util::Vector;

bool BlockVectorType::add_to(PyObject* module, State& state) noexcept {
    state.block_vector_type = Type::create(module, spec, state.vector_type);
    return state.block_vector_type != nullptr;
}

bool BlockVectorType::check(PyObject* object) noexcept {
    return Type::is_instance(object, spec);
}

PyDoc_STRVAR(
    type_doc,
    "BlockVector(x=0.0, y=0.0, z=0.0)\n"
    "--\n"
    "\n"
    "A vector with a hash function that truncates the X, Y, Z components, a la\n"
    "BlockVector in WorldEdit.\n"
    "\n"
    "BlockVectors can be used in sets and as keys of dictionaries. Be aware that\n"
    "BlockVectors are mutable, but it is important that BlockVectors are never\n"
    "changed once put into a set or a dictionary.\n"
    "\n"
    "Instead of the components a single :class:`Vector` may be passed, whose\n"
    "components are copied.\n"
    "\n"
    ".. container:: operations\n"
    "\n"
    "    .. describe:: x == y\n"
    "\n"
    "        Checks if another block vector is equivalent. Two block vectors are\n"
    "        equivalent if their components are equal after they have been\n"
    "        truncated towards zero.\n"
    "\n"
    "    .. describe:: x != y\n"
    "\n"
    "        Checks if another block vector is not equivalent.\n"
    "\n"
    "    .. describe:: hash(x)\n"
    "\n"
    "        Returns the hash code of the block vector, identical to the one of Java.\n"
    "\n"
    "Parameters\n"
    "-----------\n"
    "x: :class:`float`\n"
    "    The X component.\n"
    "y: :class:`float`\n"
    "    The Y component.\n"
    "z: :class:`float`\n"
    "    The Z component."
);

int BlockVectorType::init(PyObject* self, PyObject* args, PyObject* kwargs) noexcept {
    const bool keywords = kwargs != nullptr && PyDict_GET_SIZE(kwargs) != 0;
    if (PyTuple_GET_SIZE(args) == 1 && !keywords && VectorType::check(PyTuple_GET_ITEM(args, 0))) {
        VectorType::value(self) = VectorType::value(PyTuple_GET_ITEM(args, 0));
        return 0;
    }

    return VectorType::parse("BlockVector", args, kwargs, VectorType::value(self)) ? 0 : -1;
}

PyObject* BlockVectorType::vectorcall(
    PyObject* type, PyObject* const* args, std::size_t nargsf, PyObject* kwnames
) noexcept {
    const Py_ssize_t count = PyVectorcall_NARGS(nargsf);
    const bool keywords = kwnames != nullptr && PyTuple_GET_SIZE(kwnames) != 0;
    if (count == 1 && !keywords && VectorType::check(args[0])) {
        return VectorType::create(reinterpret_cast<PyTypeObject*>(type), VectorType::value(args[0]));
    }

    Vector vector;
    if (!VectorType::parse("BlockVector", args, count, kwnames, vector)) {
        return nullptr;
    }

    return VectorType::create(reinterpret_cast<PyTypeObject*>(type), vector);
}

Py_hash_t BlockVectorType::hash(PyObject* self) noexcept {
    const Py_hash_t code = BlockVector(VectorType::value(self)).hash_code();
    return code == -1 ? -2 : code;
}

PyObject* BlockVectorType::richcompare(PyObject* self, PyObject* other, int operation) noexcept {
    if ((operation != Py_EQ && operation != Py_NE) || !VectorType::check(other)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    const bool equal = check(other) && BlockVector(VectorType::value(self)).equals(BlockVector(VectorType::value(other)));
    return PyBool_FromLong(equal == (operation == Py_EQ));
}

PyType_Slot BlockVectorType::slots[] = {
    {Py_tp_token, nullptr},
    {Py_tp_doc, const_cast<char*>(type_doc)},
    {Py_tp_init, Type::slot(init)},
    {Py_tp_vectorcall, Type::slot(vectorcall)},
    {Py_tp_hash, Type::slot(hash)},
    {Py_tp_richcompare, Type::slot(richcompare)},
    {0, nullptr}
};

PyType_Spec BlockVectorType::spec = {
    "minecraft.util.BlockVector", sizeof(VectorObject), 0, Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, slots
};

}
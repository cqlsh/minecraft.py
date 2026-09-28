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

#include <speedups/module.hpp>

#include <new>

#include <minecraft/python/type.hpp>
#include <speedups/util/block_vector.hpp>
#include <speedups/util/vector.hpp>

namespace minecraft::speedups {

using minecraft::python::Type;

PyDoc_STRVAR(module_doc, "The parts of minecraft.py that are implemented in C++ for performance.");

int Module::exec(PyObject* module) noexcept {
    State& created = *new (PyModule_GetState(module)) State();
    if (!util::VectorType::add_to(module, created)) {
        return -1;
    }
    if (!util::BlockVectorType::add_to(module, created)) {
        return -1;
    }

    return 0;
}

int Module::traverse(PyObject* module, visitproc visit, void* arg) noexcept {
    const State& current = state(module);
    Py_VISIT(current.vector_type);
    Py_VISIT(current.block_vector_type);
    return 0;
}

int Module::clear(PyObject* module) noexcept {
    State& current = state(module);
    Py_CLEAR(current.vector_type);
    Py_CLEAR(current.block_vector_type);
    return 0;
}

void Module::free(void* module) noexcept {
    clear(static_cast<PyObject*>(module));
}

PyModuleDef_Slot Module::slots[] = {
    {Py_mod_exec, Type::slot(exec)},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, nullptr}
};

PyModuleDef Module::definition = {
    PyModuleDef_HEAD_INIT, "minecraft._speedups", module_doc, sizeof(State), nullptr, slots, traverse, clear, free
};

}

PyMODINIT_FUNC PyInit__speedups() {
    return PyModuleDef_Init(&minecraft::speedups::Module::definition);
}
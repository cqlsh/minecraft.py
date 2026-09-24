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

#include <minecraft/nbt_python.hpp>
#include <minecraft/position.hpp>

namespace minecraft {

extern PyModuleDef buffer_definition;

}

namespace {

PyDoc_STRVAR(module_doc,
"The C++ core behind minecraft, with one submodule per accelerated Python\n"
"module. Plugins never import it; the Python modules on top of it are the\n"
"supported API.");

const char *const package = "minecraft._native";

struct Submodule
{
    const char *name;
    PyModuleDef *definition;
};

const Submodule submodules[] = {
    {"buffer", &minecraft::buffer_definition},
    {"nbt", &minecraft::nbt_definition},
    {"position", &minecraft::position_definition},
    {nullptr, nullptr}
};

PyObject *create_submodule(PyObject *machinery, PyObject *name, PyModuleDef *definition)
{
    PyObject *spec = PyObject_CallMethod(machinery, "ModuleSpec", "OO", name, Py_None);
    if (spec == nullptr) {
        return nullptr;
    }

    PyObject *module = PyModule_FromDefAndSpec(definition, spec);
    if (module == nullptr) {
        Py_DECREF(spec);
        return nullptr;
    }

    int described = PyObject_SetAttrString(module, "__spec__", spec);
    Py_DECREF(spec);
    if (described < 0) {
        Py_DECREF(module);
        return nullptr;
    }

    if (PyModule_ExecDef(module, definition) < 0) {
        Py_DECREF(module);
        return nullptr;
    }

    return module;
}

int register_submodule(PyObject *parent, PyObject *name, PyObject *module, const char *attribute)
{
    if (PyDict_SetItem(PyImport_GetModuleDict(), name, module) < 0) {
        return -1;
    }

    return PyModule_AddObjectRef(parent, attribute, module);
}

int add_submodule(PyObject *parent, PyObject *machinery, const Submodule &entry)
{
    PyObject *name = PyUnicode_FromFormat("%s.%s", package, entry.name);
    if (name == nullptr) {
        return -1;
    }

    PyObject *module = create_submodule(machinery, name, entry.definition);
    if (module == nullptr) {
        Py_DECREF(name);
        return -1;
    }

    int status = register_submodule(parent, name, module, entry.name);
    Py_DECREF(module);
    Py_DECREF(name);

    return status;
}

int exec(PyObject *module)
{
    PyObject *machinery = PyImport_ImportModule("importlib.machinery");
    if (machinery == nullptr) {
        return -1;
    }

    for (const Submodule *entry = submodules; entry->name != nullptr; entry++) {
        if (add_submodule(module, machinery, *entry) < 0) {
            Py_DECREF(machinery);
            return -1;
        }
    }

    Py_DECREF(machinery);

    return 0;
}

PyModuleDef_Slot slots[] = {
    {Py_mod_exec, reinterpret_cast<void *>(exec)},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, nullptr}
};

PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    package,
    module_doc,
    0,
    nullptr,
    slots,
    nullptr,
    nullptr,
    nullptr
};

}

PyMODINIT_FUNC PyInit__native(void)
{
    return PyModuleDef_Init(&module);
}
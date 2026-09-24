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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <minecraft/aes.hpp>
#include <new>

namespace {

PyDoc_STRVAR(module_doc, "The connection cipher behind minecraft.net.cipher.");

PyDoc_STRVAR(cipher_doc,
"Cfb8Cipher(secret, /)\n"
"--\n"
"\n"
"Encrypts and decrypts a Java Edition connection.\n"
"\n"
"Java uses AES in CFB8 mode with the shared secret from the login\n"
"handshake as both key and IV. Each direction keeps its own stream\n"
"state, so hand every outgoing byte to :meth:`encrypt` and every\n"
"incoming byte to :meth:`decrypt`, once and in wire order. The key\n"
"material is wiped when the cipher goes away.\n"
"\n"
"Parameters\n"
"-----------\n"
"secret: :class:`bytes`\n"
"    The shared secret, the 16 bytes of the AES-128 key Java negotiates.\n"
"\n"
"Raises\n"
"-------\n"
"ValueError\n"
"    The secret is not 16 bytes long.\n"
"\n"
"Attributes\n"
"-----------\n"
"hardware: :class:`bool`\n"
"    Whether the CPU's AES instructions are in use rather than the\n"
"    software fallback.");

PyDoc_STRVAR(encrypt_doc,
"encrypt(data, /)\n"
"--\n"
"\n"
"Encrypts the next outgoing bytes and returns them.");

PyDoc_STRVAR(decrypt_doc,
"decrypt(data, /)\n"
"--\n"
"\n"
"Decrypts the next incoming bytes and returns them.");

struct CipherObject
{
    PyObject_HEAD
    minecraft::Cfb8 outgoing;
    minecraft::Cfb8 incoming;
    bool hardware;
};

void wipe(void *memory, size_t size)
{
    volatile uint8_t *bytes = static_cast<volatile uint8_t *>(memory);
    for (size_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

PyObject *build(PyTypeObject *type, PyObject *secret)
{
    Py_buffer view;
    if (PyObject_GetBuffer(secret, &view, PyBUF_SIMPLE) < 0) {
        return nullptr;
    }
    if (view.len != static_cast<Py_ssize_t>(minecraft::aes_block_size)) {
        PyBuffer_Release(&view);
        PyErr_Format(PyExc_ValueError, "secret must be 16 bytes, got %zd", view.len);
        return nullptr;
    }

    minecraft::Aes aes;
    aes.set_key(static_cast<const uint8_t *>(view.buf), static_cast<size_t>(view.len));
    CipherObject *self = reinterpret_cast<CipherObject *>(type->tp_alloc(type, 0));
    if (self == nullptr) {
        PyBuffer_Release(&view);
        wipe(&aes, sizeof(aes));
        return nullptr;
    }
    new (&self->outgoing) minecraft::Cfb8();
    new (&self->incoming) minecraft::Cfb8();
    self->outgoing.start(aes, static_cast<const uint8_t *>(view.buf));
    self->incoming.start(aes, static_cast<const uint8_t *>(view.buf));
    self->hardware = aes.uses_hardware();
    PyBuffer_Release(&view);
    wipe(&aes, sizeof(aes));

    return reinterpret_cast<PyObject *>(self);
}

PyObject *cipher_vectorcall(PyObject *callable, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (nargs != 1 || (kwnames != nullptr && PyTuple_GET_SIZE(kwnames) != 0)) {
        PyErr_SetString(PyExc_TypeError, "Cfb8Cipher() takes exactly one positional argument, the secret");
        return nullptr;
    }

    return build(reinterpret_cast<PyTypeObject *>(callable), args[0]);
}

PyObject *cipher_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *secret;
    if ((kwargs != nullptr && PyDict_GET_SIZE(kwargs) != 0) || !PyArg_ParseTuple(args, "O", &secret)) {
        PyErr_SetString(PyExc_TypeError, "Cfb8Cipher() takes exactly one positional argument, the secret");
        return nullptr;
    }

    return build(type, secret);
}

void cipher_dealloc(PyObject *object)
{
    CipherObject *self = reinterpret_cast<CipherObject *>(object);
    PyTypeObject *type = Py_TYPE(object);
    wipe(&self->outgoing, sizeof(self->outgoing));
    wipe(&self->incoming, sizeof(self->incoming));
    type->tp_free(object);
    Py_DECREF(type);
}

PyObject *transform(PyObject *object, PyObject *data, bool encrypting)
{
    CipherObject *self = reinterpret_cast<CipherObject *>(object);
    Py_buffer view;
    if (PyObject_GetBuffer(data, &view, PyBUF_SIMPLE) < 0) {
        return nullptr;
    }
    PyObject *result = PyBytes_FromStringAndSize(nullptr, view.len);
    if (result == nullptr) {
        PyBuffer_Release(&view);
        return nullptr;
    }

    const uint8_t *in = static_cast<const uint8_t *>(view.buf);
    uint8_t *out = reinterpret_cast<uint8_t *>(PyBytes_AS_STRING(result));
    size_t size = static_cast<size_t>(view.len);
    Py_BEGIN_CRITICAL_SECTION(object);
    if (encrypting) {
        self->outgoing.encrypt(in, out, size);
    } else {
        self->incoming.decrypt(in, out, size);
    }
    Py_END_CRITICAL_SECTION();
    PyBuffer_Release(&view);

    return result;
}

PyObject *cipher_encrypt(PyObject *object, PyObject *data)
{
    return transform(object, data, true);
}

PyObject *cipher_decrypt(PyObject *object, PyObject *data)
{
    return transform(object, data, false);
}

PyObject *cipher_hardware(PyObject *object, void *)
{
    return PyBool_FromLong(reinterpret_cast<CipherObject *>(object)->hardware);
}

PyMethodDef cipher_methods[] = {
    {"encrypt", cipher_encrypt, METH_O, encrypt_doc},
    {"decrypt", cipher_decrypt, METH_O, decrypt_doc},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef cipher_getset[] = {
    {"hardware", cipher_hardware, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot cipher_slots[] = {
    {Py_tp_doc, const_cast<char *>(cipher_doc)},
    {Py_tp_new, reinterpret_cast<void *>(cipher_new)},
    {Py_tp_vectorcall, reinterpret_cast<void *>(cipher_vectorcall)},
    {Py_tp_dealloc, reinterpret_cast<void *>(cipher_dealloc)},
    {Py_tp_methods, cipher_methods},
    {Py_tp_getset, cipher_getset},
    {0, nullptr}
};

PyType_Spec cipher_spec = {
    "minecraft.net.cipher.Cfb8Cipher",
    sizeof(CipherObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    cipher_slots
};

int exec(PyObject *module)
{
    PyObject *type = PyType_FromModuleAndSpec(module, &cipher_spec, nullptr);
    if (type == nullptr) {
        return -1;
    }

    int status = PyModule_AddObjectRef(module, "Cfb8Cipher", type);
    Py_DECREF(type);

    return status;
}

PyModuleDef_Slot slots[] = {
    {Py_mod_exec, reinterpret_cast<void *>(exec)},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, nullptr}
};

}

namespace minecraft {

PyModuleDef cipher_definition = {
    PyModuleDef_HEAD_INIT,
    "minecraft._native.cipher",
    module_doc,
    0,
    nullptr,
    slots,
    nullptr,
    nullptr,
    nullptr
};

}
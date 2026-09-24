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
#include <cfloat>
#include <climits>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace {

namespace nbt = minecraft::nbt;

PyDoc_STRVAR(module_doc, "The NBT number types and the load and dump functions behind minecraft.nbt.");

PyDoc_STRVAR(byte_doc,
"Byte(value)\n"
"--\n"
"\n"
"An NBT byte, a signed 8-bit integer.\n"
"\n"
"Behaves like :class:`int` everywhere and only differs in how it is\n"
"written. Values outside -128 to 127 raise :exc:`OverflowError`.");

PyDoc_STRVAR(short_doc,
"Short(value)\n"
"--\n"
"\n"
"An NBT short, a signed 16-bit integer.\n"
"\n"
"Behaves like :class:`int` everywhere and only differs in how it is\n"
"written. Values outside -32768 to 32767 raise :exc:`OverflowError`.");

PyDoc_STRVAR(int_doc,
"Int(value)\n"
"--\n"
"\n"
"An NBT int, a signed 32-bit integer.\n"
"\n"
"Behaves like :class:`int` everywhere and only differs in how it is\n"
"written. A plain :class:`int` is written as this type too, so it is\n"
"only needed to be explicit. Values outside the 32-bit range raise\n"
":exc:`OverflowError`.");

PyDoc_STRVAR(long_doc,
"Long(value)\n"
"--\n"
"\n"
"An NBT long, a signed 64-bit integer.\n"
"\n"
"Behaves like :class:`int` everywhere and only differs in how it is\n"
"written. Values outside the 64-bit range raise :exc:`OverflowError`.");

PyDoc_STRVAR(float_doc,
"Float(value)\n"
"--\n"
"\n"
"An NBT float, a single precision number.\n"
"\n"
"Behaves like :class:`float` everywhere and only differs in how it is\n"
"written: with 32 bits, so at most seven significant digits survive the\n"
"wire. Finite values beyond the single precision range raise\n"
":exc:`OverflowError`.");

PyDoc_STRVAR(double_doc,
"Double(value)\n"
"--\n"
"\n"
"An NBT double, a double precision number.\n"
"\n"
"Behaves like :class:`float` everywhere and only differs in how it is\n"
"written. A plain :class:`float` is written as this type too, so it is\n"
"only needed to be explicit.");

PyDoc_STRVAR(load_doc,
"load(data, format, named, /)\n"
"--\n"
"\n"
"Reads one NBT tag from bytes and builds the Python value for it.\n"
"\n"
"format is 0 for Java, 1 for Bedrock and 2 for Bedrock's network form.\n"
"With named set the root carries a name and the result is a (name, value)\n"
"tuple, otherwise it is the value alone. Anything that does not parse,\n"
"including trailing bytes, raises :exc:`~minecraft.errors.InvalidData`.");

PyDoc_STRVAR(dump_doc,
"dump(value, format, name, /)\n"
"--\n"
"\n"
"Writes a Python value as one NBT tag and returns the bytes.\n"
"\n"
"format is 0 for Java, 1 for Bedrock and 2 for Bedrock's network form.\n"
"name is the root name or None for an unnamed root. Plain ints become\n"
"Int, plain floats become Double, bools become Byte, bytes become byte\n"
"arrays and arrays of typecode i or q become int or long arrays. A value\n"
"that has no NBT form raises :exc:`TypeError`.");

enum NumberIndex
{
    number_byte = 0,
    number_short = 1,
    number_int = 2,
    number_long = 3,
    number_float = 4,
    number_double = 5,
    number_count = 6
};

struct State
{
    PyObject *numbers[number_count];
    PyObject *array_type;
    PyObject *invalid_data;
};

State *state_of(PyObject *module)
{
    return static_cast<State *>(PyModule_GetState(module));
}

PyObject *invalid_data_type(State *state)
{
    if (state->invalid_data == nullptr) {
        PyObject *module = PyImport_ImportModule("minecraft.errors.base");
        if (module == nullptr) {
            return nullptr;
        }
        state->invalid_data = PyObject_GetAttrString(module, "InvalidData");
        Py_DECREF(module);
    }

    return state->invalid_data;
}

const char *describe(nbt::Error error)
{
    switch (error) {
    case nbt::Error::truncated:
        return "NBT data ends in the middle of a tag";
    case nbt::Error::bad_tag:
        return "NBT data contains an unknown tag type";
    case nbt::Error::bad_length:
        return "NBT data contains a negative or malformed length";
    case nbt::Error::bad_string:
        return "NBT data contains a string that is not valid UTF-8";
    case nbt::Error::too_deep:
        return "NBT data nests deeper than 512 levels";
    case nbt::Error::none:
        break;
    }

    return "NBT data is invalid";
}

PyObject *raise_invalid(State *state, nbt::Error error, size_t offset)
{
    PyObject *type = invalid_data_type(state);
    if (type != nullptr) {
        PyErr_Format(type, "%s (at byte %zu)", describe(error), offset);
    }

    return nullptr;
}

template <long long low, long long high>
PyObject *integer_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *value = PyLong_Type.tp_new(type, args, kwargs);
    if (value == nullptr) {
        return nullptr;
    }

    int overflow = 0;
    long long number = PyLong_AsLongLongAndOverflow(value, &overflow);
    if (overflow != 0 || number < low || number > high) {
        PyErr_Format(PyExc_OverflowError, "%s must be between %lld and %lld, got %S", type->tp_name, low, high, value);
        Py_DECREF(value);
        return nullptr;
    }

    return value;
}

PyObject *float_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *value = PyFloat_Type.tp_new(type, args, kwargs);
    if (value == nullptr) {
        return nullptr;
    }

    double number = PyFloat_AS_DOUBLE(value);
    if (std::isfinite(number) && std::fabs(number) > FLT_MAX) {
        PyErr_Format(PyExc_OverflowError, "%s must fit single precision, got %S", type->tp_name, value);
        Py_DECREF(value);
        return nullptr;
    }

    return value;
}

PyObject *number_repr(PyObject *object)
{
    PyObject *inner = PyLong_Check(object) ? PyLong_Type.tp_repr(object) : PyFloat_Type.tp_repr(object);
    if (inner == nullptr) {
        return nullptr;
    }

    PyObject *name = PyType_GetName(Py_TYPE(object));
    if (name == nullptr) {
        Py_DECREF(inner);
        return nullptr;
    }

    PyObject *result = PyUnicode_FromFormat("%U(%U)", name, inner);
    Py_DECREF(name);
    Py_DECREF(inner);

    return result;
}

PyObject *integer_str(PyObject *object)
{
    return PyLong_Type.tp_repr(object);
}

PyObject *float_str(PyObject *object)
{
    return PyFloat_Type.tp_repr(object);
}

PyObject *boxed_integer(PyObject *type, long long value)
{
    PyObject *raw = PyLong_FromLongLong(value);
    if (raw == nullptr) {
        return nullptr;
    }

    PyObject *args = PyTuple_Pack(1, raw);
    Py_DECREF(raw);
    if (args == nullptr) {
        return nullptr;
    }

    PyObject *result = PyLong_Type.tp_new(reinterpret_cast<PyTypeObject *>(type), args, nullptr);
    Py_DECREF(args);

    return result;
}

PyObject *boxed_float(PyObject *type, double value)
{
    PyTypeObject *float_type = reinterpret_cast<PyTypeObject *>(type);
    PyObject *result = float_type->tp_alloc(float_type, 0);
    if (result == nullptr) {
        return nullptr;
    }

    reinterpret_cast<PyFloatObject *>(result)->ob_fval = value;

    return result;
}

PyObject *make_array(State *state, const char *typecode, PyObject *bytes)
{
    return PyObject_CallFunction(state->array_type, "sO", typecode, bytes);
}

template <nbt::Format format>
class Loader
{
public:
    Loader(State *state, const uint8_t *begin, const uint8_t *end) : state(state), reader(begin, end), begin(begin)
    {
    }

    nbt::Reader<format> reader;

    PyObject *fail()
    {
        return raise_invalid(state, reader.error(), static_cast<size_t>(reader.position() - begin));
    }

    PyObject *string()
    {
        std::string_view raw;
        if (!reader.read_string(raw)) {
            return fail();
        }

        if constexpr (format == nbt::Format::java) {
            if (!nbt::is_plain_java_string(raw)) {
                std::string utf8;
                if (!nbt::decode_java_string(raw, utf8)) {
                    return raise_invalid(state, nbt::Error::bad_string, static_cast<size_t>(reader.position() - begin));
                }

                return decoded(utf8.data(), static_cast<Py_ssize_t>(utf8.size()));
            }
        }

        return decoded(raw.data(), static_cast<Py_ssize_t>(raw.size()));
    }

    PyObject *value(nbt::TagType type, int depth)
    {
        if (depth > nbt::max_depth) {
            return raise_invalid(state, nbt::Error::too_deep, static_cast<size_t>(reader.position() - begin));
        }

        switch (type) {
        case nbt::TagType::end:
            return raise_invalid(state, nbt::Error::bad_tag, static_cast<size_t>(reader.position() - begin));
        case nbt::TagType::int8: {
            int8_t number;
            return reader.read_int8(number) ? boxed_integer(state->numbers[number_byte], number) : fail();
        }
        case nbt::TagType::int16: {
            int16_t number;
            return reader.read_int16(number) ? boxed_integer(state->numbers[number_short], number) : fail();
        }
        case nbt::TagType::int32: {
            int32_t number;
            return reader.read_int32(number) ? boxed_integer(state->numbers[number_int], number) : fail();
        }
        case nbt::TagType::int64: {
            int64_t number;
            return reader.read_int64(number) ? boxed_integer(state->numbers[number_long], number) : fail();
        }
        case nbt::TagType::float32: {
            float number;
            return reader.read_float32(number) ? boxed_float(state->numbers[number_float], number) : fail();
        }
        case nbt::TagType::float64: {
            double number;
            return reader.read_float64(number) ? boxed_float(state->numbers[number_double], number) : fail();
        }
        case nbt::TagType::byte_array: {
            int32_t count;
            const uint8_t *data;
            if (!reader.read_array_header(1, count, data)) {
                return fail();
            }

            return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(data), count);
        }
        case nbt::TagType::string:
            return string();
        case nbt::TagType::list:
            return list(depth);
        case nbt::TagType::compound:
            return compound(depth);
        case nbt::TagType::int32_array:
            return int_array<int32_t>("i");
        case nbt::TagType::int64_array:
            return int_array<int64_t>("q");
        }

        return raise_invalid(state, nbt::Error::bad_tag, static_cast<size_t>(reader.position() - begin));
    }

private:
    State *state;
    const uint8_t *begin;

    PyObject *decoded(const char *utf8, Py_ssize_t size)
    {
        PyObject *result = PyUnicode_DecodeUTF8(utf8, size, nullptr);
        if (result == nullptr && PyErr_ExceptionMatches(PyExc_UnicodeDecodeError)) {
            PyErr_Clear();
            return raise_invalid(state, nbt::Error::bad_string, static_cast<size_t>(reader.position() - begin));
        }

        return result;
    }

    PyObject *list(int depth)
    {
        nbt::TagType element;
        int32_t count;
        if (!reader.read_type(element) || !reader.read_length(count)) {
            return fail();
        }
        if (count > 0 && element == nbt::TagType::end) {
            return raise_invalid(state, nbt::Error::bad_tag, static_cast<size_t>(reader.position() - begin));
        }
        if (static_cast<size_t>(count) > reader.remaining()) {
            return raise_invalid(state, nbt::Error::truncated, static_cast<size_t>(reader.position() - begin));
        }

        PyObject *result = PyList_New(count);
        if (result == nullptr) {
            return nullptr;
        }

        for (int32_t i = 0; i < count; i++) {
            PyObject *item = value(element, depth + 1);
            if (item == nullptr) {
                Py_DECREF(result);
                return nullptr;
            }
            PyList_SET_ITEM(result, i, item);
        }

        return result;
    }

    PyObject *compound(int depth)
    {
        PyObject *result = PyDict_New();
        if (result == nullptr) {
            return nullptr;
        }

        for (;;) {
            nbt::TagType child;
            if (!reader.read_type(child)) {
                Py_DECREF(result);
                return fail();
            }
            if (child == nbt::TagType::end) {
                return result;
            }

            PyObject *key = string();
            if (key == nullptr) {
                Py_DECREF(result);
                return nullptr;
            }

            PyObject *item = value(child, depth + 1);
            if (item == nullptr || PyDict_SetItem(result, key, item) < 0) {
                Py_XDECREF(item);
                Py_DECREF(key);
                Py_DECREF(result);
                return nullptr;
            }

            Py_DECREF(item);
            Py_DECREF(key);
        }
    }

    template <typename T>
    PyObject *int_array(const char *typecode)
    {
        int32_t count;
        const uint8_t *data;
        if (!reader.read_array_header(sizeof(T), count, data)) {
            return fail();
        }
        if constexpr (format == nbt::Format::bedrock_network) {
            if (static_cast<size_t>(count) > reader.remaining()) {
                return raise_invalid(state, nbt::Error::truncated, static_cast<size_t>(reader.position() - begin));
            }
        }

        PyObject *bytes = PyBytes_FromStringAndSize(nullptr, static_cast<Py_ssize_t>(count) * static_cast<Py_ssize_t>(sizeof(T)));
        if (bytes == nullptr) {
            return nullptr;
        }

        T *values = reinterpret_cast<T *>(PyBytes_AS_STRING(bytes));
        for (int32_t i = 0; i < count; i++) {
            if constexpr (format == nbt::Format::bedrock_network) {
                if constexpr (sizeof(T) == 4) {
                    int32_t number;
                    if (!reader.read_int32(number)) {
                        Py_DECREF(bytes);
                        return fail();
                    }
                    values[i] = number;
                } else {
                    int64_t number;
                    if (!reader.read_int64(number)) {
                        Py_DECREF(bytes);
                        return fail();
                    }
                    values[i] = number;
                }
            } else if constexpr (format == nbt::Format::java) {
                values[i] = minecraft::load_big<T>(data + static_cast<size_t>(i) * sizeof(T));
            } else {
                values[i] = minecraft::load_little<T>(data + static_cast<size_t>(i) * sizeof(T));
            }
        }

        PyObject *result = make_array(state, typecode, bytes);
        Py_DECREF(bytes);

        return result;
    }
};

template <nbt::Format format>
class Dumper
{
public:
    Dumper(State *state, std::vector<uint8_t> &out) : state(state), writer(out)
    {
    }

    nbt::Writer<format> writer;

    bool classify(PyObject *object, nbt::TagType &type)
    {
        if (PyLong_CheckExact(object)) {
            type = nbt::TagType::int32;
            return true;
        }
        if (PyUnicode_CheckExact(object)) {
            type = nbt::TagType::string;
            return true;
        }
        if (PyDict_CheckExact(object)) {
            type = nbt::TagType::compound;
            return true;
        }
        if (PyList_CheckExact(object)) {
            type = nbt::TagType::list;
            return true;
        }
        if (PyFloat_CheckExact(object)) {
            type = nbt::TagType::float64;
            return true;
        }
        for (int i = 0; i < number_count; i++) {
            if (PyObject_TypeCheck(object, reinterpret_cast<PyTypeObject *>(state->numbers[i]))) {
                type = static_cast<nbt::TagType>(i + 1);
                return true;
            }
        }
        if (PyBool_Check(object)) {
            type = nbt::TagType::int8;
            return true;
        }
        if (PyLong_Check(object)) {
            type = nbt::TagType::int32;
            return true;
        }
        if (PyFloat_Check(object)) {
            type = nbt::TagType::float64;
            return true;
        }
        if (PyUnicode_Check(object)) {
            type = nbt::TagType::string;
            return true;
        }
        if (PyBytes_Check(object) || PyByteArray_Check(object)) {
            type = nbt::TagType::byte_array;
            return true;
        }
        if (PyDict_Check(object)) {
            type = nbt::TagType::compound;
            return true;
        }
        if (PyList_Check(object) || PyTuple_Check(object)) {
            type = nbt::TagType::list;
            return true;
        }
        if (PyObject_TypeCheck(object, reinterpret_cast<PyTypeObject *>(state->array_type))) {
            return classify_array(object, type);
        }

        PyErr_Format(PyExc_TypeError, "cannot write %.100s as NBT", Py_TYPE(object)->tp_name);

        return false;
    }

    bool value(PyObject *object, nbt::TagType type, int depth)
    {
        if (depth > nbt::max_depth) {
            PyErr_SetString(PyExc_ValueError, "NBT nests deeper than 512 levels");
            return false;
        }

        switch (type) {
        case nbt::TagType::end:
            break;
        case nbt::TagType::int8:
            return integer<int8_t>(object, [this](int8_t number) { writer.write_int8(number); });
        case nbt::TagType::int16:
            return integer<int16_t>(object, [this](int16_t number) { writer.write_int16(number); });
        case nbt::TagType::int32:
            return integer<int32_t>(object, [this](int32_t number) { writer.write_int32(number); });
        case nbt::TagType::int64:
            return integer<int64_t>(object, [this](int64_t number) { writer.write_int64(number); });
        case nbt::TagType::float32:
            writer.write_float32(static_cast<float>(PyFloat_AsDouble(object)));
            return !PyErr_Occurred();
        case nbt::TagType::float64:
            writer.write_float64(PyFloat_AsDouble(object));
            return !PyErr_Occurred();
        case nbt::TagType::byte_array:
            return byte_array(object);
        case nbt::TagType::string:
            return string(object);
        case nbt::TagType::list:
            return list(object, depth);
        case nbt::TagType::compound:
            return compound(object, depth);
        case nbt::TagType::int32_array:
            return int_array<int32_t>(object);
        case nbt::TagType::int64_array:
            return int_array<int64_t>(object);
        }

        PyErr_SetString(PyExc_TypeError, "cannot write an End tag as a value");

        return false;
    }

    bool string(PyObject *object)
    {
        Py_ssize_t size;
        const char *utf8 = PyUnicode_AsUTF8AndSize(object, &size);
        if (utf8 == nullptr) {
            return false;
        }
        if (!writer.write_string(std::string_view(utf8, static_cast<size_t>(size)))) {
            PyErr_Format(PyExc_ValueError, "NBT strings hold at most 65535 bytes, got %zd", size);
            return false;
        }

        return true;
    }

private:
    State *state;

    bool classify_array(PyObject *object, nbt::TagType &type)
    {
        PyObject *typecode = PyObject_GetAttrString(object, "typecode");
        if (typecode == nullptr) {
            return false;
        }

        Py_UCS4 code = PyUnicode_GET_LENGTH(typecode) == 1 ? PyUnicode_READ_CHAR(typecode, 0) : 0;
        Py_DECREF(typecode);
        if (code == 'b' || code == 'B') {
            type = nbt::TagType::byte_array;
            return true;
        }
        if (code == 'i') {
            type = nbt::TagType::int32_array;
            return true;
        }
        if (code == 'q') {
            type = nbt::TagType::int64_array;
            return true;
        }

        PyErr_Format(PyExc_TypeError, "NBT arrays need typecode b, i or q, got %c", static_cast<int>(code));

        return false;
    }

    template <typename T, typename Write>
    bool integer(PyObject *object, Write write)
    {
        int overflow = 0;
        long long number = PyLong_AsLongLongAndOverflow(object, &overflow);
        if (number == -1 && PyErr_Occurred()) {
            return false;
        }
        if (overflow != 0 || number < static_cast<long long>(std::numeric_limits<T>::min()) || number > static_cast<long long>(std::numeric_limits<T>::max())) {
            PyErr_Format(PyExc_OverflowError, "%S does not fit the NBT type with %zu bytes; wrap it in Long or a wider type", object, sizeof(T));
            return false;
        }

        write(static_cast<T>(number));

        return true;
    }

    bool byte_array(PyObject *object)
    {
        Py_buffer view;
        if (PyObject_GetBuffer(object, &view, PyBUF_SIMPLE) < 0) {
            return false;
        }
        if (view.len > INT_MAX) {
            PyBuffer_Release(&view);
            PyErr_SetString(PyExc_OverflowError, "NBT byte arrays hold at most 2147483647 bytes");
            return false;
        }

        writer.write_length(static_cast<int32_t>(view.len));
        writer.write_bytes(static_cast<const uint8_t *>(view.buf), static_cast<size_t>(view.len));
        PyBuffer_Release(&view);

        return true;
    }

    bool list(PyObject *object, int depth)
    {
        PyObject *sequence = PySequence_Fast(object, "NBT lists come from lists or tuples");
        if (sequence == nullptr) {
            return false;
        }

        Py_ssize_t count = PySequence_Fast_GET_SIZE(sequence);
        PyObject **items = PySequence_Fast_ITEMS(sequence);
        nbt::TagType element = nbt::TagType::end;
        if (count > 0 && !classify(items[0], element)) {
            Py_DECREF(sequence);
            return false;
        }
        if (count > INT_MAX) {
            Py_DECREF(sequence);
            PyErr_SetString(PyExc_OverflowError, "NBT lists hold at most 2147483647 items");
            return false;
        }

        writer.write_type(element);
        writer.write_length(static_cast<int32_t>(count));
        for (Py_ssize_t i = 0; i < count; i++) {
            nbt::TagType type;
            if (!classify(items[i], type)) {
                Py_DECREF(sequence);
                return false;
            }
            if (type != element) {
                Py_DECREF(sequence);
                PyErr_Format(PyExc_TypeError, "NBT lists hold one type; item %zd is %.100s while the first is tag type %d", i, Py_TYPE(items[i])->tp_name, static_cast<int>(element));
                return false;
            }
            if (!value(items[i], type, depth + 1)) {
                Py_DECREF(sequence);
                return false;
            }
        }

        Py_DECREF(sequence);

        return true;
    }

    bool compound(PyObject *object, int depth)
    {
        Py_ssize_t position = 0;
        PyObject *key;
        PyObject *item;
        while (PyDict_Next(object, &position, &key, &item)) {
            if (!PyUnicode_Check(key)) {
                PyErr_Format(PyExc_TypeError, "NBT compound keys are strings, got %.100s", Py_TYPE(key)->tp_name);
                return false;
            }

            nbt::TagType type;
            if (!classify(item, type)) {
                return false;
            }

            writer.write_type(type);
            if (!string(key) || !value(item, type, depth + 1)) {
                return false;
            }
        }

        writer.write_type(nbt::TagType::end);

        return true;
    }

    template <typename T>
    bool int_array(PyObject *object)
    {
        Py_buffer view;
        if (PyObject_GetBuffer(object, &view, PyBUF_SIMPLE) < 0) {
            return false;
        }

        size_t count = static_cast<size_t>(view.len) / sizeof(T);
        if (count > static_cast<size_t>(INT_MAX)) {
            PyBuffer_Release(&view);
            PyErr_SetString(PyExc_OverflowError, "NBT arrays hold at most 2147483647 items");
            return false;
        }

        const T *values = static_cast<const T *>(view.buf);
        if constexpr (sizeof(T) == 4) {
            writer.write_int32_array(values, static_cast<int32_t>(count));
        } else {
            writer.write_int64_array(values, static_cast<int32_t>(count));
        }
        PyBuffer_Release(&view);

        return true;
    }
};

template <nbt::Format format>
PyObject *load_with(State *state, const uint8_t *begin, const uint8_t *end, bool named, PyObject **name, size_t &consumed)
{
    Loader<format> loader(state, begin, end);
    nbt::TagType root;
    if (!loader.reader.read_type(root)) {
        return loader.fail();
    }
    if (named) {
        *name = loader.string();
        if (*name == nullptr) {
            return nullptr;
        }
    }

    PyObject *result = loader.value(root, 0);
    if (result == nullptr && named) {
        Py_CLEAR(*name);
    }
    consumed = static_cast<size_t>(loader.reader.position() - begin);

    return result;
}

template <nbt::Format format>
bool dump_with(State *state, PyObject *value, PyObject *name, std::vector<uint8_t> &out)
{
    Dumper<format> dumper(state, out);
    nbt::TagType type;
    if (!dumper.classify(value, type)) {
        return false;
    }

    dumper.writer.write_type(type);
    if (name != Py_None && !dumper.string(name)) {
        return false;
    }

    return dumper.value(value, type, 0);
}

bool format_of(long code, nbt::Format &format)
{
    if (code == 0) {
        format = nbt::Format::java;
    } else if (code == 1) {
        format = nbt::Format::bedrock;
    } else if (code == 2) {
        format = nbt::Format::bedrock_network;
    } else {
        PyErr_Format(PyExc_ValueError, "format must be 0 (Java), 1 (Bedrock) or 2 (Bedrock network), got %ld", code);
        return false;
    }

    return true;
}

PyObject *load(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_Format(PyExc_TypeError, "load() takes exactly 3 positional arguments (%zd given)", nargs);
        return nullptr;
    }

    long code = PyLong_AsLong(args[1]);
    nbt::Format format;
    if ((code == -1 && PyErr_Occurred()) || !format_of(code, format)) {
        return nullptr;
    }
    int named = PyObject_IsTrue(args[2]);
    if (named < 0) {
        return nullptr;
    }

    Py_buffer view;
    if (PyObject_GetBuffer(args[0], &view, PyBUF_SIMPLE) < 0) {
        return nullptr;
    }

    const uint8_t *begin = static_cast<const uint8_t *>(view.buf);
    const uint8_t *end = begin + view.len;
    PyObject *name = nullptr;
    size_t consumed = 0;
    PyObject *value = minecraft::nbt_load(module, format, begin, end, named != 0, &name, consumed);
    if (value != nullptr && consumed != static_cast<size_t>(view.len)) {
        PyObject *type = invalid_data_type(state_of(module));
        if (type != nullptr) {
            PyErr_Format(type, "NBT data has %zu trailing bytes after the root tag", static_cast<size_t>(view.len) - consumed);
        }
        Py_CLEAR(value);
        Py_CLEAR(name);
    }
    PyBuffer_Release(&view);
    if (value == nullptr) {
        return nullptr;
    }
    if (!named) {
        return value;
    }

    PyObject *pair = PyTuple_Pack(2, name, value);
    Py_DECREF(name);
    Py_DECREF(value);

    return pair;
}

PyObject *dump(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_Format(PyExc_TypeError, "dump() takes exactly 3 positional arguments (%zd given)", nargs);
        return nullptr;
    }

    long code = PyLong_AsLong(args[1]);
    nbt::Format format;
    if ((code == -1 && PyErr_Occurred()) || !format_of(code, format)) {
        return nullptr;
    }
    if (args[2] != Py_None && !PyUnicode_Check(args[2])) {
        PyErr_Format(PyExc_TypeError, "name must be a str or None, got %.100s", Py_TYPE(args[2])->tp_name);
        return nullptr;
    }

    std::vector<uint8_t> out;
    if (!minecraft::nbt_dump(module, format, args[0], args[2], out)) {
        return nullptr;
    }

    return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(out.data()), static_cast<Py_ssize_t>(out.size()));
}

PyMethodDef methods[] = {
    {"load", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(load)), METH_FASTCALL, load_doc},
    {"dump", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(dump)), METH_FASTCALL, dump_doc},
    {nullptr, nullptr, 0, nullptr}
};

struct NumberSpec
{
    const char *attribute;
    PyType_Spec spec;
    PyTypeObject *base;
};

PyType_Slot byte_slots[] = {
    {Py_tp_doc, const_cast<char *>(byte_doc)},
    {Py_tp_str, reinterpret_cast<void *>(integer_str)},
    {Py_tp_new, reinterpret_cast<void *>(integer_new<-128, 127>)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

PyType_Slot short_slots[] = {
    {Py_tp_doc, const_cast<char *>(short_doc)},
    {Py_tp_str, reinterpret_cast<void *>(integer_str)},
    {Py_tp_new, reinterpret_cast<void *>(integer_new<-32768, 32767>)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

PyType_Slot int_slots[] = {
    {Py_tp_doc, const_cast<char *>(int_doc)},
    {Py_tp_str, reinterpret_cast<void *>(integer_str)},
    {Py_tp_new, reinterpret_cast<void *>(integer_new<INT32_MIN, INT32_MAX>)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

PyType_Slot long_slots[] = {
    {Py_tp_doc, const_cast<char *>(long_doc)},
    {Py_tp_str, reinterpret_cast<void *>(integer_str)},
    {Py_tp_new, reinterpret_cast<void *>(integer_new<INT64_MIN, INT64_MAX>)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

PyType_Slot float_slots[] = {
    {Py_tp_doc, const_cast<char *>(float_doc)},
    {Py_tp_str, reinterpret_cast<void *>(float_str)},
    {Py_tp_new, reinterpret_cast<void *>(float_new)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

PyType_Slot double_slots[] = {
    {Py_tp_doc, const_cast<char *>(double_doc)},
    {Py_tp_str, reinterpret_cast<void *>(float_str)},
    {Py_tp_repr, reinterpret_cast<void *>(number_repr)},
    {0, nullptr}
};

constexpr unsigned long number_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE;

NumberSpec number_specs[number_count] = {
    {"Byte", {"minecraft.nbt.tag.Byte", 0, 0, number_flags, byte_slots}, &PyLong_Type},
    {"Short", {"minecraft.nbt.tag.Short", 0, 0, number_flags, short_slots}, &PyLong_Type},
    {"Int", {"minecraft.nbt.tag.Int", 0, 0, number_flags, int_slots}, &PyLong_Type},
    {"Long", {"minecraft.nbt.tag.Long", 0, 0, number_flags, long_slots}, &PyLong_Type},
    {"Float", {"minecraft.nbt.tag.Float", 0, 0, number_flags, float_slots}, &PyFloat_Type},
    {"Double", {"minecraft.nbt.tag.Double", 0, 0, number_flags, double_slots}, &PyFloat_Type}
};

int exec(PyObject *module)
{
    State *state = state_of(module);
    for (int i = 0; i < number_count; i++) {
        state->numbers[i] = PyType_FromModuleAndSpec(module, &number_specs[i].spec, reinterpret_cast<PyObject *>(number_specs[i].base));
        if (state->numbers[i] == nullptr || PyModule_AddObjectRef(module, number_specs[i].attribute, state->numbers[i]) < 0) {
            return -1;
        }
    }

    PyObject *array_module = PyImport_ImportModule("array");
    if (array_module == nullptr) {
        return -1;
    }
    state->array_type = PyObject_GetAttrString(array_module, "array");
    Py_DECREF(array_module);
    if (state->array_type == nullptr) {
        return -1;
    }

    PyObject *probe = PyObject_CallFunction(state->array_type, "s", "i");
    if (probe == nullptr) {
        return -1;
    }
    PyObject *itemsize = PyObject_GetAttrString(probe, "itemsize");
    Py_DECREF(probe);
    if (itemsize == nullptr) {
        return -1;
    }
    long size = PyLong_AsLong(itemsize);
    Py_DECREF(itemsize);
    if (size != 4) {
        PyErr_SetString(PyExc_ImportError, "array typecode i is not 4 bytes on this platform");
        return -1;
    }

    return 0;
}

int traverse(PyObject *module, visitproc visit, void *arg)
{
    State *state = state_of(module);
    for (int i = 0; i < number_count; i++) {
        Py_VISIT(state->numbers[i]);
    }
    Py_VISIT(state->array_type);
    Py_VISIT(state->invalid_data);

    return 0;
}

int clear(PyObject *module)
{
    State *state = state_of(module);
    for (int i = 0; i < number_count; i++) {
        Py_CLEAR(state->numbers[i]);
    }
    Py_CLEAR(state->array_type);
    Py_CLEAR(state->invalid_data);

    return 0;
}

void free_state(void *module)
{
    clear(static_cast<PyObject *>(module));
}

PyModuleDef_Slot slots[] = {
    {Py_mod_exec, reinterpret_cast<void *>(exec)},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, nullptr}
};

}

namespace minecraft {

PyObject *nbt_load(PyObject *nbt_module, nbt::Format format, const uint8_t *begin, const uint8_t *end, bool named, PyObject **name, size_t &consumed)
{
    State *state = state_of(nbt_module);
    switch (format) {
    case nbt::Format::java:
        return load_with<nbt::Format::java>(state, begin, end, named, name, consumed);
    case nbt::Format::bedrock:
        return load_with<nbt::Format::bedrock>(state, begin, end, named, name, consumed);
    case nbt::Format::bedrock_network:
        return load_with<nbt::Format::bedrock_network>(state, begin, end, named, name, consumed);
    }

    PyErr_SetString(PyExc_ValueError, "unknown NBT format");

    return nullptr;
}

bool nbt_dump(PyObject *nbt_module, nbt::Format format, PyObject *value, PyObject *name, std::vector<uint8_t> &out)
{
    State *state = state_of(nbt_module);
    switch (format) {
    case nbt::Format::java:
        return dump_with<nbt::Format::java>(state, value, name, out);
    case nbt::Format::bedrock:
        return dump_with<nbt::Format::bedrock>(state, value, name, out);
    case nbt::Format::bedrock_network:
        return dump_with<nbt::Format::bedrock_network>(state, value, name, out);
    }

    PyErr_SetString(PyExc_ValueError, "unknown NBT format");

    return false;
}

PyModuleDef nbt_definition = {
    PyModuleDef_HEAD_INIT,
    "minecraft._native.nbt",
    module_doc,
    sizeof(State),
    methods,
    slots,
    traverse,
    clear,
    free_state
};

}
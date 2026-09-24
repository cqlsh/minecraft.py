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

#include <minecraft/buffer.hpp>
#include <minecraft/nbt_python.hpp>
#include <minecraft/position.hpp>
#include <climits>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

using minecraft::Cursor;
using minecraft::ReadError;
using minecraft::Sink;

PyDoc_STRVAR(module_doc, "The packet reader, writer and codec types behind minecraft.net.buffer.");

PyDoc_STRVAR(reader_doc,
"Reader(data, /, *, little=False)\n"
"--\n"
"\n"
"Reads packet fields from a bytes-like object, front to back.\n"
"\n"
"Every method takes the next field off the data and moves on. Fixed-width\n"
"numbers are big endian for Java and little endian for Bedrock, which the\n"
"``little`` flag decides. Reading past the end, a malformed varint or an\n"
"over-long string raises :exc:`~minecraft.errors.InvalidData`, so a\n"
"connection that sends garbage fails at the first bad field.\n"
"\n"
"Parameters\n"
"-----------\n"
"data: :class:`bytes`\n"
"    The packet payload. Other bytes-like objects work as well and are\n"
"    held for as long as the reader lives.\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are little endian, as Bedrock sends them.\n"
"\n"
"Attributes\n"
"-----------\n"
"remaining: :class:`int`\n"
"    How many bytes are left to read.\n"
"offset: :class:`int`\n"
"    How many bytes have been read so far.\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are read little endian.");

PyDoc_STRVAR(writer_doc,
"Writer(*, little=False)\n"
"--\n"
"\n"
"Builds a packet payload field by field.\n"
"\n"
"Every method appends one field. Fixed-width numbers are big endian for\n"
"Java and little endian for Bedrock, which the ``little`` flag decides.\n"
":meth:`take` hands the bytes over and empties the writer, so one writer\n"
"can build packet after packet without allocating again.\n"
"\n"
"Parameters\n"
"-----------\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are little endian, as Bedrock expects.\n"
"\n"
"Attributes\n"
"-----------\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are written little endian.");

PyDoc_STRVAR(codec_doc,
"Codec(spec, /, *, little=False)\n"
"--\n"
"\n"
"A compiled packet layout that decodes and encodes in one call.\n"
"\n"
"The spec lists the fields in order, one letter each, with spaces allowed\n"
"between them. A number after a letter sets a limit: the maximum length\n"
"of a string, the exact size of a raw field or the maximum count of an\n"
"array.\n"
"\n"
"+--------+---------------------------------------------------------+\n"
"| Letter | Field                                                   |\n"
"+========+=========================================================+\n"
"| ``?``  | boolean                                                 |\n"
"| ``b``  | signed 8-bit integer, ``B`` unsigned                    |\n"
"| ``h``  | signed 16-bit integer, ``H`` unsigned                   |\n"
"| ``i``  | signed 32-bit integer, ``I`` unsigned                   |\n"
"| ``q``  | signed 64-bit integer, ``Q`` unsigned                   |\n"
"| ``f``  | 32-bit float, ``d`` 64-bit double                       |\n"
"| ``v``  | Java varint, ``V`` Java varlong                         |\n"
"| ``u``  | unsigned varint, ``U`` unsigned varlong                 |\n"
"| ``z``  | zigzag varint, ``Z`` zigzag varlong                     |\n"
"| ``s``  | string, at most 32767 characters unless a number follows|\n"
"| ``g``  | UUID                                                    |\n"
"| ``p``  | block position                                          |\n"
"| ``n``  | NBT, unnamed for Java and named for Bedrock             |\n"
"| ``x``  | raw bytes of the size that follows                      |\n"
"| ``a``  | byte array with a varint length prefix                  |\n"
"| ``r``  | every remaining byte, only as the last field            |\n"
"| ``[]`` | array of the fields inside, with a varint count prefix  |\n"
"| ``()`` | optional fields inside, with a boolean prefix           |\n"
"+--------+---------------------------------------------------------+\n"
"\n"
"An array decodes to a list and an optional to ``None`` or the value. A\n"
"group of one field yields that field alone, a longer group a tuple.\n"
"\n"
"Parameters\n"
"-----------\n"
"spec: :class:`str`\n"
"    The field letters.\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are little endian, as Bedrock uses them.\n"
"\n"
"Attributes\n"
"-----------\n"
"spec: :class:`str`\n"
"    The spec the codec was built from.\n"
"fields: :class:`int`\n"
"    How many top-level fields the codec reads and writes.\n"
"little: :class:`bool`\n"
"    Whether fixed-width numbers are little endian.");

PyDoc_STRVAR(decode_doc,
"decode(data, /)\n"
"--\n"
"\n"
"Reads every field from a whole packet payload.\n"
"\n"
"Returns a tuple with one value per field. Bytes left over after the\n"
"last field count as an error, because they mean the codec does not\n"
"match the packet.\n"
"\n"
"Raises\n"
"-------\n"
"~minecraft.errors.InvalidData\n"
"    The payload is shorter than the fields need, has trailing bytes or\n"
"    holds a value the codec rejects.");

PyDoc_STRVAR(encode_doc,
"encode(*values)\n"
"--\n"
"\n"
"Writes one value per field and returns the payload bytes.\n"
"\n"
"Raises\n"
"-------\n"
"TypeError\n"
"    A value has the wrong type or the count does not match the fields.\n"
"OverflowError\n"
"    A number does not fit its field.\n"
"ValueError\n"
"    A string or byte field is longer than its limit.");

PyDoc_STRVAR(read_doc,
"read(reader, /)\n"
"--\n"
"\n"
"Reads the fields from a :class:`Reader` at its current position.\n"
"\n"
"Unlike :meth:`decode` this leaves whatever follows untouched, so a\n"
"packet can start with a codec and continue by hand.");

PyDoc_STRVAR(write_doc,
"write(writer, /, *values)\n"
"--\n"
"\n"
"Appends the fields to a :class:`Writer`.");

enum class Kind : uint8_t
{
    boolean,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    f32,
    f64,
    varint,
    varlong,
    uvarint,
    uvarlong,
    zigzag32,
    zigzag64,
    string,
    uuid,
    position,
    nbt,
    raw,
    byte_array,
    rest,
    array,
    optional
};

constexpr uint32_t no_limit = 0xFFFFFFFF;
constexpr uint32_t nbt_by_edition = 0;
constexpr uint32_t nbt_named = 1;
constexpr uint32_t nbt_unnamed = 2;

struct Op
{
    Kind kind;
    uint32_t limit;
    uint32_t child_begin;
    uint32_t child_count;
};

struct State
{
    PyObject *invalid_data;
    PyObject *position_type;
    PyObject *nbt_module;
    PyObject *uuid_type;
    PyObject *safe_unknown;
    PyObject *str_int;
    PyObject *str_is_safe;
    PyObject *empty_string;
    PyObject *reader_type;
    PyObject *writer_type;
    PyObject *codec_type;
};

struct ReaderObject
{
    PyObject_HEAD
    State *state;
    PyObject *source;
    Py_buffer view;
    Cursor cursor;
    bool little;
};

struct WriterObject
{
    PyObject_HEAD
    State *state;
    std::vector<uint8_t> storage;
    bool little;
};

struct CodecObject
{
    PyObject_HEAD
    State *state;
    std::vector<Op> ops;
    PyObject *spec;
    Py_ssize_t fields;
    bool little;
};

const char *kind_name(Kind kind)
{
    switch (kind) {
    case Kind::boolean:
        return "boolean";
    case Kind::i8:
        return "i8";
    case Kind::u8:
        return "u8";
    case Kind::i16:
        return "i16";
    case Kind::u16:
        return "u16";
    case Kind::i32:
        return "i32";
    case Kind::u32:
        return "u32";
    case Kind::i64:
        return "i64";
    case Kind::u64:
        return "u64";
    case Kind::f32:
        return "f32";
    case Kind::f64:
        return "f64";
    case Kind::varint:
        return "varint";
    case Kind::varlong:
        return "varlong";
    case Kind::uvarint:
        return "unsigned varint";
    case Kind::uvarlong:
        return "unsigned varlong";
    case Kind::zigzag32:
        return "zigzag varint";
    case Kind::zigzag64:
        return "zigzag varlong";
    case Kind::string:
        return "string";
    case Kind::uuid:
        return "uuid";
    case Kind::position:
        return "position";
    case Kind::nbt:
        return "nbt";
    case Kind::raw:
        return "raw bytes";
    case Kind::byte_array:
        return "byte array";
    case Kind::rest:
        return "remaining bytes";
    case Kind::array:
        return "array";
    case Kind::optional:
        return "optional";
    }

    return "field";
}

PyObject *lazy_attribute(PyObject **slot, const char *module_name, const char *attribute)
{
    if (*slot == nullptr) {
        PyObject *module = PyImport_ImportModule(module_name);
        if (module == nullptr) {
            return nullptr;
        }
        *slot = PyObject_GetAttrString(module, attribute);
        Py_DECREF(module);
    }

    return *slot;
}

PyObject *invalid_data_type(State *state)
{
    return lazy_attribute(&state->invalid_data, "minecraft.errors.base", "InvalidData");
}

PyObject *position_type(State *state)
{
    return lazy_attribute(&state->position_type, "minecraft.world.position", "BlockPosition");
}

PyObject *nbt_module(State *state)
{
    if (state->nbt_module == nullptr) {
        state->nbt_module = PyImport_ImportModule("minecraft._native.nbt");
    }

    return state->nbt_module;
}

PyObject *raise_invalid(State *state, const char *format, const char *what, size_t offset)
{
    PyObject *type = invalid_data_type(state);
    if (type != nullptr) {
        PyErr_Format(type, format, what, offset);
    }

    return nullptr;
}

PyObject *read_failure(State *state, const Cursor &cursor, const Op &op)
{
    const char *what = kind_name(op.kind);
    switch (cursor.error()) {
    case ReadError::malformed:
        return raise_invalid(state, "malformed varint while reading %s at byte %zu", what, cursor.offset());
    case ReadError::too_long:
        return raise_invalid(state, "%s is longer than allowed at byte %zu", what, cursor.offset());
    case ReadError::truncated:
    case ReadError::none:
        break;
    }

    return raise_invalid(state, "packet ends early while reading %s at byte %zu", what, cursor.offset());
}

inline PyObject *box(int8_t value)
{
    return PyLong_FromLong(value);
}

inline PyObject *box(uint8_t value)
{
    return PyLong_FromLong(value);
}

inline PyObject *box(int16_t value)
{
    return PyLong_FromLong(value);
}

inline PyObject *box(uint16_t value)
{
    return PyLong_FromLong(value);
}

inline PyObject *box(int32_t value)
{
    return PyLong_FromLong(value);
}

inline PyObject *box(uint32_t value)
{
    return PyLong_FromUnsignedLong(value);
}

inline PyObject *box(int64_t value)
{
    return PyLong_FromLongLong(value);
}

inline PyObject *box(uint64_t value)
{
    return PyLong_FromUnsignedLongLong(value);
}

inline PyObject *box(float value)
{
    return PyFloat_FromDouble(value);
}

inline PyObject *box(double value)
{
    return PyFloat_FromDouble(value);
}

template <typename T>
PyObject *read_fixed(State *state, Cursor &cursor, bool little, const Op &op)
{
    T value;
    bool ok = little ? cursor.read_little<T>(value) : cursor.read_big<T>(value);

    return ok ? box(value) : read_failure(state, cursor, op);
}

PyObject *safe_unknown(State *state)
{
    if (state->safe_unknown == nullptr) {
        PyObject *enum_type = nullptr;
        if (lazy_attribute(&enum_type, "uuid", "SafeUUID") == nullptr) {
            return nullptr;
        }
        state->safe_unknown = PyObject_GetAttrString(enum_type, "unknown");
        Py_DECREF(enum_type);
    }

    return state->safe_unknown;
}

PyObject *make_uuid(State *state, const uint8_t *bytes)
{
    PyObject *type = lazy_attribute(&state->uuid_type, "uuid", "UUID");
    PyObject *unknown = safe_unknown(state);
    if (type == nullptr || unknown == nullptr) {
        return nullptr;
    }

    PyObject *number = PyLong_FromNativeBytes(bytes, 16, Py_ASNATIVEBYTES_BIG_ENDIAN | Py_ASNATIVEBYTES_UNSIGNED_BUFFER);
    if (number == nullptr) {
        return nullptr;
    }

    PyTypeObject *uuid_type = reinterpret_cast<PyTypeObject *>(type);
    PyObject *result = uuid_type->tp_alloc(uuid_type, 0);
    if (result == nullptr || PyObject_GenericSetAttr(result, state->str_int, number) < 0 || PyObject_GenericSetAttr(result, state->str_is_safe, unknown) < 0) {
        Py_XDECREF(result);
        Py_DECREF(number);
        return nullptr;
    }
    Py_DECREF(number);

    return result;
}

PyObject *read_string(State *state, Cursor &cursor, const Op &op)
{
    std::string_view view;
    if (!cursor.read_java_string(op.limit, view)) {
        return read_failure(state, cursor, op);
    }
    if (view.size() > op.limit && minecraft::utf16_length(view) > op.limit) {
        return raise_invalid(state, "%s is longer than allowed at byte %zu", kind_name(op.kind), cursor.offset());
    }

    PyObject *result = PyUnicode_DecodeUTF8(view.data(), static_cast<Py_ssize_t>(view.size()), nullptr);
    if (result == nullptr && PyErr_ExceptionMatches(PyExc_UnicodeDecodeError)) {
        PyErr_Clear();
        return raise_invalid(state, "invalid UTF-8 in %s at byte %zu", kind_name(op.kind), cursor.offset());
    }

    return result;
}

PyObject *read_nbt(State *state, Cursor &cursor, bool little, const Op &op)
{
    PyObject *module = nbt_module(state);
    if (module == nullptr) {
        return nullptr;
    }

    bool named = op.limit == nbt_by_edition ? little : op.limit == nbt_named;
    minecraft::nbt::Format format = little ? minecraft::nbt::Format::bedrock_network : minecraft::nbt::Format::java;
    PyObject *name = nullptr;
    size_t consumed = 0;
    PyObject *value = minecraft::nbt_load(module, format, cursor.position(), cursor.position() + cursor.remaining(), named, &name, consumed);
    Py_XDECREF(name);
    if (value == nullptr) {
        return nullptr;
    }
    cursor.skip(consumed);

    return value;
}

PyObject *read_scalar(State *state, Cursor &cursor, bool little, const Op &op)
{
    switch (op.kind) {
    case Kind::boolean: {
        uint8_t value;
        if (!cursor.read_big<uint8_t>(value)) {
            return read_failure(state, cursor, op);
        }
        return PyBool_FromLong(value != 0);
    }
    case Kind::i8:
        return read_fixed<int8_t>(state, cursor, little, op);
    case Kind::u8:
        return read_fixed<uint8_t>(state, cursor, little, op);
    case Kind::i16:
        return read_fixed<int16_t>(state, cursor, little, op);
    case Kind::u16:
        return read_fixed<uint16_t>(state, cursor, little, op);
    case Kind::i32:
        return read_fixed<int32_t>(state, cursor, little, op);
    case Kind::u32:
        return read_fixed<uint32_t>(state, cursor, little, op);
    case Kind::i64:
        return read_fixed<int64_t>(state, cursor, little, op);
    case Kind::u64:
        return read_fixed<uint64_t>(state, cursor, little, op);
    case Kind::f32:
        return read_fixed<float>(state, cursor, little, op);
    case Kind::f64:
        return read_fixed<double>(state, cursor, little, op);
    case Kind::varint: {
        uint32_t value;
        return cursor.read_varint(value) ? box(static_cast<int32_t>(value)) : read_failure(state, cursor, op);
    }
    case Kind::varlong: {
        uint64_t value;
        return cursor.read_varlong(value) ? box(static_cast<int64_t>(value)) : read_failure(state, cursor, op);
    }
    case Kind::uvarint: {
        uint32_t value;
        return cursor.read_varint(value) ? box(value) : read_failure(state, cursor, op);
    }
    case Kind::uvarlong: {
        uint64_t value;
        return cursor.read_varlong(value) ? box(value) : read_failure(state, cursor, op);
    }
    case Kind::zigzag32: {
        int32_t value;
        return cursor.read_zigzag32(value) ? box(value) : read_failure(state, cursor, op);
    }
    case Kind::zigzag64: {
        int64_t value;
        return cursor.read_zigzag64(value) ? box(value) : read_failure(state, cursor, op);
    }
    case Kind::string:
        return read_string(state, cursor, op);
    case Kind::uuid: {
        const uint8_t *bytes;
        if (!cursor.read_bytes(16, bytes)) {
            return read_failure(state, cursor, op);
        }
        return make_uuid(state, bytes);
    }
    case Kind::position: {
        int64_t packed;
        if (!cursor.read_big<int64_t>(packed)) {
            return read_failure(state, cursor, op);
        }
        PyObject *type = position_type(state);
        if (type == nullptr) {
            return nullptr;
        }
        int x, y, z;
        minecraft::unpack_position(packed, x, y, z);
        return minecraft::make_position(reinterpret_cast<PyTypeObject *>(type), x, y, z);
    }
    case Kind::nbt:
        return read_nbt(state, cursor, little, op);
    case Kind::raw: {
        const uint8_t *bytes;
        if (!cursor.read_bytes(op.limit, bytes)) {
            return read_failure(state, cursor, op);
        }
        return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bytes), op.limit);
    }
    case Kind::byte_array: {
        uint32_t count;
        const uint8_t *bytes;
        if (!cursor.read_java_array(op.limit, count) || !cursor.read_bytes(count, bytes)) {
            return read_failure(state, cursor, op);
        }
        return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bytes), count);
    }
    case Kind::rest: {
        const uint8_t *bytes;
        size_t count = cursor.remaining();
        cursor.read_bytes(count, bytes);
        return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bytes), static_cast<Py_ssize_t>(count));
    }
    case Kind::array:
    case Kind::optional:
        break;
    }

    PyErr_SetString(PyExc_SystemError, "group used as a scalar");

    return nullptr;
}

PyObject *read_op(State *state, Cursor &cursor, bool little, const std::vector<Op> &ops, const Op &op);

PyObject *read_group(State *state, Cursor &cursor, bool little, const std::vector<Op> &ops, const Op &group)
{
    if (group.child_count == 1) {
        return read_op(state, cursor, little, ops, ops[group.child_begin]);
    }

    PyObject *result = PyTuple_New(group.child_count);
    if (result == nullptr) {
        return nullptr;
    }
    for (uint32_t i = 0; i < group.child_count; i++) {
        PyObject *item = read_op(state, cursor, little, ops, ops[group.child_begin + i]);
        if (item == nullptr) {
            Py_DECREF(result);
            return nullptr;
        }
        PyTuple_SET_ITEM(result, i, item);
    }

    return result;
}

PyObject *read_op(State *state, Cursor &cursor, bool little, const std::vector<Op> &ops, const Op &op)
{
    if (op.kind == Kind::array) {
        uint32_t count;
        if (!cursor.read_java_array(op.limit, count)) {
            return read_failure(state, cursor, op);
        }
        PyObject *result = PyList_New(count);
        if (result == nullptr) {
            return nullptr;
        }
        for (uint32_t i = 0; i < count; i++) {
            PyObject *item = read_group(state, cursor, little, ops, op);
            if (item == nullptr) {
                Py_DECREF(result);
                return nullptr;
            }
            PyList_SET_ITEM(result, i, item);
        }
        return result;
    }
    if (op.kind == Kind::optional) {
        uint8_t present;
        if (!cursor.read_big<uint8_t>(present)) {
            return read_failure(state, cursor, op);
        }
        if (present == 0) {
            Py_RETURN_NONE;
        }
        return read_group(state, cursor, little, ops, op);
    }

    return read_scalar(state, cursor, little, op);
}

bool unbox_integer(PyObject *value, long long low, long long high, const Op &op, long long &out)
{
    int overflow = 0;
    long long number = PyLong_AsLongLongAndOverflow(value, &overflow);
    if (number == -1 && PyErr_Occurred()) {
        return false;
    }
    if (overflow != 0 || number < low || number > high) {
        PyErr_Format(PyExc_OverflowError, "%S does not fit a %s field", value, kind_name(op.kind));
        return false;
    }
    out = number;

    return true;
}

template <typename T>
bool write_fixed(Sink &sink, bool little, T value)
{
    if (little) {
        sink.write_little<T>(value);
    } else {
        sink.write_big<T>(value);
    }

    return true;
}

template <typename T>
bool write_integer(Sink &sink, bool little, const Op &op, PyObject *value)
{
    long long number;
    if (!unbox_integer(value, static_cast<long long>(std::numeric_limits<T>::min()), static_cast<long long>(std::numeric_limits<T>::max()), op, number)) {
        return false;
    }

    return write_fixed<T>(sink, little, static_cast<T>(number));
}

template <typename T>
bool write_float(Sink &sink, bool little, PyObject *value)
{
    double number = PyFloat_AsDouble(value);
    if (number == -1.0 && PyErr_Occurred()) {
        return false;
    }

    return write_fixed<T>(sink, little, static_cast<T>(number));
}

bool write_buffer(Sink &sink, const Op &op, PyObject *value, bool prefixed)
{
    Py_buffer view;
    if (PyObject_GetBuffer(value, &view, PyBUF_SIMPLE) < 0) {
        return false;
    }
    bool ok = true;
    if (op.kind == Kind::raw && static_cast<size_t>(view.len) != op.limit) {
        PyErr_Format(PyExc_ValueError, "raw field needs exactly %u bytes, got %zd", op.limit, view.len);
        ok = false;
    } else if (static_cast<uint64_t>(view.len) > op.limit) {
        PyErr_Format(PyExc_ValueError, "%s holds at most %u bytes, got %zd", kind_name(op.kind), op.limit, view.len);
        ok = false;
    } else {
        if (prefixed) {
            sink.write_varint(static_cast<uint32_t>(view.len));
        }
        sink.write_bytes(static_cast<const uint8_t *>(view.buf), static_cast<size_t>(view.len));
    }
    PyBuffer_Release(&view);

    return ok;
}

bool write_string(Sink &sink, const Op &op, PyObject *value)
{
    if (!PyUnicode_Check(value)) {
        PyErr_Format(PyExc_TypeError, "string field needs a str, got %.100s", Py_TYPE(value)->tp_name);
        return false;
    }
    Py_ssize_t size;
    const char *utf8 = PyUnicode_AsUTF8AndSize(value, &size);
    if (utf8 == nullptr) {
        return false;
    }
    std::string_view view(utf8, static_cast<size_t>(size));
    if (view.size() > op.limit && minecraft::utf16_length(view) > op.limit) {
        PyErr_Format(PyExc_ValueError, "string holds at most %u characters, got %zu", op.limit, minecraft::utf16_length(view));
        return false;
    }
    sink.write_java_string(view);

    return true;
}

bool write_uuid(State *state, Sink &sink, PyObject *value)
{
    PyObject *number = PyObject_GetAttr(value, state->str_int);
    if (number == nullptr) {
        if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
            PyErr_Clear();
            PyErr_Format(PyExc_TypeError, "uuid field needs a UUID, got %.100s", Py_TYPE(value)->tp_name);
        }
        return false;
    }
    uint8_t bytes[16];
    Py_ssize_t written = PyLong_AsNativeBytes(number, bytes, 16, Py_ASNATIVEBYTES_BIG_ENDIAN | Py_ASNATIVEBYTES_UNSIGNED_BUFFER);
    Py_DECREF(number);
    if (written < 0) {
        return false;
    }
    sink.write_bytes(bytes, 16);

    return true;
}

bool write_position(State *state, Sink &sink, PyObject *value)
{
    PyObject *type = position_type(state);
    if (type == nullptr) {
        return false;
    }
    if (!PyObject_TypeCheck(value, reinterpret_cast<PyTypeObject *>(type))) {
        PyErr_Format(PyExc_TypeError, "position field needs a BlockPosition, got %.100s", Py_TYPE(value)->tp_name);
        return false;
    }
    const minecraft::BlockPosition *position = reinterpret_cast<const minecraft::BlockPosition *>(value);
    sink.write_big<int64_t>(minecraft::pack_position(position->x, position->y, position->z));

    return true;
}

bool write_nbt(State *state, std::vector<uint8_t> &storage, bool little, const Op &op, PyObject *value)
{
    PyObject *module = nbt_module(state);
    if (module == nullptr) {
        return false;
    }
    bool named = op.limit == nbt_by_edition ? little : op.limit == nbt_named;
    minecraft::nbt::Format format = little ? minecraft::nbt::Format::bedrock_network : minecraft::nbt::Format::java;

    return minecraft::nbt_dump(module, format, value, named ? state->empty_string : Py_None, storage);
}

bool write_scalar(State *state, std::vector<uint8_t> &storage, bool little, const Op &op, PyObject *value)
{
    Sink sink(storage);
    long long number;
    switch (op.kind) {
    case Kind::boolean: {
        int truth = value == Py_True ? 1 : value == Py_False ? 0 : PyObject_IsTrue(value);
        if (truth < 0) {
            return false;
        }
        sink.write_byte(static_cast<uint8_t>(truth));
        return true;
    }
    case Kind::i8:
        return write_integer<int8_t>(sink, little, op, value);
    case Kind::u8:
        return write_integer<uint8_t>(sink, little, op, value);
    case Kind::i16:
        return write_integer<int16_t>(sink, little, op, value);
    case Kind::u16:
        return write_integer<uint16_t>(sink, little, op, value);
    case Kind::i32:
        return write_integer<int32_t>(sink, little, op, value);
    case Kind::u32:
        return write_integer<uint32_t>(sink, little, op, value);
    case Kind::i64:
        return write_integer<int64_t>(sink, little, op, value);
    case Kind::u64: {
        unsigned long long wide = PyLong_AsUnsignedLongLong(value);
        if (wide == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
            return false;
        }
        return write_fixed<uint64_t>(sink, little, wide);
    }
    case Kind::f32:
        return write_float<float>(sink, little, value);
    case Kind::f64:
        return write_float<double>(sink, little, value);
    case Kind::varint:
        if (!unbox_integer(value, INT32_MIN, INT32_MAX, op, number)) {
            return false;
        }
        sink.write_varint(static_cast<uint32_t>(static_cast<int32_t>(number)));
        return true;
    case Kind::varlong:
        if (!unbox_integer(value, INT64_MIN, INT64_MAX, op, number)) {
            return false;
        }
        sink.write_varlong(static_cast<uint64_t>(number));
        return true;
    case Kind::uvarint:
        if (!unbox_integer(value, 0, UINT32_MAX, op, number)) {
            return false;
        }
        sink.write_varint(static_cast<uint32_t>(number));
        return true;
    case Kind::uvarlong: {
        unsigned long long wide = PyLong_AsUnsignedLongLong(value);
        if (wide == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
            return false;
        }
        sink.write_varlong(wide);
        return true;
    }
    case Kind::zigzag32:
        if (!unbox_integer(value, INT32_MIN, INT32_MAX, op, number)) {
            return false;
        }
        sink.write_zigzag32(static_cast<int32_t>(number));
        return true;
    case Kind::zigzag64:
        if (!unbox_integer(value, INT64_MIN, INT64_MAX, op, number)) {
            return false;
        }
        sink.write_zigzag64(number);
        return true;
    case Kind::string:
        return write_string(sink, op, value);
    case Kind::uuid:
        return write_uuid(state, sink, value);
    case Kind::position:
        return write_position(state, sink, value);
    case Kind::nbt:
        return write_nbt(state, storage, little, op, value);
    case Kind::raw:
    case Kind::rest:
        return write_buffer(sink, op, value, false);
    case Kind::byte_array:
        return write_buffer(sink, op, value, true);
    case Kind::array:
    case Kind::optional:
        break;
    }

    PyErr_SetString(PyExc_SystemError, "group used as a scalar");

    return false;
}

bool write_op(State *state, std::vector<uint8_t> &storage, bool little, const std::vector<Op> &ops, const Op &op, PyObject *value);

bool write_group(State *state, std::vector<uint8_t> &storage, bool little, const std::vector<Op> &ops, const Op &group, PyObject *value)
{
    if (group.child_count == 1) {
        return write_op(state, storage, little, ops, ops[group.child_begin], value);
    }

    PyObject *sequence = PySequence_Fast(value, "a group of fields needs a tuple or list");
    if (sequence == nullptr) {
        return false;
    }
    bool ok = true;
    if (PySequence_Fast_GET_SIZE(sequence) != static_cast<Py_ssize_t>(group.child_count)) {
        PyErr_Format(PyExc_TypeError, "group needs %u values, got %zd", group.child_count, PySequence_Fast_GET_SIZE(sequence));
        ok = false;
    }
    PyObject **items = PySequence_Fast_ITEMS(sequence);
    for (uint32_t i = 0; ok && i < group.child_count; i++) {
        ok = write_op(state, storage, little, ops, ops[group.child_begin + i], items[i]);
    }
    Py_DECREF(sequence);

    return ok;
}

bool write_op(State *state, std::vector<uint8_t> &storage, bool little, const std::vector<Op> &ops, const Op &op, PyObject *value)
{
    if (op.kind == Kind::array) {
        PyObject *sequence = PySequence_Fast(value, "array field needs a list or tuple");
        if (sequence == nullptr) {
            return false;
        }
        Py_ssize_t count = PySequence_Fast_GET_SIZE(sequence);
        bool ok = true;
        if (static_cast<uint64_t>(count) > op.limit) {
            PyErr_Format(PyExc_ValueError, "array holds at most %u items, got %zd", op.limit, count);
            ok = false;
        } else {
            Sink(storage).write_varint(static_cast<uint32_t>(count));
        }
        PyObject **items = PySequence_Fast_ITEMS(sequence);
        for (Py_ssize_t i = 0; ok && i < count; i++) {
            ok = write_group(state, storage, little, ops, op, items[i]);
        }
        Py_DECREF(sequence);
        return ok;
    }
    if (op.kind == Kind::optional) {
        Sink sink(storage);
        if (value == Py_None) {
            sink.write_byte(0);
            return true;
        }
        sink.write_byte(1);
        return write_group(state, storage, little, ops, op, value);
    }

    return write_scalar(state, storage, little, op, value);
}

struct Node
{
    Kind kind;
    uint32_t limit;
    std::vector<Node> children;
};

class SpecParser
{
public:
    explicit SpecParser(const char *text) : begin(text), cursor(text)
    {
    }

    bool parse(std::vector<Node> &nodes)
    {
        if (!parse_sequence(nodes, '\0')) {
            return false;
        }
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].kind == Kind::rest && i + 1 != nodes.size()) {
                return error("the rest field must come last");
            }
        }

        return true;
    }

private:
    const char *begin;
    const char *cursor;

    bool error(const char *message)
    {
        PyErr_Format(PyExc_ValueError, "%s at position %zd of the codec spec", message, static_cast<Py_ssize_t>(cursor - begin));

        return false;
    }

    bool parse_number(uint32_t &value, bool &present)
    {
        present = false;
        if (*cursor < '0' || *cursor > '9') {
            return true;
        }
        uint64_t number = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            number = number * 10 + static_cast<uint64_t>(*cursor - '0');
            if (number > 0xFFFFFFFFull) {
                return error("limit is too large");
            }
            cursor++;
        }
        value = static_cast<uint32_t>(number);
        present = true;

        return true;
    }

    bool parse_sequence(std::vector<Node> &nodes, char terminator)
    {
        for (;;) {
            while (*cursor == ' ') {
                cursor++;
            }
            char letter = *cursor;
            if (letter == terminator) {
                if (terminator != '\0') {
                    cursor++;
                }
                return true;
            }
            if (letter == '\0') {
                return error("group is not closed");
            }
            cursor++;
            if (letter == '[' || letter == '(') {
                Node group{letter == '[' ? Kind::array : Kind::optional, no_limit, {}};
                bool present;
                if (letter == '[' && !parse_number(group.limit, present)) {
                    return false;
                }
                if (!parse_sequence(group.children, letter == '[' ? ']' : ')')) {
                    return false;
                }
                if (group.children.empty()) {
                    return error("group is empty");
                }
                for (const Node &child : group.children) {
                    if (child.kind == Kind::rest) {
                        return error("the rest field cannot sit inside a group");
                    }
                }
                nodes.push_back(std::move(group));
                continue;
            }
            Node node{Kind::boolean, no_limit, {}};
            if (!classify(letter, node)) {
                return false;
            }
            uint32_t limit;
            bool present;
            if (!parse_number(limit, present)) {
                return false;
            }
            if (present) {
                if (node.kind != Kind::string && node.kind != Kind::raw && node.kind != Kind::byte_array) {
                    return error("only strings, raw bytes, byte arrays and arrays take a limit");
                }
                if (limit == 0 && node.kind != Kind::raw) {
                    return error("limit must be at least 1");
                }
                node.limit = limit;
            } else if (node.kind == Kind::raw) {
                return error("raw bytes need a size");
            }
            nodes.push_back(std::move(node));
        }
    }

    bool classify(char letter, Node &node)
    {
        switch (letter) {
        case '?':
            node.kind = Kind::boolean;
            return true;
        case 'b':
            node.kind = Kind::i8;
            return true;
        case 'B':
            node.kind = Kind::u8;
            return true;
        case 'h':
            node.kind = Kind::i16;
            return true;
        case 'H':
            node.kind = Kind::u16;
            return true;
        case 'i':
            node.kind = Kind::i32;
            return true;
        case 'I':
            node.kind = Kind::u32;
            return true;
        case 'q':
            node.kind = Kind::i64;
            return true;
        case 'Q':
            node.kind = Kind::u64;
            return true;
        case 'f':
            node.kind = Kind::f32;
            return true;
        case 'd':
            node.kind = Kind::f64;
            return true;
        case 'v':
            node.kind = Kind::varint;
            return true;
        case 'V':
            node.kind = Kind::varlong;
            return true;
        case 'u':
            node.kind = Kind::uvarint;
            return true;
        case 'U':
            node.kind = Kind::uvarlong;
            return true;
        case 'z':
            node.kind = Kind::zigzag32;
            return true;
        case 'Z':
            node.kind = Kind::zigzag64;
            return true;
        case 's':
            node.kind = Kind::string;
            node.limit = minecraft::max_string_chars;
            return true;
        case 'g':
            node.kind = Kind::uuid;
            return true;
        case 'p':
            node.kind = Kind::position;
            return true;
        case 'n':
            node.kind = Kind::nbt;
            node.limit = nbt_by_edition;
            return true;
        case 'x':
            node.kind = Kind::raw;
            return true;
        case 'a':
            node.kind = Kind::byte_array;
            return true;
        case 'r':
            node.kind = Kind::rest;
            return true;
        default:
            break;
        }

        return error("unknown field letter");
    }
};

void flatten(const std::vector<Node> &nodes, std::vector<Op> &ops)
{
    size_t base = ops.size();
    for (const Node &node : nodes) {
        ops.push_back(Op{node.kind, node.limit, 0, 0});
    }
    for (size_t i = 0; i < nodes.size(); i++) {
        if (!nodes[i].children.empty()) {
            ops[base + i].child_begin = static_cast<uint32_t>(ops.size());
            ops[base + i].child_count = static_cast<uint32_t>(nodes[i].children.size());
            flatten(nodes[i].children, ops);
        }
    }
}

struct Source
{
    PyObject *keep;
    Py_buffer view;
    const uint8_t *begin;
    size_t size;

    explicit Source(PyObject *data) : keep(nullptr), view(), begin(nullptr), size(0)
    {
        if (PyBytes_CheckExact(data)) {
            keep = Py_NewRef(data);
            begin = reinterpret_cast<const uint8_t *>(PyBytes_AS_STRING(data));
            size = static_cast<size_t>(PyBytes_GET_SIZE(data));
        } else if (PyObject_GetBuffer(data, &view, PyBUF_SIMPLE) == 0) {
            begin = static_cast<const uint8_t *>(view.buf);
            size = static_cast<size_t>(view.len);
        }
    }

    bool ok() const
    {
        return begin != nullptr;
    }

    ~Source()
    {
        if (view.obj != nullptr) {
            PyBuffer_Release(&view);
        }
        Py_XDECREF(keep);
    }
};

bool keyword_little(PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames, PyObject *&little)
{
    if (kwnames == nullptr) {
        return true;
    }
    for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(kwnames); i++) {
        PyObject *name = PyTuple_GET_ITEM(kwnames, i);
        if (PyUnicode_CompareWithASCIIString(name, "little") != 0) {
            PyErr_Format(PyExc_TypeError, "unexpected keyword argument %R", name);
            return false;
        }
        little = args[nargs + i];
    }

    return true;
}

bool truth(PyObject *object, bool &out)
{
    int value = object == nullptr ? 0 : PyObject_IsTrue(object);
    if (value < 0) {
        return false;
    }
    out = value != 0;

    return true;
}

bool attach(ReaderObject *self, PyObject *data)
{
    const uint8_t *begin;
    size_t size;
    if (PyBytes_CheckExact(data)) {
        self->source = Py_NewRef(data);
        begin = reinterpret_cast<const uint8_t *>(PyBytes_AS_STRING(data));
        size = static_cast<size_t>(PyBytes_GET_SIZE(data));
    } else {
        if (PyObject_GetBuffer(data, &self->view, PyBUF_SIMPLE) < 0) {
            return false;
        }
        begin = static_cast<const uint8_t *>(self->view.buf);
        size = static_cast<size_t>(self->view.len);
    }
    new (&self->cursor) Cursor(begin, begin + size);

    return true;
}

void detach(ReaderObject *self)
{
    if (self->view.obj != nullptr) {
        PyBuffer_Release(&self->view);
    }
    Py_CLEAR(self->source);
}

PyObject *build_reader(PyTypeObject *type, PyObject *data, PyObject *little_object)
{
    bool little;
    if (!truth(little_object, little)) {
        return nullptr;
    }
    ReaderObject *self = reinterpret_cast<ReaderObject *>(type->tp_alloc(type, 0));
    if (self == nullptr) {
        return nullptr;
    }
    self->state = static_cast<State *>(PyType_GetModuleState(type));
    self->little = little;
    if (!attach(self, data)) {
        Py_DECREF(self);
        return nullptr;
    }

    return reinterpret_cast<PyObject *>(self);
}

PyObject *reader_vectorcall(PyObject *callable, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (nargs < 1 || nargs > 2) {
        PyErr_Format(PyExc_TypeError, "Reader() takes 1 to 2 positional arguments (%zd given)", nargs);
        return nullptr;
    }
    PyObject *little = nargs == 2 ? args[1] : nullptr;
    if (!keyword_little(args, nargs, kwnames, little)) {
        return nullptr;
    }

    return build_reader(reinterpret_cast<PyTypeObject *>(callable), args[0], little);
}

PyObject *reader_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    static const char *keywords[] = {"", "little", nullptr};
    PyObject *data;
    PyObject *little = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|$O", const_cast<char **>(keywords), &data, &little)) {
        return nullptr;
    }

    return build_reader(type, data, little);
}

void reader_dealloc(PyObject *object)
{
    ReaderObject *self = reinterpret_cast<ReaderObject *>(object);
    PyTypeObject *type = Py_TYPE(object);
    detach(self);
    type->tp_free(object);
    Py_DECREF(type);
}

PyObject *reader_scalar(PyObject *object, Kind kind, uint32_t limit)
{
    ReaderObject *self = reinterpret_cast<ReaderObject *>(object);
    Op op{kind, limit, 0, 0};

    return read_scalar(self->state, self->cursor, self->little, op);
}

PyObject *reader_boolean(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::boolean, no_limit);
}

PyObject *reader_i8(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::i8, no_limit);
}

PyObject *reader_u8(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::u8, no_limit);
}

PyObject *reader_i16(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::i16, no_limit);
}

PyObject *reader_u16(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::u16, no_limit);
}

PyObject *reader_i32(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::i32, no_limit);
}

PyObject *reader_u32(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::u32, no_limit);
}

PyObject *reader_i64(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::i64, no_limit);
}

PyObject *reader_u64(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::u64, no_limit);
}

PyObject *reader_f32(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::f32, no_limit);
}

PyObject *reader_f64(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::f64, no_limit);
}

PyObject *reader_varint(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::varint, no_limit);
}

PyObject *reader_varlong(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::varlong, no_limit);
}

PyObject *reader_uvarint(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::uvarint, no_limit);
}

PyObject *reader_uvarlong(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::uvarlong, no_limit);
}

PyObject *reader_zigzag(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::zigzag32, no_limit);
}

PyObject *reader_zigzag64(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::zigzag64, no_limit);
}

PyObject *reader_uuid(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::uuid, no_limit);
}

PyObject *reader_position(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::position, no_limit);
}

PyObject *reader_rest(PyObject *object, PyObject *)
{
    return reader_scalar(object, Kind::rest, no_limit);
}

bool optional_limit(PyObject *const *args, Py_ssize_t nargs, const char *method, uint32_t fallback, uint32_t &out)
{
    if (nargs > 1) {
        PyErr_Format(PyExc_TypeError, "%s() takes at most 1 positional argument (%zd given)", method, nargs);
        return false;
    }
    out = fallback;
    if (nargs == 1) {
        long value = PyLong_AsLong(args[0]);
        if (value == -1 && PyErr_Occurred()) {
            return false;
        }
        if (value < 1 || value > static_cast<long>(minecraft::max_string_chars)) {
            PyErr_Format(PyExc_ValueError, "%s() limit must be between 1 and %u", method, minecraft::max_string_chars);
            return false;
        }
        out = static_cast<uint32_t>(value);
    }

    return true;
}

PyObject *reader_string(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    uint32_t limit;
    if (!optional_limit(args, nargs, "string", minecraft::max_string_chars, limit)) {
        return nullptr;
    }

    return reader_scalar(object, Kind::string, limit);
}

bool nbt_mode(PyObject *const *args, Py_ssize_t nargs, const char *method, uint32_t &mode)
{
    if (nargs > 1) {
        PyErr_Format(PyExc_TypeError, "%s() takes at most 1 positional argument (%zd given)", method, nargs);
        return false;
    }
    mode = nbt_by_edition;
    if (nargs == 1 && args[0] != Py_None) {
        bool named;
        if (!truth(args[0], named)) {
            return false;
        }
        mode = named ? nbt_named : nbt_unnamed;
    }

    return true;
}

PyObject *reader_nbt(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    uint32_t mode;
    if (!nbt_mode(args, nargs, "nbt", mode)) {
        return nullptr;
    }

    return reader_scalar(object, Kind::nbt, mode);
}

bool byte_count(PyObject *object, Py_ssize_t &count)
{
    count = PyLong_AsSsize_t(object);
    if (count == -1 && PyErr_Occurred()) {
        return false;
    }
    if (count < 0) {
        PyErr_SetString(PyExc_ValueError, "byte count cannot be negative");
        return false;
    }

    return true;
}

PyObject *reader_raw(PyObject *object, PyObject *count_object)
{
    Py_ssize_t count;
    if (!byte_count(count_object, count)) {
        return nullptr;
    }
    if (static_cast<uint64_t>(count) > 0xFFFFFFFFull) {
        PyErr_SetString(PyExc_OverflowError, "byte count is too large");
        return nullptr;
    }

    return reader_scalar(object, Kind::raw, static_cast<uint32_t>(count));
}

PyObject *reader_byte_array(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    uint32_t limit = no_limit;
    if (nargs > 1) {
        PyErr_Format(PyExc_TypeError, "byte_array() takes at most 1 positional argument (%zd given)", nargs);
        return nullptr;
    }
    if (nargs == 1) {
        Py_ssize_t count;
        if (!byte_count(args[0], count)) {
            return nullptr;
        }
        limit = static_cast<uint64_t>(count) > 0xFFFFFFFFull ? no_limit : static_cast<uint32_t>(count);
    }

    return reader_scalar(object, Kind::byte_array, limit);
}

PyObject *reader_skip(PyObject *object, PyObject *count_object)
{
    ReaderObject *self = reinterpret_cast<ReaderObject *>(object);
    Py_ssize_t count;
    if (!byte_count(count_object, count)) {
        return nullptr;
    }
    if (!self->cursor.skip(static_cast<size_t>(count))) {
        Op op{Kind::raw, 0, 0, 0};
        return read_failure(self->state, self->cursor, op);
    }
    Py_RETURN_NONE;
}

PyObject *reader_reset(PyObject *object, PyObject *data)
{
    ReaderObject *self = reinterpret_cast<ReaderObject *>(object);
    detach(self);
    if (!attach(self, data)) {
        new (&self->cursor) Cursor(nullptr, nullptr);
        return nullptr;
    }
    Py_RETURN_NONE;
}

PyObject *reader_remaining(PyObject *object, void *)
{
    return PyLong_FromSize_t(reinterpret_cast<ReaderObject *>(object)->cursor.remaining());
}

PyObject *reader_offset(PyObject *object, void *)
{
    return PyLong_FromSize_t(reinterpret_cast<ReaderObject *>(object)->cursor.offset());
}

PyObject *reader_little(PyObject *object, void *)
{
    return PyBool_FromLong(reinterpret_cast<ReaderObject *>(object)->little);
}

PyDoc_STRVAR(reader_boolean_doc, "boolean()\n--\n\nReads a boolean stored in one byte.");
PyDoc_STRVAR(reader_i8_doc, "i8()\n--\n\nReads a signed 8-bit integer.");
PyDoc_STRVAR(reader_u8_doc, "u8()\n--\n\nReads an unsigned 8-bit integer.");
PyDoc_STRVAR(reader_i16_doc, "i16()\n--\n\nReads a signed 16-bit integer.");
PyDoc_STRVAR(reader_u16_doc, "u16()\n--\n\nReads an unsigned 16-bit integer.");
PyDoc_STRVAR(reader_i32_doc, "i32()\n--\n\nReads a signed 32-bit integer.");
PyDoc_STRVAR(reader_u32_doc, "u32()\n--\n\nReads an unsigned 32-bit integer.");
PyDoc_STRVAR(reader_i64_doc, "i64()\n--\n\nReads a signed 64-bit integer.");
PyDoc_STRVAR(reader_u64_doc, "u64()\n--\n\nReads an unsigned 64-bit integer.");
PyDoc_STRVAR(reader_f32_doc, "f32()\n--\n\nReads a 32-bit float.");
PyDoc_STRVAR(reader_f64_doc, "f64()\n--\n\nReads a 64-bit double.");
PyDoc_STRVAR(reader_varint_doc, "varint()\n--\n\nReads a Java varint as a signed 32-bit integer.");
PyDoc_STRVAR(reader_varlong_doc, "varlong()\n--\n\nReads a Java varlong as a signed 64-bit integer.");
PyDoc_STRVAR(reader_uvarint_doc, "uvarint()\n--\n\nReads an unsigned varint, the Bedrock form for counts and ids.");
PyDoc_STRVAR(reader_uvarlong_doc, "uvarlong()\n--\n\nReads an unsigned varlong.");
PyDoc_STRVAR(reader_zigzag_doc, "zigzag()\n--\n\nReads a zigzag varint, the Bedrock form for signed 32-bit values.");
PyDoc_STRVAR(reader_zigzag64_doc, "zigzag64()\n--\n\nReads a zigzag varlong, the Bedrock form for signed 64-bit values.");
PyDoc_STRVAR(reader_string_doc,
"string(limit=32767, /)\n"
"--\n"
"\n"
"Reads a UTF-8 string with a varint length prefix.\n"
"\n"
"Rejects strings longer than ``limit`` characters the way vanilla does,\n"
"before the bytes are decoded.");
PyDoc_STRVAR(reader_uuid_doc, "uuid()\n--\n\nReads a UUID from 16 bytes.");
PyDoc_STRVAR(reader_position_doc, "position()\n--\n\nReads a block position from its packed 64-bit form.");
PyDoc_STRVAR(reader_nbt_doc,
"nbt(named=None, /)\n"
"--\n"
"\n"
"Reads an NBT value.\n"
"\n"
"Java sends an unnamed root and Bedrock a named one; pass ``named`` to\n"
"override that for older versions.");
PyDoc_STRVAR(reader_raw_doc, "raw(count, /)\n--\n\nReads exactly ``count`` bytes.");
PyDoc_STRVAR(reader_byte_array_doc,
"byte_array(limit=None, /)\n"
"--\n"
"\n"
"Reads a byte array with a varint length prefix, at most ``limit`` bytes long.");
PyDoc_STRVAR(reader_rest_doc, "rest()\n--\n\nReads every byte that is left.");
PyDoc_STRVAR(reader_skip_doc, "skip(count, /)\n--\n\nMoves past ``count`` bytes without reading them.");
PyDoc_STRVAR(reader_reset_doc, "reset(data, /)\n--\n\nStarts over on new data, so one reader can serve packet after packet.");

PyMethodDef reader_methods[] = {
    {"boolean", reader_boolean, METH_NOARGS, reader_boolean_doc},
    {"i8", reader_i8, METH_NOARGS, reader_i8_doc},
    {"u8", reader_u8, METH_NOARGS, reader_u8_doc},
    {"i16", reader_i16, METH_NOARGS, reader_i16_doc},
    {"u16", reader_u16, METH_NOARGS, reader_u16_doc},
    {"i32", reader_i32, METH_NOARGS, reader_i32_doc},
    {"u32", reader_u32, METH_NOARGS, reader_u32_doc},
    {"i64", reader_i64, METH_NOARGS, reader_i64_doc},
    {"u64", reader_u64, METH_NOARGS, reader_u64_doc},
    {"f32", reader_f32, METH_NOARGS, reader_f32_doc},
    {"f64", reader_f64, METH_NOARGS, reader_f64_doc},
    {"varint", reader_varint, METH_NOARGS, reader_varint_doc},
    {"varlong", reader_varlong, METH_NOARGS, reader_varlong_doc},
    {"uvarint", reader_uvarint, METH_NOARGS, reader_uvarint_doc},
    {"uvarlong", reader_uvarlong, METH_NOARGS, reader_uvarlong_doc},
    {"zigzag", reader_zigzag, METH_NOARGS, reader_zigzag_doc},
    {"zigzag64", reader_zigzag64, METH_NOARGS, reader_zigzag64_doc},
    {"string", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(reader_string)), METH_FASTCALL, reader_string_doc},
    {"uuid", reader_uuid, METH_NOARGS, reader_uuid_doc},
    {"position", reader_position, METH_NOARGS, reader_position_doc},
    {"nbt", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(reader_nbt)), METH_FASTCALL, reader_nbt_doc},
    {"raw", reader_raw, METH_O, reader_raw_doc},
    {"byte_array", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(reader_byte_array)), METH_FASTCALL, reader_byte_array_doc},
    {"rest", reader_rest, METH_NOARGS, reader_rest_doc},
    {"skip", reader_skip, METH_O, reader_skip_doc},
    {"reset", reader_reset, METH_O, reader_reset_doc},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef reader_getset[] = {
    {"remaining", reader_remaining, nullptr, nullptr, nullptr},
    {"offset", reader_offset, nullptr, nullptr, nullptr},
    {"little", reader_little, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot reader_slots[] = {
    {Py_tp_doc, const_cast<char *>(reader_doc)},
    {Py_tp_new, reinterpret_cast<void *>(reader_new)},
    {Py_tp_vectorcall, reinterpret_cast<void *>(reader_vectorcall)},
    {Py_tp_dealloc, reinterpret_cast<void *>(reader_dealloc)},
    {Py_tp_methods, reader_methods},
    {Py_tp_getset, reader_getset},
    {0, nullptr}
};

PyType_Spec reader_spec = {
    "minecraft.net.buffer.Reader",
    sizeof(ReaderObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    reader_slots
};

PyObject *build_writer(PyTypeObject *type, PyObject *little_object)
{
    bool little;
    if (!truth(little_object, little)) {
        return nullptr;
    }
    WriterObject *self = reinterpret_cast<WriterObject *>(type->tp_alloc(type, 0));
    if (self == nullptr) {
        return nullptr;
    }
    self->state = static_cast<State *>(PyType_GetModuleState(type));
    self->little = little;
    new (&self->storage) std::vector<uint8_t>();
    self->storage.reserve(128);

    return reinterpret_cast<PyObject *>(self);
}

PyObject *writer_vectorcall(PyObject *callable, PyObject *const *args, size_t nargsf, PyObject *kwnames)
{
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    if (nargs != 0) {
        PyErr_Format(PyExc_TypeError, "Writer() takes no positional arguments (%zd given)", nargs);
        return nullptr;
    }
    PyObject *little = nullptr;
    if (!keyword_little(args, nargs, kwnames, little)) {
        return nullptr;
    }

    return build_writer(reinterpret_cast<PyTypeObject *>(callable), little);
}

PyObject *writer_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    static const char *keywords[] = {"little", nullptr};
    PyObject *little = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|$O", const_cast<char **>(keywords), &little)) {
        return nullptr;
    }

    return build_writer(type, little);
}

void writer_dealloc(PyObject *object)
{
    WriterObject *self = reinterpret_cast<WriterObject *>(object);
    PyTypeObject *type = Py_TYPE(object);
    self->storage.~vector();
    type->tp_free(object);
    Py_DECREF(type);
}

PyObject *writer_scalar(PyObject *object, Kind kind, uint32_t limit, PyObject *value)
{
    WriterObject *self = reinterpret_cast<WriterObject *>(object);
    Op op{kind, limit, 0, 0};
    if (!write_scalar(self->state, self->storage, self->little, op, value)) {
        return nullptr;
    }
    Py_RETURN_NONE;
}

PyObject *writer_boolean(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::boolean, no_limit, value);
}

PyObject *writer_i8(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::i8, no_limit, value);
}

PyObject *writer_u8(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::u8, no_limit, value);
}

PyObject *writer_i16(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::i16, no_limit, value);
}

PyObject *writer_u16(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::u16, no_limit, value);
}

PyObject *writer_i32(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::i32, no_limit, value);
}

PyObject *writer_u32(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::u32, no_limit, value);
}

PyObject *writer_i64(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::i64, no_limit, value);
}

PyObject *writer_u64(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::u64, no_limit, value);
}

PyObject *writer_f32(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::f32, no_limit, value);
}

PyObject *writer_f64(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::f64, no_limit, value);
}

PyObject *writer_varint(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::varint, no_limit, value);
}

PyObject *writer_varlong(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::varlong, no_limit, value);
}

PyObject *writer_uvarint(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::uvarint, no_limit, value);
}

PyObject *writer_uvarlong(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::uvarlong, no_limit, value);
}

PyObject *writer_zigzag(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::zigzag32, no_limit, value);
}

PyObject *writer_zigzag64(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::zigzag64, no_limit, value);
}

PyObject *writer_uuid(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::uuid, no_limit, value);
}

PyObject *writer_position(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::position, no_limit, value);
}

PyObject *writer_raw(PyObject *object, PyObject *value)
{
    return writer_scalar(object, Kind::rest, no_limit, value);
}

PyObject *writer_byte_array(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs < 1 || nargs > 2) {
        PyErr_Format(PyExc_TypeError, "byte_array() takes 1 to 2 positional arguments (%zd given)", nargs);
        return nullptr;
    }
    uint32_t limit = no_limit;
    if (nargs == 2) {
        Py_ssize_t count;
        if (!byte_count(args[1], count)) {
            return nullptr;
        }
        limit = static_cast<uint64_t>(count) > 0xFFFFFFFFull ? no_limit : static_cast<uint32_t>(count);
    }

    return writer_scalar(object, Kind::byte_array, limit, args[0]);
}

PyObject *writer_string(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs < 1) {
        PyErr_SetString(PyExc_TypeError, "string() needs the string to write");
        return nullptr;
    }
    uint32_t limit;
    if (!optional_limit(args + 1, nargs - 1, "string", minecraft::max_string_chars, limit)) {
        return nullptr;
    }

    return writer_scalar(object, Kind::string, limit, args[0]);
}

PyObject *writer_nbt(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs < 1) {
        PyErr_SetString(PyExc_TypeError, "nbt() needs the value to write");
        return nullptr;
    }
    uint32_t mode;
    if (!nbt_mode(args + 1, nargs - 1, "nbt", mode)) {
        return nullptr;
    }

    return writer_scalar(object, Kind::nbt, mode, args[0]);
}

PyObject *writer_take(PyObject *object, PyObject *)
{
    WriterObject *self = reinterpret_cast<WriterObject *>(object);
    PyObject *result = PyBytes_FromStringAndSize(reinterpret_cast<const char *>(self->storage.data()), static_cast<Py_ssize_t>(self->storage.size()));
    self->storage.clear();

    return result;
}

PyObject *writer_clear(PyObject *object, PyObject *)
{
    reinterpret_cast<WriterObject *>(object)->storage.clear();
    Py_RETURN_NONE;
}

Py_ssize_t writer_length(PyObject *object)
{
    return static_cast<Py_ssize_t>(reinterpret_cast<WriterObject *>(object)->storage.size());
}

PyObject *writer_little(PyObject *object, void *)
{
    return PyBool_FromLong(reinterpret_cast<WriterObject *>(object)->little);
}

PyDoc_STRVAR(writer_boolean_doc, "boolean(value, /)\n--\n\nWrites a boolean as one byte.");
PyDoc_STRVAR(writer_i8_doc, "i8(value, /)\n--\n\nWrites a signed 8-bit integer.");
PyDoc_STRVAR(writer_u8_doc, "u8(value, /)\n--\n\nWrites an unsigned 8-bit integer.");
PyDoc_STRVAR(writer_i16_doc, "i16(value, /)\n--\n\nWrites a signed 16-bit integer.");
PyDoc_STRVAR(writer_u16_doc, "u16(value, /)\n--\n\nWrites an unsigned 16-bit integer.");
PyDoc_STRVAR(writer_i32_doc, "i32(value, /)\n--\n\nWrites a signed 32-bit integer.");
PyDoc_STRVAR(writer_u32_doc, "u32(value, /)\n--\n\nWrites an unsigned 32-bit integer.");
PyDoc_STRVAR(writer_i64_doc, "i64(value, /)\n--\n\nWrites a signed 64-bit integer.");
PyDoc_STRVAR(writer_u64_doc, "u64(value, /)\n--\n\nWrites an unsigned 64-bit integer.");
PyDoc_STRVAR(writer_f32_doc, "f32(value, /)\n--\n\nWrites a 32-bit float.");
PyDoc_STRVAR(writer_f64_doc, "f64(value, /)\n--\n\nWrites a 64-bit double.");
PyDoc_STRVAR(writer_varint_doc, "varint(value, /)\n--\n\nWrites a signed 32-bit integer as a Java varint.");
PyDoc_STRVAR(writer_varlong_doc, "varlong(value, /)\n--\n\nWrites a signed 64-bit integer as a Java varlong.");
PyDoc_STRVAR(writer_uvarint_doc, "uvarint(value, /)\n--\n\nWrites an unsigned varint, the Bedrock form for counts and ids.");
PyDoc_STRVAR(writer_uvarlong_doc, "uvarlong(value, /)\n--\n\nWrites an unsigned varlong.");
PyDoc_STRVAR(writer_zigzag_doc, "zigzag(value, /)\n--\n\nWrites a zigzag varint, the Bedrock form for signed 32-bit values.");
PyDoc_STRVAR(writer_zigzag64_doc, "zigzag64(value, /)\n--\n\nWrites a zigzag varlong, the Bedrock form for signed 64-bit values.");
PyDoc_STRVAR(writer_string_doc,
"string(value, limit=32767, /)\n"
"--\n"
"\n"
"Writes a UTF-8 string with a varint length prefix.\n"
"\n"
"Raises :exc:`ValueError` when the string is longer than ``limit``\n"
"characters, so a packet the client would reject never leaves.");
PyDoc_STRVAR(writer_uuid_doc, "uuid(value, /)\n--\n\nWrites a UUID as 16 bytes.");
PyDoc_STRVAR(writer_position_doc, "position(value, /)\n--\n\nWrites a block position in its packed 64-bit form.");
PyDoc_STRVAR(writer_nbt_doc,
"nbt(value, named=None, /)\n"
"--\n"
"\n"
"Writes an NBT value.\n"
"\n"
"Java gets an unnamed root and Bedrock a named one; pass ``named`` to\n"
"override that for older versions.");
PyDoc_STRVAR(writer_raw_doc, "raw(data, /)\n--\n\nWrites bytes as they are, without a length prefix.");
PyDoc_STRVAR(writer_byte_array_doc,
"byte_array(data, limit=None, /)\n"
"--\n"
"\n"
"Writes a byte array with a varint length prefix, refusing more than ``limit`` bytes.");
PyDoc_STRVAR(writer_take_doc, "take()\n--\n\nReturns everything written so far and empties the writer.");
PyDoc_STRVAR(writer_clear_doc, "clear()\n--\n\nDrops everything written so far.");

PyMethodDef writer_methods[] = {
    {"boolean", writer_boolean, METH_O, writer_boolean_doc},
    {"i8", writer_i8, METH_O, writer_i8_doc},
    {"u8", writer_u8, METH_O, writer_u8_doc},
    {"i16", writer_i16, METH_O, writer_i16_doc},
    {"u16", writer_u16, METH_O, writer_u16_doc},
    {"i32", writer_i32, METH_O, writer_i32_doc},
    {"u32", writer_u32, METH_O, writer_u32_doc},
    {"i64", writer_i64, METH_O, writer_i64_doc},
    {"u64", writer_u64, METH_O, writer_u64_doc},
    {"f32", writer_f32, METH_O, writer_f32_doc},
    {"f64", writer_f64, METH_O, writer_f64_doc},
    {"varint", writer_varint, METH_O, writer_varint_doc},
    {"varlong", writer_varlong, METH_O, writer_varlong_doc},
    {"uvarint", writer_uvarint, METH_O, writer_uvarint_doc},
    {"uvarlong", writer_uvarlong, METH_O, writer_uvarlong_doc},
    {"zigzag", writer_zigzag, METH_O, writer_zigzag_doc},
    {"zigzag64", writer_zigzag64, METH_O, writer_zigzag64_doc},
    {"string", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(writer_string)), METH_FASTCALL, writer_string_doc},
    {"uuid", writer_uuid, METH_O, writer_uuid_doc},
    {"position", writer_position, METH_O, writer_position_doc},
    {"nbt", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(writer_nbt)), METH_FASTCALL, writer_nbt_doc},
    {"raw", writer_raw, METH_O, writer_raw_doc},
    {"byte_array", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(writer_byte_array)), METH_FASTCALL, writer_byte_array_doc},
    {"take", writer_take, METH_NOARGS, writer_take_doc},
    {"clear", writer_clear, METH_NOARGS, writer_clear_doc},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef writer_getset[] = {
    {"little", writer_little, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot writer_slots[] = {
    {Py_tp_doc, const_cast<char *>(writer_doc)},
    {Py_tp_new, reinterpret_cast<void *>(writer_new)},
    {Py_tp_vectorcall, reinterpret_cast<void *>(writer_vectorcall)},
    {Py_tp_dealloc, reinterpret_cast<void *>(writer_dealloc)},
    {Py_sq_length, reinterpret_cast<void *>(writer_length)},
    {Py_tp_methods, writer_methods},
    {Py_tp_getset, writer_getset},
    {0, nullptr}
};

PyType_Spec writer_spec = {
    "minecraft.net.buffer.Writer",
    sizeof(WriterObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    writer_slots
};

PyObject *codec_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    static const char *keywords[] = {"", "little", nullptr};
    PyObject *spec;
    PyObject *little_object = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "U|$O", const_cast<char **>(keywords), &spec, &little_object)) {
        return nullptr;
    }
    bool little;
    if (!truth(little_object, little)) {
        return nullptr;
    }
    const char *text = PyUnicode_AsUTF8(spec);
    if (text == nullptr) {
        return nullptr;
    }

    std::vector<Node> nodes;
    SpecParser parser(text);
    if (!parser.parse(nodes)) {
        return nullptr;
    }

    CodecObject *self = reinterpret_cast<CodecObject *>(type->tp_alloc(type, 0));
    if (self == nullptr) {
        return nullptr;
    }
    self->state = static_cast<State *>(PyType_GetModuleState(type));
    self->spec = Py_NewRef(spec);
    self->fields = static_cast<Py_ssize_t>(nodes.size());
    self->little = little;
    new (&self->ops) std::vector<Op>();
    flatten(nodes, self->ops);

    return reinterpret_cast<PyObject *>(self);
}

void codec_dealloc(PyObject *object)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    PyTypeObject *type = Py_TYPE(object);
    self->ops.~vector();
    Py_XDECREF(self->spec);
    type->tp_free(object);
    Py_DECREF(type);
}

PyObject *codec_repr(PyObject *object)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    if (self->little) {
        return PyUnicode_FromFormat("Codec(%R, little=True)", self->spec);
    }

    return PyUnicode_FromFormat("Codec(%R)", self->spec);
}

PyObject *codec_read_all(CodecObject *self, Cursor &cursor)
{
    PyObject *result = PyTuple_New(self->fields);
    if (result == nullptr) {
        return nullptr;
    }
    for (Py_ssize_t i = 0; i < self->fields; i++) {
        PyObject *item = read_op(self->state, cursor, self->little, self->ops, self->ops[static_cast<size_t>(i)]);
        if (item == nullptr) {
            Py_DECREF(result);
            return nullptr;
        }
        PyTuple_SET_ITEM(result, i, item);
    }

    return result;
}

PyObject *codec_decode(PyObject *object, PyObject *data)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    Source source(data);
    if (!source.ok()) {
        return nullptr;
    }
    Cursor cursor(source.begin, source.begin + source.size);
    PyObject *result = codec_read_all(self, cursor);
    if (result != nullptr && !cursor.exhausted()) {
        Py_DECREF(result);
        PyObject *type = invalid_data_type(self->state);
        if (type != nullptr) {
            PyErr_Format(type, "packet has %zu trailing bytes after the last field", cursor.remaining());
        }
        return nullptr;
    }

    return result;
}

PyObject *codec_read(PyObject *object, PyObject *reader)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    if (!PyObject_TypeCheck(reader, reinterpret_cast<PyTypeObject *>(self->state->reader_type))) {
        PyErr_Format(PyExc_TypeError, "read() needs a Reader, got %.100s", Py_TYPE(reader)->tp_name);
        return nullptr;
    }

    return codec_read_all(self, reinterpret_cast<ReaderObject *>(reader)->cursor);
}

bool codec_write_all(CodecObject *self, std::vector<uint8_t> &storage, PyObject *const *values, Py_ssize_t count)
{
    if (count != self->fields) {
        PyErr_Format(PyExc_TypeError, "codec has %zd fields but %zd values were given", self->fields, count);
        return false;
    }
    for (Py_ssize_t i = 0; i < count; i++) {
        if (!write_op(self->state, storage, self->little, self->ops, self->ops[static_cast<size_t>(i)], values[i])) {
            return false;
        }
    }

    return true;
}

PyObject *codec_encode(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    thread_local std::vector<uint8_t> scratch;
    scratch.clear();
    if (!codec_write_all(self, scratch, args, nargs)) {
        return nullptr;
    }

    return PyBytes_FromStringAndSize(reinterpret_cast<const char *>(scratch.data()), static_cast<Py_ssize_t>(scratch.size()));
}

PyObject *codec_write(PyObject *object, PyObject *const *args, Py_ssize_t nargs)
{
    CodecObject *self = reinterpret_cast<CodecObject *>(object);
    if (nargs < 1 || !PyObject_TypeCheck(args[0], reinterpret_cast<PyTypeObject *>(self->state->writer_type))) {
        PyErr_SetString(PyExc_TypeError, "write() needs a Writer as its first argument");
        return nullptr;
    }
    WriterObject *writer = reinterpret_cast<WriterObject *>(args[0]);
    size_t rollback = writer->storage.size();
    if (!codec_write_all(self, writer->storage, args + 1, nargs - 1)) {
        writer->storage.resize(rollback);
        return nullptr;
    }
    Py_RETURN_NONE;
}

PyObject *codec_spec(PyObject *object, void *)
{
    return Py_NewRef(reinterpret_cast<CodecObject *>(object)->spec);
}

PyObject *codec_fields(PyObject *object, void *)
{
    return PyLong_FromSsize_t(reinterpret_cast<CodecObject *>(object)->fields);
}

PyObject *codec_little(PyObject *object, void *)
{
    return PyBool_FromLong(reinterpret_cast<CodecObject *>(object)->little);
}

PyMethodDef codec_methods[] = {
    {"decode", codec_decode, METH_O, decode_doc},
    {"encode", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(codec_encode)), METH_FASTCALL, encode_doc},
    {"read", codec_read, METH_O, read_doc},
    {"write", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(codec_write)), METH_FASTCALL, write_doc},
    {nullptr, nullptr, 0, nullptr}
};

PyGetSetDef codec_getset[] = {
    {"spec", codec_spec, nullptr, nullptr, nullptr},
    {"fields", codec_fields, nullptr, nullptr, nullptr},
    {"little", codec_little, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}
};

PyType_Slot codec_slots[] = {
    {Py_tp_doc, const_cast<char *>(codec_doc)},
    {Py_tp_new, reinterpret_cast<void *>(codec_new)},
    {Py_tp_dealloc, reinterpret_cast<void *>(codec_dealloc)},
    {Py_tp_repr, reinterpret_cast<void *>(codec_repr)},
    {Py_tp_methods, codec_methods},
    {Py_tp_getset, codec_getset},
    {0, nullptr}
};

PyType_Spec codec_spec_definition = {
    "minecraft.net.buffer.Codec",
    sizeof(CodecObject),
    0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE,
    codec_slots
};

State *state_of(PyObject *module)
{
    return static_cast<State *>(PyModule_GetState(module));
}

int add_type(PyObject *module, PyType_Spec *spec, const char *attribute, PyObject **slot)
{
    *slot = PyType_FromModuleAndSpec(module, spec, nullptr);
    if (*slot == nullptr) {
        return -1;
    }

    return PyModule_AddObjectRef(module, attribute, *slot);
}

int exec(PyObject *module)
{
    State *state = state_of(module);
    state->str_int = PyUnicode_InternFromString("int");
    state->str_is_safe = PyUnicode_InternFromString("is_safe");
    state->empty_string = PyUnicode_InternFromString("");
    if (state->str_int == nullptr || state->str_is_safe == nullptr || state->empty_string == nullptr) {
        return -1;
    }
    if (add_type(module, &reader_spec, "Reader", &state->reader_type) < 0 || add_type(module, &writer_spec, "Writer", &state->writer_type) < 0 || add_type(module, &codec_spec_definition, "Codec", &state->codec_type) < 0) {
        return -1;
    }

    return 0;
}

int traverse(PyObject *module, visitproc visit, void *arg)
{
    State *state = state_of(module);
    Py_VISIT(state->invalid_data);
    Py_VISIT(state->position_type);
    Py_VISIT(state->nbt_module);
    Py_VISIT(state->uuid_type);
    Py_VISIT(state->safe_unknown);
    Py_VISIT(state->reader_type);
    Py_VISIT(state->writer_type);
    Py_VISIT(state->codec_type);

    return 0;
}

int clear(PyObject *module)
{
    State *state = state_of(module);
    Py_CLEAR(state->invalid_data);
    Py_CLEAR(state->position_type);
    Py_CLEAR(state->nbt_module);
    Py_CLEAR(state->uuid_type);
    Py_CLEAR(state->safe_unknown);
    Py_CLEAR(state->str_int);
    Py_CLEAR(state->str_is_safe);
    Py_CLEAR(state->empty_string);
    Py_CLEAR(state->reader_type);
    Py_CLEAR(state->writer_type);
    Py_CLEAR(state->codec_type);

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

PyModuleDef buffer_definition = {
    PyModuleDef_HEAD_INIT,
    "minecraft._native.buffer",
    module_doc,
    sizeof(State),
    nullptr,
    slots,
    traverse,
    clear,
    free_state
};

}
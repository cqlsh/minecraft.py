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

#include <minecraft/bytes.hpp>
#include <minecraft/varint.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace minecraft {

enum class ReadError
{
    none,
    truncated,
    malformed,
    too_long
};

constexpr uint32_t max_string_chars = 32767;

inline size_t utf16_length(std::string_view utf8)
{
    size_t units = 0;
    for (unsigned char byte : utf8) {
        if ((byte & 0xC0) != 0x80) {
            units++;
        }
        if ((byte & 0xF8) == 0xF0) {
            units++;
        }
    }

    return units;
}

class Cursor
{
public:
    Cursor(const uint8_t *begin, const uint8_t *end) : start(begin), cursor(begin), limit(end), failure(ReadError::none)
    {
    }

    const uint8_t *position() const
    {
        return cursor;
    }

    size_t offset() const
    {
        return static_cast<size_t>(cursor - start);
    }

    size_t remaining() const
    {
        return static_cast<size_t>(limit - cursor);
    }

    bool exhausted() const
    {
        return cursor == limit;
    }

    ReadError error() const
    {
        return failure;
    }

    void seek(size_t offset)
    {
        cursor = start + offset;
    }

    bool skip(size_t count)
    {
        if (remaining() < count) {
            return fail(ReadError::truncated);
        }
        cursor += count;

        return true;
    }

    bool read_bytes(size_t count, const uint8_t *&out)
    {
        if (remaining() < count) {
            return fail(ReadError::truncated);
        }
        out = cursor;
        cursor += count;

        return true;
    }

    template <std::endian order, typename T>
    bool read(T &out)
    {
        if (remaining() < sizeof(T)) {
            return fail(ReadError::truncated);
        }
        out = load_ordered<order, T>(cursor);
        cursor += sizeof(T);

        return true;
    }

    template <typename T>
    bool read_big(T &out)
    {
        return read<std::endian::big, T>(out);
    }

    template <typename T>
    bool read_little(T &out)
    {
        return read<std::endian::little, T>(out);
    }

    bool read_varint(uint32_t &out)
    {
        int used = minecraft::read_varint(cursor, limit, out);
        if (used == varint_incomplete) {
            return fail(ReadError::truncated);
        }
        if (used == varint_malformed) {
            return fail(ReadError::malformed);
        }
        cursor += used;

        return true;
    }

    bool read_varlong(uint64_t &out)
    {
        int used = minecraft::read_varlong(cursor, limit, out);
        if (used == varint_incomplete) {
            return fail(ReadError::truncated);
        }
        if (used == varint_malformed) {
            return fail(ReadError::malformed);
        }
        cursor += used;

        return true;
    }

    bool read_zigzag32(int32_t &out)
    {
        uint32_t raw;
        if (!read_varint(raw)) {
            return false;
        }
        out = unzigzag(raw);

        return true;
    }

    bool read_zigzag64(int64_t &out)
    {
        uint64_t raw;
        if (!read_varlong(raw)) {
            return false;
        }
        out = unzigzag(raw);

        return true;
    }

    bool read_java_string(uint32_t max_chars, std::string_view &out)
    {
        uint32_t size;
        if (!read_varint(size)) {
            return false;
        }
        if (size > max_chars * 3) {
            return fail(ReadError::too_long);
        }

        return read_view(size, out);
    }

    bool read_bedrock_string(uint32_t max_bytes, std::string_view &out)
    {
        uint32_t size;
        if (!read_varint(size)) {
            return false;
        }
        if (size > max_bytes) {
            return fail(ReadError::too_long);
        }

        return read_view(size, out);
    }

    bool read_java_array(uint32_t max_count, uint32_t &count)
    {
        if (!read_varint(count)) {
            return false;
        }
        if (count > max_count || count > remaining()) {
            return fail(count > max_count ? ReadError::too_long : ReadError::truncated);
        }

        return true;
    }

private:
    const uint8_t *start;
    const uint8_t *cursor;
    const uint8_t *limit;
    ReadError failure;

    bool fail(ReadError error)
    {
        failure = error;

        return false;
    }

    bool read_view(size_t size, std::string_view &out)
    {
        if (remaining() < size) {
            return fail(ReadError::truncated);
        }
        out = std::string_view(reinterpret_cast<const char *>(cursor), size);
        cursor += size;

        return true;
    }
};

class Sink
{
public:
    explicit Sink(std::vector<uint8_t> &target) : out(target)
    {
    }

    size_t size() const
    {
        return out.size();
    }

    uint8_t *data()
    {
        return out.data();
    }

    void reserve(size_t extra)
    {
        out.reserve(out.size() + extra);
    }

    void write_byte(uint8_t value)
    {
        out.push_back(value);
    }

    void write_bytes(const uint8_t *bytes, size_t count)
    {
        out.insert(out.end(), bytes, bytes + count);
    }

    void write_bytes(std::string_view bytes)
    {
        write_bytes(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    }

    template <std::endian order, typename T>
    void write(T value)
    {
        uint8_t bytes[sizeof(T)];
        store_ordered<order, T>(bytes, value);
        write_bytes(bytes, sizeof(T));
    }

    template <typename T>
    void write_big(T value)
    {
        write<std::endian::big, T>(value);
    }

    template <typename T>
    void write_little(T value)
    {
        write<std::endian::little, T>(value);
    }

    void write_varint(uint32_t value)
    {
        uint8_t bytes[max_varint_size];
        size_t size = minecraft::write_varint(bytes, value);
        write_bytes(bytes, size);
    }

    void write_varlong(uint64_t value)
    {
        uint8_t bytes[max_varlong_size];
        size_t size = minecraft::write_varlong(bytes, value);
        write_bytes(bytes, size);
    }

    void write_zigzag32(int32_t value)
    {
        write_varint(zigzag(value));
    }

    void write_zigzag64(int64_t value)
    {
        write_varlong(zigzag(value));
    }

    void write_java_string(std::string_view utf8)
    {
        write_varint(static_cast<uint32_t>(utf8.size()));
        write_bytes(utf8);
    }

    void write_bedrock_string(std::string_view utf8)
    {
        write_varint(static_cast<uint32_t>(utf8.size()));
        write_bytes(utf8);
    }

    void patch_varint(size_t offset, uint32_t value)
    {
        minecraft::write_varint(out.data() + offset, value);
    }

private:
    std::vector<uint8_t> &out;
};

}
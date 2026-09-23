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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace minecraft::nbt {

enum class TagType : uint8_t
{
    end = 0,
    int8 = 1,
    int16 = 2,
    int32 = 3,
    int64 = 4,
    float32 = 5,
    float64 = 6,
    byte_array = 7,
    string = 8,
    list = 9,
    compound = 10,
    int32_array = 11,
    int64_array = 12
};

enum class Format
{
    java,
    bedrock,
    bedrock_network
};

enum class Error
{
    none,
    truncated,
    bad_tag,
    bad_length,
    bad_string,
    too_deep
};

constexpr int max_depth = 512;
constexpr TagType last_tag = TagType::int64_array;

inline bool is_valid_tag(uint8_t byte)
{
    return byte <= static_cast<uint8_t>(last_tag);
}

inline bool decode_java_string(std::string_view modified, std::string &out)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(modified.data());
    const uint8_t *end = p + modified.size();
    out.clear();
    out.reserve(modified.size());
    while (p < end) {
        uint8_t byte = *p;
        if (byte < 0x80) {
            out.push_back(static_cast<char>(byte));
            p++;
            continue;
        }
        if ((byte & 0xE0) == 0xC0) {
            if (end - p < 2 || (p[1] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t code = (static_cast<uint32_t>(byte & 0x1F) << 6) | (p[1] & 0x3F);
            if (code == 0) {
                out.push_back('\0');
            } else if (code < 0x80) {
                return false;
            } else {
                out.push_back(static_cast<char>(byte));
                out.push_back(static_cast<char>(p[1]));
            }
            p += 2;
            continue;
        }
        if ((byte & 0xF8) == 0xF0) {
            if (end - p < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t scalar = (static_cast<uint32_t>(byte & 0x07) << 18) | (static_cast<uint32_t>(p[1] & 0x3F) << 12) | (static_cast<uint32_t>(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            if (scalar < 0x10000 || scalar > 0x10FFFF) {
                return false;
            }
            out.append(reinterpret_cast<const char *>(p), 4);
            p += 4;
            continue;
        }
        if ((byte & 0xF0) != 0xE0 || end - p < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) {
            return false;
        }
        uint32_t code = (static_cast<uint32_t>(byte & 0x0F) << 12) | (static_cast<uint32_t>(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        if (code < 0x800) {
            return false;
        }
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (end - p < 6 || p[3] != 0xED || (p[4] & 0xF0) != 0xB0 || (p[5] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t low = (static_cast<uint32_t>(p[4] & 0x0F) << 6) | (p[5] & 0x3F);
            uint32_t scalar = 0x10000 + ((code - 0xD800) << 10) + low;
            out.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
            out.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
            p += 6;
            continue;
        }
        if (code >= 0xDC00 && code <= 0xDFFF) {
            return false;
        }
        out.push_back(static_cast<char>(byte));
        out.push_back(static_cast<char>(p[1]));
        out.push_back(static_cast<char>(p[2]));
        p += 3;
    }

    return true;
}

inline bool is_plain_java_string(std::string_view modified)
{
    for (unsigned char byte : modified) {
        if (byte == 0xC0 || byte == 0xED || byte == 0x00) {
            return false;
        }
    }

    return true;
}

inline void encode_java_string(std::string_view utf8, std::string &out)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(utf8.data());
    const uint8_t *end = p + utf8.size();
    out.clear();
    out.reserve(utf8.size() + 8);
    while (p < end) {
        uint8_t byte = *p;
        if (byte == 0) {
            out.push_back(static_cast<char>(0xC0));
            out.push_back(static_cast<char>(0x80));
            p++;
            continue;
        }
        if ((byte & 0xF8) == 0xF0 && end - p >= 4) {
            uint32_t scalar = (static_cast<uint32_t>(byte & 0x07) << 18) | (static_cast<uint32_t>(p[1] & 0x3F) << 12) | (static_cast<uint32_t>(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            uint32_t high = 0xD800 + ((scalar - 0x10000) >> 10);
            uint32_t low = 0xDC00 + ((scalar - 0x10000) & 0x3FF);
            out.push_back(static_cast<char>(0xE0 | (high >> 12)));
            out.push_back(static_cast<char>(0x80 | ((high >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (high & 0x3F)));
            out.push_back(static_cast<char>(0xE0 | (low >> 12)));
            out.push_back(static_cast<char>(0x80 | ((low >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (low & 0x3F)));
            p += 4;
            continue;
        }
        out.push_back(static_cast<char>(byte));
        p++;
    }
}

template <Format format>
class Reader
{
public:
    Reader(const uint8_t *begin, const uint8_t *end) : cursor(begin), limit(end), failure(Error::none)
    {
    }

    const uint8_t *position() const
    {
        return cursor;
    }

    size_t remaining() const
    {
        return static_cast<size_t>(limit - cursor);
    }

    Error error() const
    {
        return failure;
    }

    bool read_type(TagType &out)
    {
        if (remaining() < 1) {
            return fail(Error::truncated);
        }
        if (!is_valid_tag(*cursor)) {
            return fail(Error::bad_tag);
        }
        out = static_cast<TagType>(*cursor++);

        return true;
    }

    bool read_int8(int8_t &out)
    {
        if (remaining() < 1) {
            return fail(Error::truncated);
        }
        out = static_cast<int8_t>(*cursor++);

        return true;
    }

    bool read_int16(int16_t &out)
    {
        return read_fixed<int16_t>(out);
    }

    bool read_int32(int32_t &out)
    {
        if constexpr (format == Format::bedrock_network) {
            uint32_t raw;
            if (!read_varint32(raw)) {
                return false;
            }
            out = unzigzag(raw);

            return true;
        } else {
            return read_fixed<int32_t>(out);
        }
    }

    bool read_int64(int64_t &out)
    {
        if constexpr (format == Format::bedrock_network) {
            uint64_t raw;
            if (!read_varint64(raw)) {
                return false;
            }
            out = unzigzag(raw);

            return true;
        } else {
            return read_fixed<int64_t>(out);
        }
    }

    bool read_float32(float &out)
    {
        return read_fixed<float>(out);
    }

    bool read_float64(double &out)
    {
        return read_fixed<double>(out);
    }

    bool read_length(int32_t &out)
    {
        if (!read_int32(out)) {
            return false;
        }
        if (out < 0) {
            return fail(Error::bad_length);
        }

        return true;
    }

    bool read_string(std::string_view &out)
    {
        size_t size;
        if constexpr (format == Format::bedrock_network) {
            uint32_t raw;
            if (!read_varint32(raw)) {
                return false;
            }
            size = raw;
        } else {
            uint16_t raw;
            if (!read_fixed<uint16_t>(raw)) {
                return false;
            }
            size = raw;
        }
        if (remaining() < size) {
            return fail(Error::truncated);
        }
        out = std::string_view(reinterpret_cast<const char *>(cursor), size);
        cursor += size;

        return true;
    }

    bool read_bytes(size_t count, const uint8_t *&out)
    {
        if (remaining() < count) {
            return fail(Error::truncated);
        }
        out = cursor;
        cursor += count;

        return true;
    }

    bool read_array_header(size_t element_size, int32_t &count, const uint8_t *&data)
    {
        if (!read_length(count)) {
            return false;
        }
        if constexpr (format == Format::bedrock_network) {
            if (element_size > 1) {
                data = cursor;

                return true;
            }
        }

        return read_bytes(static_cast<size_t>(count) * element_size, data);
    }

    bool skip_value(TagType type, int depth)
    {
        if (depth > max_depth) {
            return fail(Error::too_deep);
        }
        switch (type) {
        case TagType::end:
            return fail(Error::bad_tag);
        case TagType::int8:
            return skip_fixed(1);
        case TagType::int16:
            return skip_fixed(2);
        case TagType::int32: {
            int32_t value;
            return read_int32(value);
        }
        case TagType::int64: {
            int64_t value;
            return read_int64(value);
        }
        case TagType::float32:
            return skip_fixed(4);
        case TagType::float64:
            return skip_fixed(8);
        case TagType::byte_array:
            return skip_array(1);
        case TagType::string: {
            std::string_view value;
            return read_string(value);
        }
        case TagType::list: {
            TagType element;
            int32_t count;
            if (!read_type(element) || !read_length(count)) {
                return false;
            }
            if (count > 0 && element == TagType::end) {
                return fail(Error::bad_tag);
            }
            for (int32_t i = 0; i < count; i++) {
                if (!skip_value(element, depth + 1)) {
                    return false;
                }
            }

            return true;
        }
        case TagType::compound: {
            for (;;) {
                TagType child;
                if (!read_type(child)) {
                    return false;
                }
                if (child == TagType::end) {
                    return true;
                }
                std::string_view name;
                if (!read_string(name) || !skip_value(child, depth + 1)) {
                    return false;
                }
            }
        }
        case TagType::int32_array:
            return skip_array(4);
        case TagType::int64_array:
            return skip_array(8);
        }

        return fail(Error::bad_tag);
    }

private:
    const uint8_t *cursor;
    const uint8_t *limit;
    Error failure;

    bool fail(Error error)
    {
        failure = error;

        return false;
    }

    template <typename T>
    bool read_fixed(T &out)
    {
        if (remaining() < sizeof(T)) {
            return fail(Error::truncated);
        }
        if constexpr (format == Format::java) {
            out = load_big<T>(cursor);
        } else {
            out = load_little<T>(cursor);
        }
        cursor += sizeof(T);

        return true;
    }

    bool read_varint32(uint32_t &out)
    {
        int used = read_varint(cursor, limit, out);
        if (used == varint_incomplete) {
            return fail(Error::truncated);
        }
        if (used == varint_malformed) {
            return fail(Error::bad_length);
        }
        cursor += used;

        return true;
    }

    bool read_varint64(uint64_t &out)
    {
        int used = read_varlong(cursor, limit, out);
        if (used == varint_incomplete) {
            return fail(Error::truncated);
        }
        if (used == varint_malformed) {
            return fail(Error::bad_length);
        }
        cursor += used;

        return true;
    }

    bool skip_fixed(size_t count)
    {
        if (remaining() < count) {
            return fail(Error::truncated);
        }
        cursor += count;

        return true;
    }

    bool skip_array(size_t element_size)
    {
        int32_t count;
        if (!read_length(count)) {
            return false;
        }
        if constexpr (format == Format::bedrock_network) {
            if (element_size > 1) {
                for (int32_t i = 0; i < count; i++) {
                    if (element_size == 4) {
                        int32_t value;
                        if (!read_int32(value)) {
                            return false;
                        }
                    } else {
                        int64_t value;
                        if (!read_int64(value)) {
                            return false;
                        }
                    }
                }

                return true;
            }
        }

        return skip_fixed(static_cast<size_t>(count) * element_size);
    }
};

template <Format format>
class Writer
{
public:
    explicit Writer(std::vector<uint8_t> &target) : out(target)
    {
    }

    void write_type(TagType type)
    {
        out.push_back(static_cast<uint8_t>(type));
    }

    void write_int8(int8_t value)
    {
        out.push_back(static_cast<uint8_t>(value));
    }

    void write_int16(int16_t value)
    {
        write_fixed<int16_t>(value);
    }

    void write_int32(int32_t value)
    {
        if constexpr (format == Format::bedrock_network) {
            write_varint32(zigzag(value));
        } else {
            write_fixed<int32_t>(value);
        }
    }

    void write_int64(int64_t value)
    {
        if constexpr (format == Format::bedrock_network) {
            write_varint64(zigzag(value));
        } else {
            write_fixed<int64_t>(value);
        }
    }

    void write_float32(float value)
    {
        write_fixed<float>(value);
    }

    void write_float64(double value)
    {
        write_fixed<double>(value);
    }

    void write_length(int32_t count)
    {
        write_int32(count);
    }

    bool write_string(std::string_view utf8)
    {
        if constexpr (format == Format::java) {
            encode_java_string(utf8, scratch);
            if (scratch.size() > 0xFFFF) {
                return false;
            }
            write_fixed<uint16_t>(static_cast<uint16_t>(scratch.size()));
            out.insert(out.end(), scratch.begin(), scratch.end());
        } else if constexpr (format == Format::bedrock_network) {
            write_varint32(static_cast<uint32_t>(utf8.size()));
            out.insert(out.end(), utf8.begin(), utf8.end());
        } else {
            if (utf8.size() > 0xFFFF) {
                return false;
            }
            write_fixed<uint16_t>(static_cast<uint16_t>(utf8.size()));
            out.insert(out.end(), utf8.begin(), utf8.end());
        }

        return true;
    }

    void write_bytes(const uint8_t *data, size_t count)
    {
        out.insert(out.end(), data, data + count);
    }

    void write_int32_array(const int32_t *values, int32_t count)
    {
        write_length(count);
        for (int32_t i = 0; i < count; i++) {
            write_int32(values[i]);
        }
    }

    void write_int64_array(const int64_t *values, int32_t count)
    {
        write_length(count);
        for (int32_t i = 0; i < count; i++) {
            write_int64(values[i]);
        }
    }

private:
    std::vector<uint8_t> &out;
    std::string scratch;

    template <typename T>
    void write_fixed(T value)
    {
        uint8_t bytes[sizeof(T)];
        if constexpr (format == Format::java) {
            store_big<T>(bytes, value);
        } else {
            store_little<T>(bytes, value);
        }
        out.insert(out.end(), bytes, bytes + sizeof(T));
    }

    void write_varint32(uint32_t value)
    {
        uint8_t bytes[max_varint_size];
        size_t size = write_varint(bytes, value);
        out.insert(out.end(), bytes, bytes + size);
    }

    void write_varint64(uint64_t value)
    {
        uint8_t bytes[max_varlong_size];
        size_t size = write_varlong(bytes, value);
        out.insert(out.end(), bytes, bytes + size);
    }
};

}
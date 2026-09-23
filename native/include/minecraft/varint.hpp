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

#include <cstddef>
#include <cstdint>

namespace minecraft {

constexpr size_t max_varint_size = 5;
constexpr size_t max_varlong_size = 10;
constexpr int varint_incomplete = 0;
constexpr int varint_malformed = -1;

inline size_t varint_size(uint32_t value)
{
    if (value < 0x80) {
        return 1;
    }
    if (value < 0x4000) {
        return 2;
    }
    if (value < 0x200000) {
        return 3;
    }
    if (value < 0x10000000) {
        return 4;
    }

    return 5;
}

inline size_t varlong_size(uint64_t value)
{
    size_t size = 1;
    while (value >= 0x80) {
        value >>= 7;
        size++;
    }

    return size;
}

inline size_t write_varint(uint8_t *out, uint32_t value)
{
    if (value < 0x80) {
        out[0] = static_cast<uint8_t>(value);
        return 1;
    }
    if (value < 0x4000) {
        out[0] = static_cast<uint8_t>(value | 0x80);
        out[1] = static_cast<uint8_t>(value >> 7);
        return 2;
    }
    if (value < 0x200000) {
        out[0] = static_cast<uint8_t>(value | 0x80);
        out[1] = static_cast<uint8_t>((value >> 7) | 0x80);
        out[2] = static_cast<uint8_t>(value >> 14);
        return 3;
    }
    if (value < 0x10000000) {
        out[0] = static_cast<uint8_t>(value | 0x80);
        out[1] = static_cast<uint8_t>((value >> 7) | 0x80);
        out[2] = static_cast<uint8_t>((value >> 14) | 0x80);
        out[3] = static_cast<uint8_t>(value >> 21);
        return 4;
    }

    out[0] = static_cast<uint8_t>(value | 0x80);
    out[1] = static_cast<uint8_t>((value >> 7) | 0x80);
    out[2] = static_cast<uint8_t>((value >> 14) | 0x80);
    out[3] = static_cast<uint8_t>((value >> 21) | 0x80);
    out[4] = static_cast<uint8_t>(value >> 28);

    return 5;
}

inline size_t write_varlong(uint8_t *out, uint64_t value)
{
    size_t written = 0;
    while (value >= 0x80) {
        out[written++] = static_cast<uint8_t>(value | 0x80);
        value >>= 7;
    }
    out[written++] = static_cast<uint8_t>(value);

    return written;
}

template <bool bounded>
inline int read_varint_bytes(const uint8_t *begin, const uint8_t *end, uint32_t &value)
{
    if constexpr (bounded) {
        if (begin == end) {
            return varint_incomplete;
        }
    }
    uint32_t byte = begin[0];
    if (byte < 0x80) {
        value = byte;
        return 1;
    }

    uint32_t result = byte & 0x7F;
    if constexpr (bounded) {
        if (end - begin < 2) {
            return varint_incomplete;
        }
    }
    byte = begin[1];
    result |= (byte & 0x7F) << 7;
    if (byte < 0x80) {
        value = result;
        return 2;
    }

    if constexpr (bounded) {
        if (end - begin < 3) {
            return varint_incomplete;
        }
    }
    byte = begin[2];
    result |= (byte & 0x7F) << 14;
    if (byte < 0x80) {
        value = result;
        return 3;
    }

    if constexpr (bounded) {
        if (end - begin < 4) {
            return varint_incomplete;
        }
    }
    byte = begin[3];
    result |= (byte & 0x7F) << 21;
    if (byte < 0x80) {
        value = result;
        return 4;
    }

    if constexpr (bounded) {
        if (end - begin < 5) {
            return varint_incomplete;
        }
    }
    byte = begin[4];
    if (byte >= 0x80) {
        return varint_malformed;
    }
    value = result | (byte << 28);

    return 5;
}

inline int read_varint(const uint8_t *begin, const uint8_t *end, uint32_t &value)
{
    if (end - begin >= static_cast<std::ptrdiff_t>(max_varint_size)) {
        return read_varint_bytes<false>(begin, end, value);
    }

    return read_varint_bytes<true>(begin, end, value);
}

inline int read_varlong(const uint8_t *begin, const uint8_t *end, uint64_t &value)
{
    uint64_t result = 0;
    const uint8_t *p = begin;
    for (unsigned shift = 0; shift < 70; shift += 7) {
        if (p == end) {
            return varint_incomplete;
        }
        uint64_t byte = *p++;
        result |= (byte & 0x7F) << shift;
        if (byte < 0x80) {
            value = result;
            return static_cast<int>(p - begin);
        }
    }

    return varint_malformed;
}

inline uint32_t zigzag(int32_t value)
{
    return (static_cast<uint32_t>(value) << 1) ^ static_cast<uint32_t>(value >> 31);
}

inline int32_t unzigzag(uint32_t value)
{
    return static_cast<int32_t>(value >> 1) ^ -static_cast<int32_t>(value & 1);
}

inline uint64_t zigzag(int64_t value)
{
    return (static_cast<uint64_t>(value) << 1) ^ static_cast<uint64_t>(value >> 63);
}

inline int64_t unzigzag(uint64_t value)
{
    return static_cast<int64_t>(value >> 1) ^ -static_cast<int64_t>(value & 1);
}

}
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

#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace minecraft {

inline uint16_t byteswap(uint16_t value)
{
#if defined(_MSC_VER)
    return _byteswap_ushort(value);
#else
    return __builtin_bswap16(value);
#endif
}

inline uint32_t byteswap(uint32_t value)
{
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#else
    return __builtin_bswap32(value);
#endif
}

inline uint64_t byteswap(uint64_t value)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    return __builtin_bswap64(value);
#endif
}

template <typename T>
inline T load_native(const uint8_t *bytes)
{
    T value;
    std::memcpy(&value, bytes, sizeof(T));

    return value;
}

template <typename T>
inline void store_native(uint8_t *bytes, T value)
{
    std::memcpy(bytes, &value, sizeof(T));
}

template <std::endian order, typename T>
inline T load_ordered(const uint8_t *bytes)
{
    if constexpr (std::is_same_v<T, float>) {
        return std::bit_cast<float>(load_ordered<order, uint32_t>(bytes));
    } else if constexpr (std::is_same_v<T, double>) {
        return std::bit_cast<double>(load_ordered<order, uint64_t>(bytes));
    } else {
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned value = load_native<Unsigned>(bytes);
        if constexpr (order != std::endian::native && sizeof(Unsigned) > 1) {
            value = byteswap(value);
        }

        return static_cast<T>(value);
    }
}

template <std::endian order, typename T>
inline void store_ordered(uint8_t *bytes, T value)
{
    if constexpr (std::is_same_v<T, float>) {
        store_ordered<order, uint32_t>(bytes, std::bit_cast<uint32_t>(value));
    } else if constexpr (std::is_same_v<T, double>) {
        store_ordered<order, uint64_t>(bytes, std::bit_cast<uint64_t>(value));
    } else {
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned raw = static_cast<Unsigned>(value);
        if constexpr (order != std::endian::native && sizeof(Unsigned) > 1) {
            raw = byteswap(raw);
        }

        store_native<Unsigned>(bytes, raw);
    }
}

template <typename T>
inline T load_big(const uint8_t *bytes)
{
    return load_ordered<std::endian::big, T>(bytes);
}

template <typename T>
inline T load_little(const uint8_t *bytes)
{
    return load_ordered<std::endian::little, T>(bytes);
}

template <typename T>
inline void store_big(uint8_t *bytes, T value)
{
    store_ordered<std::endian::big, T>(bytes, value);
}

template <typename T>
inline void store_little(uint8_t *bytes, T value)
{
    store_ordered<std::endian::little, T>(bytes, value);
}

}
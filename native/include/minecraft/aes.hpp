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
#include <cstring>

#if defined(_M_X64) || defined(__x86_64__)
#define MINECRAFT_AES_X86 1
#include <emmintrin.h>
#include <wmmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#define MINECRAFT_AES_TARGET
#else
#include <cpuid.h>
#define MINECRAFT_AES_TARGET __attribute__((target("aes,sse2")))
#endif
#else
#define MINECRAFT_AES_X86 0
#define MINECRAFT_AES_TARGET
#endif

namespace minecraft {

constexpr size_t aes_block_size = 16;
constexpr int aes_max_rounds = 14;

namespace aes_detail {

struct Planes
{
    uint16_t p[8];
};

inline void to_planes(const uint8_t *bytes, Planes &out)
{
    for (int bit = 0; bit < 8; bit++) {
        uint16_t word = 0;
        for (int i = 0; i < 16; i++) {
            word |= static_cast<uint16_t>(((bytes[i] >> bit) & 1) << i);
        }
        out.p[bit] = word;
    }
}

inline void from_planes(const Planes &in, uint8_t *bytes)
{
    for (int i = 0; i < 16; i++) {
        uint8_t value = 0;
        for (int bit = 0; bit < 8; bit++) {
            value |= static_cast<uint8_t>(((in.p[bit] >> i) & 1) << bit);
        }
        bytes[i] = value;
    }
}

inline void reduce(uint16_t *c, Planes &out)
{
    for (int k = 14; k >= 8; k--) {
        uint16_t t = c[k];
        c[k - 4] ^= t;
        c[k - 5] ^= t;
        c[k - 7] ^= t;
        c[k - 8] ^= t;
    }
    for (int i = 0; i < 8; i++) {
        out.p[i] = c[i];
    }
}

inline void multiply(const Planes &a, const Planes &b, Planes &out)
{
    uint16_t c[15] = {};
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            c[i + j] ^= a.p[i] & b.p[j];
        }
    }
    reduce(c, out);
}

inline void square(const Planes &a, Planes &out)
{
    uint16_t c[15] = {};
    for (int i = 0; i < 8; i++) {
        c[2 * i] = a.p[i];
    }
    reduce(c, out);
}

inline void invert(const Planes &x, Planes &out)
{
    Planes x2, x3, x6, x12, x15, x30, x60, x120, x126, x252;
    square(x, x2);
    multiply(x2, x, x3);
    square(x3, x6);
    square(x6, x12);
    multiply(x12, x3, x15);
    square(x15, x30);
    square(x30, x60);
    square(x60, x120);
    multiply(x120, x6, x126);
    square(x126, x252);
    multiply(x252, x2, out);
}

inline void sub_bytes(Planes &state)
{
    Planes inverse;
    invert(state, inverse);
    for (int i = 0; i < 8; i++) {
        state.p[i] = inverse.p[i] ^ inverse.p[(i + 4) & 7] ^ inverse.p[(i + 5) & 7] ^ inverse.p[(i + 6) & 7] ^ inverse.p[(i + 7) & 7];
    }
    state.p[0] ^= 0xFFFF;
    state.p[1] ^= 0xFFFF;
    state.p[5] ^= 0xFFFF;
    state.p[6] ^= 0xFFFF;
}

inline uint16_t shift_rows_word(uint16_t word)
{
    uint16_t out = 0;
    for (int row = 0; row < 4; row++) {
        uint16_t mask = static_cast<uint16_t>(0x1111 << row);
        uint16_t bits = word & mask;
        int shift = 4 * row;
        uint16_t rotated = static_cast<uint16_t>((bits >> shift) | (bits << (16 - shift)));
        out |= static_cast<uint16_t>(rotated & mask);
    }

    return out;
}

inline void shift_rows(Planes &state)
{
    for (int i = 0; i < 8; i++) {
        state.p[i] = shift_rows_word(state.p[i]);
    }
}

inline uint16_t rotate_rows(uint16_t word, int by)
{
    uint16_t low = static_cast<uint16_t>(0xFFFF >> (12 + by));
    uint16_t low_mask = static_cast<uint16_t>(low * 0x1111);
    uint16_t high_mask = static_cast<uint16_t>(0xFFFF ^ low_mask);

    return static_cast<uint16_t>(((word >> by) & low_mask) | ((word << (4 - by)) & high_mask));
}

inline void xtime(const Planes &a, Planes &out)
{
    uint16_t high = a.p[7];
    out.p[0] = high;
    out.p[1] = a.p[0] ^ high;
    out.p[2] = a.p[1];
    out.p[3] = a.p[2] ^ high;
    out.p[4] = a.p[3] ^ high;
    out.p[5] = a.p[4];
    out.p[6] = a.p[5];
    out.p[7] = a.p[6];
}

inline void mix_columns(Planes &state)
{
    Planes first, second, third, doubled;
    for (int i = 0; i < 8; i++) {
        first.p[i] = rotate_rows(state.p[i], 1);
        second.p[i] = rotate_rows(state.p[i], 2);
        third.p[i] = rotate_rows(state.p[i], 3);
    }
    Planes sum;
    for (int i = 0; i < 8; i++) {
        sum.p[i] = state.p[i] ^ first.p[i];
    }
    xtime(sum, doubled);
    for (int i = 0; i < 8; i++) {
        state.p[i] = doubled.p[i] ^ first.p[i] ^ second.p[i] ^ third.p[i];
    }
}

inline void add_round_key(Planes &state, const Planes &key)
{
    for (int i = 0; i < 8; i++) {
        state.p[i] ^= key.p[i];
    }
}

inline void sub_word(uint8_t *word)
{
    uint8_t block[16] = {};
    std::memcpy(block, word, 4);
    Planes planes;
    to_planes(block, planes);
    sub_bytes(planes);
    from_planes(planes, block);
    std::memcpy(word, block, 4);
}

inline bool detect_hardware()
{
#if MINECRAFT_AES_X86
#if defined(_MSC_VER)
    int info[4];
    __cpuid(info, 1);
    return (info[2] & (1 << 25)) != 0;
#else
    unsigned eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return (ecx & (1u << 25)) != 0;
#endif
#else
    return false;
#endif
}

#if MINECRAFT_AES_X86
MINECRAFT_AES_TARGET inline void encrypt_hardware(const uint8_t *round_keys, int rounds, const uint8_t *in, uint8_t *out)
{
    __m128i state = _mm_loadu_si128(reinterpret_cast<const __m128i *>(in));
    state = _mm_xor_si128(state, _mm_loadu_si128(reinterpret_cast<const __m128i *>(round_keys)));
    for (int round = 1; round < rounds; round++) {
        state = _mm_aesenc_si128(state, _mm_loadu_si128(reinterpret_cast<const __m128i *>(round_keys + 16 * round)));
    }
    state = _mm_aesenclast_si128(state, _mm_loadu_si128(reinterpret_cast<const __m128i *>(round_keys + 16 * rounds)));
    _mm_storeu_si128(reinterpret_cast<__m128i *>(out), state);
}
#endif

}

inline bool aes_hardware()
{
    static const bool available = aes_detail::detect_hardware();

    return available;
}

class Aes
{
public:
    Aes() : round_count(0), hardware(false)
    {
    }

    bool set_key(const uint8_t *key, size_t size)
    {
        return set_key(key, size, aes_hardware());
    }

    bool set_key(const uint8_t *key, size_t size, bool use_hardware)
    {
        if (size != 16 && size != 24 && size != 32) {
            return false;
        }
        int key_words = static_cast<int>(size / 4);
        round_count = key_words + 6;
        hardware = use_hardware && aes_hardware();
        expand(key, key_words);
        if (!hardware) {
            for (int round = 0; round <= round_count; round++) {
                aes_detail::to_planes(round_keys + 16 * round, sliced_keys[round]);
            }
        }

        return true;
    }

    int rounds() const
    {
        return round_count;
    }

    bool uses_hardware() const
    {
        return hardware;
    }

    void encrypt_block(const uint8_t *in, uint8_t *out) const
    {
#if MINECRAFT_AES_X86
        if (hardware) {
            aes_detail::encrypt_hardware(round_keys, round_count, in, out);
            return;
        }
#endif
        aes_detail::Planes state;
        aes_detail::to_planes(in, state);
        aes_detail::add_round_key(state, sliced_keys[0]);
        for (int round = 1; round < round_count; round++) {
            aes_detail::sub_bytes(state);
            aes_detail::shift_rows(state);
            aes_detail::mix_columns(state);
            aes_detail::add_round_key(state, sliced_keys[round]);
        }
        aes_detail::sub_bytes(state);
        aes_detail::shift_rows(state);
        aes_detail::add_round_key(state, sliced_keys[round_count]);
        aes_detail::from_planes(state, out);
    }

private:
    uint8_t round_keys[(aes_max_rounds + 1) * 16];
    aes_detail::Planes sliced_keys[aes_max_rounds + 1];
    int round_count;
    bool hardware;

    void expand(const uint8_t *key, int key_words)
    {
        static const uint8_t rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};
        int total_words = 4 * (round_count + 1);
        std::memcpy(round_keys, key, static_cast<size_t>(key_words) * 4);
        for (int i = key_words; i < total_words; i++) {
            uint8_t temp[4];
            std::memcpy(temp, round_keys + 4 * (i - 1), 4);
            if (i % key_words == 0) {
                uint8_t first = temp[0];
                temp[0] = temp[1];
                temp[1] = temp[2];
                temp[2] = temp[3];
                temp[3] = first;
                aes_detail::sub_word(temp);
                temp[0] ^= rcon[i / key_words - 1];
            } else if (key_words > 6 && i % key_words == 4) {
                aes_detail::sub_word(temp);
            }
            for (int j = 0; j < 4; j++) {
                round_keys[4 * i + j] = static_cast<uint8_t>(round_keys[4 * (i - key_words) + j] ^ temp[j]);
            }
        }
    }
};

class Cfb8
{
public:
    Cfb8()
    {
        std::memset(shift, 0, sizeof(shift));
    }

    void start(const Aes &block_cipher, const uint8_t *iv)
    {
        cipher = block_cipher;
        std::memcpy(shift, iv, aes_block_size);
    }

    void encrypt(const uint8_t *in, uint8_t *out, size_t size)
    {
        uint8_t block[aes_block_size];
        for (size_t i = 0; i < size; i++) {
            cipher.encrypt_block(shift, block);
            uint8_t encrypted = static_cast<uint8_t>(in[i] ^ block[0]);
            std::memmove(shift, shift + 1, aes_block_size - 1);
            shift[aes_block_size - 1] = encrypted;
            out[i] = encrypted;
        }
    }

    void decrypt(const uint8_t *in, uint8_t *out, size_t size)
    {
        uint8_t block[aes_block_size];
        for (size_t i = 0; i < size; i++) {
            cipher.encrypt_block(shift, block);
            uint8_t encrypted = in[i];
            std::memmove(shift, shift + 1, aes_block_size - 1);
            shift[aes_block_size - 1] = encrypted;
            out[i] = static_cast<uint8_t>(encrypted ^ block[0]);
        }
    }

private:
    Aes cipher;
    uint8_t shift[aes_block_size];
};

}
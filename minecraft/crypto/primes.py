"""
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
"""

from __future__ import annotations

import secrets

from typing import Final

_SMALL_PRIMES: Final = tuple(candidate for candidate in range(3, 2000, 2) if all(candidate % divisor for divisor in range(3, int(candidate ** 0.5) + 1, 2)))

class Primes:
    """
    Finds the large random primes an RSA key is built from.

    Candidates come from the secrets module, are sieved by the primes
    below 2000 and then put through Miller-Rabin with random witnesses,
    which leaves a composite a chance below one in a trillion trillion
    of slipping through.
    """

    @staticmethod
    def is_probable(candidate: int, /, *, rounds: int = 40) -> bool:
        """
        Runs trial division by the small primes and then Miller-Rabin
        with random witnesses.
        """
        for prime in _SMALL_PRIMES:
            if candidate % prime == 0:
                return candidate == prime

        odd = candidate - 1
        doublings = 0

        while odd % 2 == 0:
            odd //= 2
            doublings += 1

        for _ in range(rounds):
            witness = pow(secrets.randbelow(candidate - 3) + 2, odd, candidate)
            if witness == 1 or witness == candidate - 1:
                continue

            for _ in range(doublings - 1):
                witness = witness * witness % candidate
                if witness == candidate - 1:
                    break
            else:
                return False

        return True

    @classmethod
    def random(cls, bits: int, /) -> int:
        """
        Draws random odd numbers with the top two bits set until one of
        them is prime, so a product of two has exactly twice the bits.
        """
        while True:
            candidate = secrets.randbits(bits) | (1 << (bits - 1)) | (1 << (bits - 2)) | 1
            if cls.is_probable(candidate):
                return candidate

__all__ = ["Primes"]
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

import hashlib
import secrets
import hmac
import math

from typing import Final, Literal, Self

from ..errors.base import InvalidData
from .der import DerEncoder, DerReader
from .primes import Primes

type Digest = Literal["sha1", "sha256"]

_RSA_ALGORITHM: Final = bytes.fromhex("300d06092a864886f70d0101010500")
_DIGEST_PREFIXES: Final = {
    "sha1": bytes.fromhex("3021300906052b0e03021a05000414"),
    "sha256": bytes.fromhex("3031300d060960864801650304020105000420")
}

class RsaPublicKey:
    """
    An RSA public key.

    The server sends its own in the encryption request, and Mojang's
    session key, which signs player profile keys, is one as well. Keys
    compare equal when their numbers match.

    Attributes
    -----------
    modulus: :class:`int`
        The modulus n.
    exponent: :class:`int`
        The public exponent e.
    """

    __slots__ = ("modulus", "exponent")

    def __init__(self, modulus: int, *, exponent: int = 65537) -> None:
        if modulus < 3 or modulus % 2 == 0 or not 1 < exponent < modulus:
            raise ValueError("modulus must be odd and larger than the exponent")

        self.modulus = modulus
        self.exponent = exponent

    def __repr__(self) -> str:
        return f"<RsaPublicKey bits={self.modulus.bit_length()}>"

    def __eq__(self, other: object) -> bool:
        return isinstance(other, RsaPublicKey) and self.modulus == other.modulus and self.exponent == other.exponent

    def __hash__(self) -> int:
        return hash((self.modulus, self.exponent))

    @property
    def size(self) -> int:
        """
        :class:`int`: The modulus size in bytes, which is also the size
        of every ciphertext and signature.
        """
        return (self.modulus.bit_length() + 7) // 8

    @classmethod
    def from_der(cls, data: bytes, /) -> Self:
        """
        Parses a key from its X.509 ``SubjectPublicKeyInfo`` encoding,
        the form Java's ``getEncoded`` produces and Mojang publishes its
        session key in.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The bytes are not a DER encoded RSA public key.
        """
        outer = DerReader(data)
        info = outer.sequence()
        if not outer.finished:
            raise InvalidData("public key has trailing bytes")

        info.expect(_RSA_ALGORITHM)
        numbers = DerReader(info.bit_string()).sequence()
        if not info.finished:
            raise InvalidData("public key has trailing bytes")

        modulus = numbers.integer()
        exponent = numbers.integer()
        if not numbers.finished:
            raise InvalidData("public key has trailing bytes")

        try:
            return cls(modulus, exponent=exponent)
        except ValueError as error:
            raise InvalidData(str(error)) from error

    def to_der(self) -> bytes:
        """
        Encodes the key as X.509 ``SubjectPublicKeyInfo``, which the
        encryption request carries so the client can load it with Java's
        ``X509EncodedKeySpec``.
        """
        numbers = DerEncoder.sequence(DerEncoder.integer(self.modulus), DerEncoder.integer(self.exponent))

        return DerEncoder.sequence(_RSA_ALGORITHM, DerEncoder.bit_string(numbers))

    def verify(self, message: bytes, signature: bytes, /, *, digest: Digest = "sha1") -> bool:
        """
        Checks a PKCS#1 v1.5 signature over the message.

        Mojang signs player profile keys with SHA-1, which is why that is
        the default. The comparison takes the same time whether or not
        the signature matches.

        Parameters
        -----------
        message: :class:`bytes`
            The signed bytes.
        signature: :class:`bytes`
            The signature, as long as the modulus.
        digest: :class:`str`
            ``sha1`` or ``sha256``.

        Returns
        --------
        :class:`bool`
            Whether the signature is valid for this key.
        """
        size = self.size
        if len(signature) != size:
            return False

        value = int.from_bytes(signature, "big")
        if value >= self.modulus:
            return False

        decoded = pow(value, self.exponent, self.modulus).to_bytes(size, "big")
        prefix = _DIGEST_PREFIXES[digest]
        hashed = hashlib.new(digest, message).digest()
        expected = b"\x00\x01" + b"\xff" * (size - len(prefix) - len(hashed) - 3) + b"\x00" + prefix + hashed

        return hmac.compare_digest(decoded, expected)

    def encrypt(self, message: bytes, /) -> bytes:
        """
        Encrypts a short message with PKCS#1 v1.5 padding.

        The client side of the login does this to the shared secret and
        the verify token, so tests and proxies can speak the client's
        part.

        Raises
        -------
        ValueError
            The message does not leave room for the padding.
        """
        size = self.size
        if len(message) > size - 11:
            raise ValueError(f"message may hold at most {size - 11} bytes, got {len(message)}")

        padding = bytearray()

        while len(padding) < size - len(message) - 3:
            byte = secrets.randbits(8)
            if byte:
                padding.append(byte)

        block = int.from_bytes(b"\x00\x02" + padding + b"\x00" + message, "big")

        return pow(block, self.exponent, self.modulus).to_bytes(size, "big")

class RsaPrivateKey:
    """
    An RSA private key.

    The server generates one when it starts and keeps it for as long as
    it runs, so every login's shared secret is protected by a key that
    never touches the disk.

    Attributes
    -----------
    public: :class:`RsaPublicKey`
        The matching public key.
    """

    __slots__ = ("public", "_prime1", "_prime2", "_exponent1", "_exponent2", "_coefficient")

    def __init__(self, prime1: int, prime2: int, *, exponent: int = 65537) -> None:
        if prime1 == prime2:
            raise ValueError("the primes must differ")

        order = (prime1 - 1) * (prime2 - 1) // math.gcd(prime1 - 1, prime2 - 1)
        try:
            private = pow(exponent, -1, order)
        except ValueError as error:
            raise ValueError("exponent must be coprime to the primes minus one") from error

        self.public = RsaPublicKey(prime1 * prime2, exponent=exponent)
        self._prime1 = prime1
        self._prime2 = prime2
        self._exponent1 = private % (prime1 - 1)
        self._exponent2 = private % (prime2 - 1)
        self._coefficient = pow(prime2, -1, prime1)

    def __repr__(self) -> str:
        return f"<RsaPrivateKey bits={self.public.modulus.bit_length()}>"

    @classmethod
    def generate(cls, *, bits: int = 2048) -> Self:
        """
        Generates a fresh key pair.

        Parameters
        -----------
        bits: :class:`int`
            The modulus size, at least 1024. Vanilla uses 1024; 2048
            takes well under a second and is the safer default.

        Raises
        -------
        ValueError
            The size is too small or odd.
        """
        if bits < 1024 or bits % 2:
            raise ValueError(f"bits must be an even number of at least 1024, got {bits}")

        while True:
            prime1 = Primes.random(bits // 2)
            prime2 = Primes.random(bits // 2)
            if prime1 == prime2 or (prime1 * prime2).bit_length() != bits:
                continue

            try:
                return cls(prime1, prime2)
            except ValueError:
                continue

    def decrypt(self, ciphertext: bytes, /) -> bytes:
        """
        Decrypts a PKCS#1 v1.5 ciphertext, such as the client's shared
        secret.

        The ciphertext is blinded with a random factor before the private
        operation, so its timing does not follow the secret, and every
        check on the padding runs to the end before the result is
        decided, so a wrong ciphertext costs the same time as a right one.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The ciphertext has the wrong length or its padding is broken.
        """
        size = self.public.size
        if len(ciphertext) != size:
            raise InvalidData(f"ciphertext must be {size} bytes, got {len(ciphertext)}")

        modulus = self.public.modulus
        value = int.from_bytes(ciphertext, "big")
        if value >= modulus:
            raise InvalidData("ciphertext is larger than the modulus")

        blind, unblind = self._blinding()
        masked = value * pow(blind, self.public.exponent, modulus) % modulus
        part1 = pow(masked, self._exponent1, self._prime1)
        part2 = pow(masked, self._exponent2, self._prime2)
        combined = part2 + (self._coefficient * (part1 - part2) % self._prime1) * self._prime2
        block = (combined * unblind % modulus).to_bytes(size, "big")

        separator = 0

        for index in range(2, size):
            if separator == 0 and block[index] == 0:
                separator = index

        if block[0] | (block[1] ^ 2) or separator < 10:
            raise InvalidData("ciphertext padding is broken")

        return block[separator + 1:]

    def _blinding(self) -> tuple[int, int]:
        """
        Draws a random factor that is invertible modulo n, and its
        inverse, to mask a ciphertext with.
        """
        modulus = self.public.modulus

        while True:
            blind = secrets.randbelow(modulus - 2) + 2
            if math.gcd(blind, modulus) == 1:
                return blind, pow(blind, -1, modulus)

__all__ = ["Digest", "RsaPublicKey", "RsaPrivateKey"]
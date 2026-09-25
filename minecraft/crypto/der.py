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

from ..errors.base import InvalidData

class DerEncoder:
    """
    Encodes the few ASN.1 elements an RSA public key is made of.

    Every method returns the finished bytes of one element, so a key is
    put together from the inside out: the integers, the sequence around
    them, then the bit string that wraps the sequence.
    """

    @staticmethod
    def length(size: int, /) -> bytes:
        """
        Encodes a length in the short form below 128 and the long form
        above it.
        """
        if size < 0x80:
            return size.to_bytes()

        body = size.to_bytes((size.bit_length() + 7) // 8, "big")

        return (0x80 | len(body)).to_bytes() + body

    @classmethod
    def tagged(cls, *, tag: int, content: bytes) -> bytes:
        """
        Wraps content in a tag and its length.
        """
        return tag.to_bytes() + cls.length(len(content)) + content

    @classmethod
    def integer(cls, value: int, /) -> bytes:
        """
        Encodes a non-negative integer, with the leading zero DER wants
        when the top bit is set.
        """
        return cls.tagged(tag=0x02, content=value.to_bytes((value.bit_length() + 8) // 8, "big"))

    @classmethod
    def sequence(cls, *parts: bytes) -> bytes:
        """
        Encodes a sequence of already encoded parts.
        """
        return cls.tagged(tag=0x30, content=b"".join(parts))

    @classmethod
    def bit_string(cls, content: bytes, /) -> bytes:
        """
        Encodes a byte-aligned bit string.
        """
        return cls.tagged(tag=0x03, content=b"\x00" + content)

class DerReader:
    """
    Walks DER data one element at a time.

    Every method reads the next element, checks that it carries the tag
    it should and refuses anything else, so a key that is not shaped
    exactly as expected fails at the first wrong byte instead of being
    read as something it is not.

    Parameters
    -----------
    data: :class:`bytes`
        The DER encoded data.
    position: :class:`int`
        Where reading starts.
    end: Optional[:class:`int`]
        Where reading stops. Defaults to the end of the data.

    Attributes
    -----------
    data: :class:`bytes`
        The DER encoded data.
    position: :class:`int`
        Where the next element starts.
    end: :class:`int`
        Where reading stops.
    """

    __slots__ = ("data", "position", "end")

    def __init__(self, data: bytes, *, position: int = 0, end: int | None = None) -> None:
        self.data = data
        self.position = position
        self.end = len(data) if end is None else end

    @property
    def finished(self) -> bool:
        """
        :class:`bool`: Whether every element has been read.
        """
        return self.position == self.end

    def expect(self, encoded: bytes, /) -> None:
        """
        Skips an element that must appear exactly as given.
        """
        stop = self.position + len(encoded)
        if stop > self.end or self.data[self.position:stop] != encoded:
            raise InvalidData("DER data lacks the expected element")

        self.position = stop

    def sequence(self) -> DerReader:
        """
        Enters a sequence and returns a reader over its content.
        """
        start, stop = self.element(0x30)

        return DerReader(self.data, position=start, end=stop)

    def integer(self) -> int:
        """
        Reads a non-negative integer.
        """
        start, stop = self.element(0x02)
        if start == stop or self.data[start] & 0x80:
            raise InvalidData("DER integer is empty or negative")

        return int.from_bytes(self.data[start:stop], "big")

    def bit_string(self) -> bytes:
        """
        Reads a byte-aligned bit string.
        """
        start, stop = self.element(0x03)
        if start == stop or self.data[start] != 0:
            raise InvalidData("DER bit string is empty or not byte aligned")

        return self.data[start + 1:stop]

    def element(self, tag: int, /) -> tuple[int, int]:
        """
        Reads the header of the next element, checks its tag and returns
        where its content starts and ends.
        """
        if self.position + 2 > self.end or self.data[self.position] != tag:
            raise InvalidData(f"DER element with tag {tag:#04x} expected")

        first = self.data[self.position + 1]
        start = self.position + 2
        size = first

        if first & 0x80:
            count = first & 0x7F
            if count == 0 or count > 4 or start + count > self.end:
                raise InvalidData("DER length is malformed")

            size = int.from_bytes(self.data[start:start + count], "big")
            start += count

        stop = start + size
        if stop > self.end:
            raise InvalidData("DER element runs past its container")

        self.position = stop

        return start, stop

__all__ = ["DerEncoder", "DerReader"]
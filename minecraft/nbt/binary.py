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

from collections.abc import Buffer
from enum import Enum

from .._native import nbt as _native
from .tag import Tag, TagLike

class NbtFormat(Enum):
    """
    The byte layouts NBT comes in.

    Every member reads and writes its own layout, so a plugin picks the
    format once and calls :meth:`load` and :meth:`dump` on it.

    Attributes
    -----------
    java
        Big endian with modified UTF-8 strings, the layout of Java Edition
        files and packets.
    bedrock
        Little endian, the layout Bedrock Edition stores files in.
    bedrock_network
        Little endian with varint numbers and lengths, the layout Bedrock
        Edition sends in packets.
    """

    java = 0
    bedrock = 1
    bedrock_network = 2

    def load(self, data: Buffer, /, *, named: bool = True) -> Tag:
        """
        Reads one NBT value from bytes.

        Files and Bedrock packets carry a named root and Java packets do
        not, which ``named`` decides. The root name itself is dropped, as
        it is empty nearly everywhere.

        Parameters
        -----------
        data: :class:`bytes`
            The encoded NBT, without any compression around it.
        named: :class:`bool`
            Whether the root tag carries a name.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The bytes do not hold exactly one valid tag.

        Returns
        --------
        :data:`~minecraft.nbt.tag.Tag`
            The root value, with typed numbers and plain containers.
        """
        if named:
            return _native.load(data, self._value_, True)[1]

        return _native.load(data, self._value_, False)

    def dump(self, value: TagLike, /, *, name: str | None = "") -> bytes:
        """
        Writes one NBT value as bytes.

        Plain ints become ``Int`` tags, plain floats ``Double`` tags and
        bools ``Byte`` tags; wrap a number in one of the classes from
        :mod:`minecraft.nbt.tag` to pick another width.

        Parameters
        -----------
        value: :data:`~minecraft.nbt.tag.TagLike`
            The value to write.
        name: Optional[:class:`str`]
            The root name. ``None`` writes an unnamed root, as Java packets
            expect.

        Raises
        -------
        TypeError
            The value or something inside it has no NBT form.
        OverflowError
            A number does not fit its tag.
        ValueError
            A string is longer than NBT allows or the value nests too deep.

        Returns
        --------
        :class:`bytes`
            The encoded NBT.
        """
        return _native.dump(value, self._value_, name)

__all__ = ["NbtFormat"]
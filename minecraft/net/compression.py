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

import zlib

from typing import Final

from ..errors.base import InvalidData
from .buffer import Codec

_BODY: Final = Codec("v r")

class Compressor:
    """
    Packs and unpacks packet bodies the way Java compresses them.

    Once the login has set a threshold, every packet body on the wire
    starts with its uncompressed size as a varint followed by a zlib
    stream, or with a zero followed by the plain body when it was
    shorter than the threshold. One compressor serves one connection.

    Parameters
    -----------
    threshold: :class:`int`
        The body size from which packets are compressed. Vanilla uses 256.
    level: :class:`int`
        The zlib level from 0 to 9. Level 1 gets within a few percent of
        level 6 on chunk data at less than half the CPU time.
    limit: :class:`int`
        The largest uncompressed size a peer may declare, 8 MiB like
        vanilla, so a hostile stream cannot inflate without bounds.

    Attributes
    -----------
    threshold: :class:`int`
        The body size from which packets are compressed.
    level: :class:`int`
        The zlib level in use.
    limit: :class:`int`
        The largest uncompressed size accepted from a peer.
    """

    __slots__ = ("threshold", "level", "limit")

    def __init__(self, threshold: int, *, level: int = 1, limit: int = 8388608) -> None:
        if threshold < 0:
            raise ValueError(f"threshold must be at least 0, got {threshold}")

        if not 0 <= level <= 9:
            raise ValueError(f"level must be between 0 and 9, got {level}")

        if limit < threshold:
            raise ValueError(f"limit must be at least the threshold, got {limit}")

        self.threshold = threshold
        self.level = level
        self.limit = limit

    def __repr__(self) -> str:
        return f"<Compressor threshold={self.threshold} level={self.level}>"

    def compress(self, body: bytes, /) -> bytes:
        """
        Wraps one packet body for the wire.

        Bodies below the threshold pass through behind a zero, so the
        peer knows not to inflate them.

        Parameters
        -----------
        body: :class:`bytes`
            The packet id and payload.

        Returns
        --------
        :class:`bytes`
            The size prefix and the compressed or plain body.
        """
        size = len(body)
        if size < self.threshold:
            return b"\x00" + body

        return _BODY.encode(size, zlib.compress(body, self.level))

    def decompress(self, body: bytes, /) -> bytes:
        """
        Unwraps one packet body from the wire.

        The declared size is checked against the threshold and the limit
        before any inflating happens, and the inflated bytes must match it
        exactly, so a peer can neither slip a tiny compressed packet past
        the threshold nor make the server allocate more than it declared.

        Parameters
        -----------
        body: :class:`bytes`
            The size prefix and the compressed or plain body.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The size prefix breaks the rules or the zlib stream does not
            inflate to exactly that size.

        Returns
        --------
        :class:`bytes`
            The packet id and payload.
        """
        declared, data = _BODY.decode(body)
        if declared == 0:
            return data

        if declared < self.threshold:
            raise InvalidData(f"compressed packet declares {declared} bytes, below the threshold of {self.threshold}")

        if declared > self.limit:
            raise InvalidData(f"compressed packet declares {declared} bytes, above the limit of {self.limit}")

        inflater = zlib.decompressobj()
        try:
            inflated = inflater.decompress(data, declared)
        except zlib.error as error:
            raise InvalidData(f"compressed packet does not inflate: {error}") from error

        if len(inflated) != declared or not inflater.eof or inflater.unused_data:
            raise InvalidData(f"compressed packet does not inflate to the declared {declared} bytes")

        return inflated

__all__ = ["Compressor"]
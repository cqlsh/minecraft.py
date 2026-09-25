from collections.abc import Iterator
from typing import Self

class BlockPosition:
    """
    An immutable position of a block in a world, made of three integer
    coordinates.

    Two positions compare equal when all three coordinates match and they
    hash the same, so they work as dictionary keys and in sets. Unpacking
    ``x, y, z = position`` works as well.

    Attributes
    -----------
    x: :class:`int`
        The east-west coordinate.
    y: :class:`int`
        The height. The world decides which values lie inside its build
        limits.
    z: :class:`int`
        The north-south coordinate.
    """

    def __new__(cls, x: int, y: int, z: int, /) -> Self:
        ...

    @property
    def x(self) -> int:
        ...

    @property
    def y(self) -> int:
        ...

    @property
    def z(self) -> int:
        ...

    @property
    def packed(self) -> int:
        """
        :class:`int`: The position as the signed 64-bit long the protocol sends.

        x and z take 26 bits each and y takes 12, so coordinates beyond
        33554432 blocks or 2048 in height wrap around instead of failing.
        """
        ...

    @classmethod
    def unpack(cls, value: int, /) -> Self:
        """
        Builds a position from the packed 64-bit form the protocol uses.

        Only values that came out of :attr:`packed` round-trip. Anything else is
        read as 26 bits of x, 12 bits of y and 26 bits of z without complaint.
        """
        ...

    def offset(self, x: int, y: int, z: int, /) -> Self:
        """
        Returns a new position moved by the given amounts.

        The position itself never changes. Every method that looks like a move
        hands back a new object.
        """
        ...

    def __iter__(self) -> Iterator[int]:
        ...

    def __hash__(self) -> int:
        ...

    def __eq__(self, other: object, /) -> bool:
        ...

    def __ne__(self, other: object, /) -> bool:
        ...
from typing import Literal, overload
from collections.abc import Buffer

from ..nbt.tag import Tag, TagLike

class Byte(int):
    """
    An NBT byte, a signed 8-bit integer.

    Behaves like :class:`int` everywhere and only differs in how it is
    written. Values outside -128 to 127 raise :exc:`OverflowError`.
    """

class Short(int):
    """
    An NBT short, a signed 16-bit integer.

    Behaves like :class:`int` everywhere and only differs in how it is
    written. Values outside -32768 to 32767 raise :exc:`OverflowError`.
    """

class Int(int):
    """
    An NBT int, a signed 32-bit integer.

    Behaves like :class:`int` everywhere and only differs in how it is
    written. A plain :class:`int` is written as this type too, so it is
    only needed to be explicit. Values outside the 32-bit range raise
    :exc:`OverflowError`.
    """

class Long(int):
    """
    An NBT long, a signed 64-bit integer.

    Behaves like :class:`int` everywhere and only differs in how it is
    written. Values outside the 64-bit range raise :exc:`OverflowError`.
    """

class Float(float):
    """
    An NBT float, a single precision number.

    Behaves like :class:`float` everywhere and only differs in how it is
    written: with 32 bits, so at most seven significant digits survive the
    wire. Finite values beyond the single precision range raise
    :exc:`OverflowError`.
    """

class Double(float):
    """
    An NBT double, a double precision number.

    Behaves like :class:`float` everywhere and only differs in how it is
    written. A plain :class:`float` is written as this type too, so it is
    only needed to be explicit.
    """

@overload
def load(data: Buffer, format: int, named: Literal[True], /) -> tuple[str, Tag]:
    """
    Reads one NBT tag from bytes and builds the Python value for it.

    format is 0 for Java, 1 for Bedrock and 2 for Bedrock's network form.
    With named set the root carries a name and the result is a (name, value)
    tuple, otherwise it is the value alone. Anything that does not parse,
    including trailing bytes, raises :exc:`~minecraft.errors.InvalidData`.
    """
    ...

@overload
def load(data: Buffer, format: int, named: Literal[False], /) -> Tag:
    ...

@overload
def load(data: Buffer, format: int, named: bool, /) -> tuple[str, Tag] | Tag:
    ...

def dump(value: TagLike, format: int, name: str | None, /) -> bytes:
    """
    Writes a Python value as one NBT tag and returns the bytes.

    format is 0 for Java, 1 for Bedrock and 2 for Bedrock's network form.
    name is the root name or None for an unnamed root. Plain ints become
    Int, plain floats become Double, bools become Byte, bytes become byte
    arrays and arrays of typecode i or q become int or long arrays. A value
    that has no NBT form raises :exc:`TypeError`.
    """
    ...
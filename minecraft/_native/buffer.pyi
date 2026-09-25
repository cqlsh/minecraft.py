from collections.abc import Buffer
from typing import Any
from uuid import UUID

from ..world.position import BlockPosition
from ..nbt.tag import Tag, TagLike

class Reader:
    """
    Reads packet fields from a bytes-like object, front to back.

    Every method takes the next field off the data and moves on. Fixed-width
    numbers are big endian for Java and little endian for Bedrock, which the
    ``little`` flag decides. Reading past the end, a malformed varint or an
    over-long string raises :exc:`~minecraft.errors.InvalidData`, so a
    connection that sends garbage fails at the first bad field.

    Parameters
    -----------
    data: :class:`bytes`
        The packet payload. Other bytes-like objects work as well and are
        held for as long as the reader lives.
    little: :class:`bool`
        Whether fixed-width numbers are little endian, as Bedrock sends them.

    Attributes
    -----------
    remaining: :class:`int`
        How many bytes are left to read.
    offset: :class:`int`
        How many bytes have been read so far.
    little: :class:`bool`
        Whether fixed-width numbers are read little endian.
    """

    def __new__(cls, data: Buffer, /, *, little: bool = False) -> Reader:
        ...

    @property
    def remaining(self) -> int:
        ...

    @property
    def offset(self) -> int:
        ...

    @property
    def little(self) -> bool:
        ...

    def boolean(self) -> bool:
        """
        Reads a boolean stored in one byte.
        """
        ...

    def i8(self) -> int:
        """
        Reads a signed 8-bit integer.
        """
        ...

    def u8(self) -> int:
        """
        Reads an unsigned 8-bit integer.
        """
        ...

    def i16(self) -> int:
        """
        Reads a signed 16-bit integer.
        """
        ...

    def u16(self) -> int:
        """
        Reads an unsigned 16-bit integer.
        """
        ...

    def i32(self) -> int:
        """
        Reads a signed 32-bit integer.
        """
        ...

    def u32(self) -> int:
        """
        Reads an unsigned 32-bit integer.
        """
        ...

    def i64(self) -> int:
        """
        Reads a signed 64-bit integer.
        """
        ...

    def u64(self) -> int:
        """
        Reads an unsigned 64-bit integer.
        """
        ...

    def f32(self) -> float:
        """
        Reads a 32-bit float.
        """
        ...

    def f64(self) -> float:
        """
        Reads a 64-bit double.
        """
        ...

    def varint(self) -> int:
        """
        Reads a Java varint as a signed 32-bit integer.
        """
        ...

    def varlong(self) -> int:
        """
        Reads a Java varlong as a signed 64-bit integer.
        """
        ...

    def uvarint(self) -> int:
        """
        Reads an unsigned varint, the Bedrock form for counts and ids.
        """
        ...

    def uvarlong(self) -> int:
        """
        Reads an unsigned varlong.
        """
        ...

    def zigzag(self) -> int:
        """
        Reads a zigzag varint, the Bedrock form for signed 32-bit values.
        """
        ...

    def zigzag64(self) -> int:
        """
        Reads a zigzag varlong, the Bedrock form for signed 64-bit values.
        """
        ...

    def string(self, limit: int = 32767, /) -> str:
        """
        Reads a UTF-8 string with a varint length prefix.

        Rejects strings longer than ``limit`` characters the way vanilla does,
        before the bytes are decoded.
        """
        ...

    def uuid(self) -> UUID:
        """
        Reads a UUID from 16 bytes.
        """
        ...

    def position(self) -> BlockPosition:
        """
        Reads a block position from its packed 64-bit form.
        """
        ...

    def nbt(self, named: bool | None = None, /) -> Tag:
        """
        Reads an NBT value.

        Java sends an unnamed root and Bedrock a named one; pass ``named`` to
        override that for older versions.
        """
        ...

    def raw(self, count: int, /) -> bytes:
        """
        Reads exactly ``count`` bytes.
        """
        ...

    def byte_array(self, limit: int | None = None, /) -> bytes:
        """
        Reads a byte array with a varint length prefix, at most ``limit`` bytes long.
        """
        ...

    def rest(self) -> bytes:
        """
        Reads every byte that is left.
        """
        ...

    def skip(self, count: int, /) -> None:
        """
        Moves past ``count`` bytes without reading them.
        """
        ...

    def reset(self, data: Buffer, /) -> None:
        """
        Starts over on new data, so one reader can serve packet after packet.
        """
        ...

class Writer:
    """
    Builds a packet payload field by field.

    Every method appends one field. Fixed-width numbers are big endian for
    Java and little endian for Bedrock, which the ``little`` flag decides.
    :meth:`take` hands the bytes over and empties the writer, so one writer
    can build packet after packet without allocating again.

    Parameters
    -----------
    little: :class:`bool`
        Whether fixed-width numbers are little endian, as Bedrock expects.

    Attributes
    -----------
    little: :class:`bool`
        Whether fixed-width numbers are written little endian.
    """

    def __new__(cls, *, little: bool = False) -> Writer:
        ...

    def __len__(self) -> int:
        ...

    @property
    def little(self) -> bool:
        ...

    def boolean(self, value: bool, /) -> None:
        """
        Writes a boolean as one byte.
        """
        ...

    def i8(self, value: int, /) -> None:
        """
        Writes a signed 8-bit integer.
        """
        ...

    def u8(self, value: int, /) -> None:
        """
        Writes an unsigned 8-bit integer.
        """
        ...

    def i16(self, value: int, /) -> None:
        """
        Writes a signed 16-bit integer.
        """
        ...

    def u16(self, value: int, /) -> None:
        """
        Writes an unsigned 16-bit integer.
        """
        ...

    def i32(self, value: int, /) -> None:
        """
        Writes a signed 32-bit integer.
        """
        ...

    def u32(self, value: int, /) -> None:
        """
        Writes an unsigned 32-bit integer.
        """
        ...

    def i64(self, value: int, /) -> None:
        """
        Writes a signed 64-bit integer.
        """
        ...

    def u64(self, value: int, /) -> None:
        """
        Writes an unsigned 64-bit integer.
        """
        ...

    def f32(self, value: float, /) -> None:
        """
        Writes a 32-bit float.
        """
        ...

    def f64(self, value: float, /) -> None:
        """
        Writes a 64-bit double.
        """
        ...

    def varint(self, value: int, /) -> None:
        """
        Writes a signed 32-bit integer as a Java varint.
        """
        ...

    def varlong(self, value: int, /) -> None:
        """
        Writes a signed 64-bit integer as a Java varlong.
        """
        ...

    def uvarint(self, value: int, /) -> None:
        """
        Writes an unsigned varint, the Bedrock form for counts and ids.
        """
        ...

    def uvarlong(self, value: int, /) -> None:
        """
        Writes an unsigned varlong.
        """
        ...

    def zigzag(self, value: int, /) -> None:
        """
        Writes a zigzag varint, the Bedrock form for signed 32-bit values.
        """
        ...

    def zigzag64(self, value: int, /) -> None:
        """
        Writes a zigzag varlong, the Bedrock form for signed 64-bit values.
        """
        ...

    def string(self, value: str, limit: int = 32767, /) -> None:
        """
        Writes a UTF-8 string with a varint length prefix.

        Raises :exc:`ValueError` when the string is longer than ``limit``
        characters, so a packet the client would reject never leaves.
        """
        ...

    def uuid(self, value: UUID, /) -> None:
        """
        Writes a UUID as 16 bytes.
        """
        ...

    def position(self, value: BlockPosition, /) -> None:
        """
        Writes a block position in its packed 64-bit form.
        """
        ...

    def nbt(self, value: TagLike, named: bool | None = None, /) -> None:
        """
        Writes an NBT value.

        Java gets an unnamed root and Bedrock a named one; pass ``named`` to
        override that for older versions.
        """
        ...

    def raw(self, data: Buffer, /) -> None:
        """
        Writes bytes as they are, without a length prefix.
        """
        ...

    def byte_array(self, data: Buffer, limit: int | None = None, /) -> None:
        """
        Writes a byte array with a varint length prefix, refusing more than ``limit`` bytes.
        """
        ...

    def take(self) -> bytes:
        """
        Returns everything written so far and empties the writer.
        """
        ...

    def clear(self) -> None:
        """
        Drops everything written so far.
        """
        ...

class Codec:
    """
    A compiled packet layout that decodes and encodes in one call.

    The spec lists the fields in order, one letter each, with spaces allowed
    between them. A number after a letter sets a limit: the maximum length
    of a string, the exact size of a raw field or the maximum count of an
    array.

    +--------+---------------------------------------------------------+
    | Letter | Field                                                   |
    +========+=========================================================+
    | ``?``  | boolean                                                 |
    | ``b``  | signed 8-bit integer, ``B`` unsigned                    |
    | ``h``  | signed 16-bit integer, ``H`` unsigned                   |
    | ``i``  | signed 32-bit integer, ``I`` unsigned                   |
    | ``q``  | signed 64-bit integer, ``Q`` unsigned                   |
    | ``f``  | 32-bit float, ``d`` 64-bit double                       |
    | ``v``  | Java varint, ``V`` Java varlong                         |
    | ``u``  | unsigned varint, ``U`` unsigned varlong                 |
    | ``z``  | zigzag varint, ``Z`` zigzag varlong                     |
    | ``s``  | string, at most 32767 characters unless a number follows|
    | ``g``  | UUID                                                    |
    | ``p``  | block position                                          |
    | ``n``  | NBT, unnamed for Java and named for Bedrock             |
    | ``x``  | raw bytes of the size that follows                      |
    | ``a``  | byte array with a varint length prefix                  |
    | ``r``  | every remaining byte, only as the last field            |
    | ``[]`` | array of the fields inside, with a varint count prefix  |
    | ``()`` | optional fields inside, with a boolean prefix           |
    +--------+---------------------------------------------------------+

    An array decodes to a list and an optional to ``None`` or the value. A
    group of one field yields that field alone, a longer group a tuple.

    Parameters
    -----------
    spec: :class:`str`
        The field letters.
    little: :class:`bool`
        Whether fixed-width numbers are little endian, as Bedrock uses them.

    Attributes
    -----------
    spec: :class:`str`
        The spec the codec was built from.
    fields: :class:`int`
        How many top-level fields the codec reads and writes.
    little: :class:`bool`
        Whether fixed-width numbers are little endian.
    """

    def __new__(cls, spec: str, /, *, little: bool = False) -> Codec:
        ...

    @property
    def spec(self) -> str:
        ...

    @property
    def fields(self) -> int:
        ...

    @property
    def little(self) -> bool:
        ...

    def decode(self, data: Buffer, /) -> tuple[Any, ...]:
        """
        Reads every field from a whole packet payload.

        Returns a tuple with one value per field. Bytes left over after the
        last field count as an error, because they mean the codec does not
        match the packet.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The payload is shorter than the fields need, has trailing bytes or
            holds a value the codec rejects.
        """
        ...

    def encode(self, *values: object) -> bytes:
        """
        Writes one value per field and returns the payload bytes.

        Raises
        -------
        TypeError
            A value has the wrong type or the count does not match the fields.
        OverflowError
            A number does not fit its field.
        ValueError
            A string or byte field is longer than its limit.
        """
        ...

    def read(self, reader: Reader, /) -> tuple[Any, ...]:
        """
        Reads the fields from a :class:`Reader` at its current position.

        Unlike :meth:`decode` this leaves whatever follows untouched, so a
        packet can start with a codec and continue by hand.
        """
        ...

    def write(self, writer: Writer, /, *values: object) -> None:
        """
        Appends the fields to a :class:`Writer`.
        """
        ...
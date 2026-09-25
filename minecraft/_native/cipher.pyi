from collections.abc import Buffer

class Cfb8Cipher:
    """
    Encrypts and decrypts a Java Edition connection.

    Java uses AES in CFB8 mode with the shared secret from the login
    handshake as both key and IV. Each direction keeps its own stream
    state, so hand every outgoing byte to :meth:`encrypt` and every
    incoming byte to :meth:`decrypt`, once and in wire order. The key
    material is wiped when the cipher goes away.

    Parameters
    -----------
    secret: :class:`bytes`
        The shared secret, the 16 bytes of the AES-128 key Java negotiates.

    Raises
    -------
    ValueError
        The secret is not 16 bytes long.

    Attributes
    -----------
    hardware: :class:`bool`
        Whether the CPU's AES instructions are in use rather than the
        software fallback.
    """

    def __new__(cls, secret: Buffer, /) -> Cfb8Cipher:
        ...

    @property
    def hardware(self) -> bool:
        ...

    def encrypt(self, data: Buffer, /) -> bytes:
        """
        Encrypts the next outgoing bytes and returns them.
        """
        ...

    def decrypt(self, data: Buffer, /) -> bytes:
        """
        Decrypts the next incoming bytes and returns them.
        """
        ...
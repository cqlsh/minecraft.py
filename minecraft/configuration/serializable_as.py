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

class SerializableAs:
    """Represents an "alias" that a :class:`ConfigurationSerializable` may be stored as.

    If this is not present on a :class:`ConfigurationSerializable` class, it
    will use the fully qualified name of the class.

    This value will be stored in the configuration so that the configuration
    deserialization can determine what type it is.

    Using this decorator on any other class than a
    :class:`ConfigurationSerializable` will have no effect.

    .. seealso::

        :meth:`ConfigurationSerialization.register_class`

    Parameters
    -----------
    value: :class:`str`
        The name your class will be stored and retrieved as.

        This name MUST be unique. We recommend using names such as
        ``MyPluginThing`` instead of ``Thing``.
    """

    __slots__ = ['value']

    def __init__(self, value: str) -> None:
        self.value: str = value

    def __repr__(self) -> str:
        return f'<SerializableAs value={self.value!r}>'

    def __call__[T: type](self, clazz: T) -> T:
        setattr(clazz, '__serializable_as__', self)
        return clazz

    @classmethod
    def of(cls, clazz: type) -> SerializableAs | None:
        """Gets the alias declared directly on the given class.

        Like a Java annotation, the alias is not inherited by subclasses.

        Parameters
        -----------
        clazz: :class:`type`
            The class to inspect.

        Returns
        --------
        Optional[:class:`SerializableAs`]
            The declared alias, or ``None`` if the class has none.
        """
        declared: object = vars(clazz).get('__serializable_as__')
        return declared if isinstance(declared, cls) else None

__all__ = ['SerializableAs']
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

from abc import ABC, abstractmethod
from typing import Any

class ConfigurationSerializable(ABC):
    """Represents an object that may be serialized.

    These objects MUST implement one of the following, in addition to the
    methods as defined by this class:

    - A static method ``deserialize`` that accepts a single :class:`dict` of
      :class:`str` to :class:`object` and returns the class.
    - A static method ``value_of`` that accepts a single :class:`dict` of
      :class:`str` to :class:`object` and returns the class.
    - A constructor that accepts a single :class:`dict` of :class:`str` to
      :class:`object`.

    In addition to subclassing this class, you must register the class with
    :meth:`ConfigurationSerialization.register_class`.

    .. seealso::

        :class:`DelegateDeserialization`, :class:`SerializableAs`
    """

    __slots__ = ()

    @abstractmethod
    def serialize(self) -> dict[str, Any]:
        """Creates a :class:`dict` representation of this class.

        This class must provide a method to restore this class, as defined in
        the :class:`ConfigurationSerializable` documentation.

        .. note::

            It is not intended for this method to be called directly, this will
            be called by the :class:`ConfigurationSerialization` class.

        Returns
        --------
        Dict[:class:`str`, Any]
            The current state of this class.
        """
        raise NotImplementedError

__all__ = ['ConfigurationSerializable']
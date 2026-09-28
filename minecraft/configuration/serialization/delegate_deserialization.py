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

from .configuration_serializable import ConfigurationSerializable

class DelegateDeserialization:
    """Applies to a :class:`ConfigurationSerializable` that will delegate all
    deserialization to another :class:`ConfigurationSerializable`.

    Parameters
    -----------
    value: Type[:class:`ConfigurationSerializable`]
        Which class should be used as a delegate for this class' deserialization.
    """

    __slots__ = ['value']

    def __init__(self, value: type[ConfigurationSerializable]) -> None:
        self.value: type[ConfigurationSerializable] = value

    def __repr__(self) -> str:
        return f'<DelegateDeserialization value={self.value!r}>'

    def __call__[T: type](self, clazz: T) -> T:
        setattr(clazz, '__delegate_deserialization__', self)
        return clazz

    @classmethod
    def of(cls, clazz: type) -> DelegateDeserialization | None:
        """Gets the delegate declared directly on the given class.

        Like a Java annotation, the delegate is not inherited by subclasses.

        Parameters
        -----------
        clazz: :class:`type`
            The class to inspect.

        Returns
        --------
        Optional[:class:`DelegateDeserialization`]
            The declared delegate, or ``None`` if the class has none.
        """
        declared: object = vars(clazz).get('__delegate_deserialization__')
        return declared if isinstance(declared, cls) else None

__all__ = ['DelegateDeserialization']
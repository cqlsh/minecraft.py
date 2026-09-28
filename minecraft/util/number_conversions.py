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

import math
from typing import Never

class NumberConversions:
    """Utils for casting number types to other number types.

    This class only consists of static methods and cannot be instantiated.

    .. note::

        Python has a single unbounded :class:`int` and a single 64 bit
        :class:`float`. The methods that differ only in the width of their
        Java result are therefore equivalent and never narrow a value.
    """

    __slots__ = ()

    def __new__(cls) -> Never:
        raise TypeError(f'{cls.__name__} cannot be instantiated')

    @staticmethod
    def floor(num: float) -> int:
        """Rounds a number down to the nearest integer.

        Parameters
        -----------
        num: :class:`float`
            The number to round.

        Raises
        -------
        ValueError
            The number is NaN.
        OverflowError
            The number is infinite.

        Returns
        --------
        :class:`int`
            The largest integer less than or equal to ``num``.
        """
        return math.floor(num)

    @staticmethod
    def ceil(num: float) -> int:
        """Rounds a number up to the nearest integer.

        Parameters
        -----------
        num: :class:`float`
            The number to round.

        Raises
        -------
        ValueError
            The number is NaN.
        OverflowError
            The number is infinite.

        Returns
        --------
        :class:`int`
            The smallest integer greater than or equal to ``num``.
        """
        return math.ceil(num)

    @staticmethod
    def round(num: float) -> int:
        """Rounds a number to the nearest integer, with ties rounding up.

        Unlike the built-in :func:`round` this does not round ties to the
        nearest even integer, ``2.5`` is rounded to ``3``.

        Parameters
        -----------
        num: :class:`float`
            The number to round.

        Raises
        -------
        ValueError
            The number is NaN.
        OverflowError
            The number is infinite.

        Returns
        --------
        :class:`int`
            The nearest integer.
        """
        return math.floor(num + 0.5)

    @staticmethod
    def square(num: float) -> float:
        """Multiplies a number with itself.

        Parameters
        -----------
        num: :class:`float`
            The number to square.

        Returns
        --------
        :class:`float`
            The squared number.
        """
        return num * num

    @staticmethod
    def to_int(value: object) -> int:
        """Converts an object to an integer.

        Numbers are truncated towards zero, any other object is parsed from
        its string representation. Booleans are not considered numbers.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`int`
            The converted integer, or ``0`` if the object cannot be converted.
        """
        if type(value) is int:
            return value

        if type(value) is float or (isinstance(value, (int, float)) and not isinstance(value, bool)):
            try:
                return int(value)
            except (ValueError, OverflowError):
                return 0

        try:
            return int(str(value))
        except ValueError:
            return 0

    @staticmethod
    def to_float(value: object) -> float:
        """Converts an object to a floating point number.

        This is equivalent to :meth:`to_double`.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`float`
            The converted number, or ``0.0`` if the object cannot be converted.
        """
        return NumberConversions.to_double(value)

    @staticmethod
    def to_double(value: object) -> float:
        """Converts an object to a floating point number.

        Any object that is not a number is parsed from its string
        representation. Booleans are not considered numbers.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`float`
            The converted number, or ``0.0`` if the object cannot be converted.
        """
        if type(value) is float:
            return value

        if isinstance(value, (int, float)) and not isinstance(value, bool):
            try:
                return float(value)
            except OverflowError:
                return 0.0

        try:
            return float(str(value))
        except ValueError:
            return 0.0

    @staticmethod
    def to_long(value: object) -> int:
        """Converts an object to an integer.

        This is equivalent to :meth:`to_int`.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`int`
            The converted integer, or ``0`` if the object cannot be converted.
        """
        return NumberConversions.to_int(value)

    @staticmethod
    def to_short(value: object) -> int:
        """Converts an object to an integer.

        This is equivalent to :meth:`to_int`.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`int`
            The converted integer, or ``0`` if the object cannot be converted.
        """
        return NumberConversions.to_int(value)

    @staticmethod
    def to_byte(value: object) -> int:
        """Converts an object to an integer.

        This is equivalent to :meth:`to_int`.

        Parameters
        -----------
        value: :class:`object`
            The object to convert.

        Returns
        --------
        :class:`int`
            The converted integer, or ``0`` if the object cannot be converted.
        """
        return NumberConversions.to_int(value)

    @staticmethod
    def is_finite(d: float) -> bool:
        """Checks whether a number is neither infinite nor NaN.

        Parameters
        -----------
        d: :class:`float`
            The number to check.

        Returns
        --------
        :class:`bool`
            Whether the number is finite.
        """
        return math.isfinite(d)

    @staticmethod
    def check_finite(d: float, message: str) -> None:
        """Ensures that a number is neither infinite nor NaN.

        Parameters
        -----------
        d: :class:`float`
            The number to check.
        message: :class:`str`
            The message of the raised exception.

        Raises
        -------
        ValueError
            The number is not finite.
        """
        if not math.isfinite(d):
            raise ValueError(message)

__all__ = ['NumberConversions']
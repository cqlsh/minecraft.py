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

from collections.abc import Mapping
from typing import Any, Self, overload

from .configuration.serialization import ConfigurationSerializable

class Vector(ConfigurationSerializable):
    """Represents a mutable vector.

    Because the components of vectors are mutable, storing vectors long term
    may be dangerous if passing code modifies the vector later. If you want to
    keep around a vector, it may be wise to call :meth:`clone` in order to get
    a copy.

    .. container:: operations

        .. describe:: x == y

            Checks if two vectors are equal. Only two vectors of the same class
            can ever be equal. This uses a fuzzy match to account for floating
            point errors, the threshold can be retrieved with :meth:`get_epsilon`.

        .. describe:: x != y

            Checks if two vectors are not equal.

        .. describe:: hash(x)

            Returns the hash code of the vector, identical to the one of Java.

        .. describe:: str(x)

            Returns the components of the vector as ``x,y,z``.

    Parameters
    -----------
    x: :class:`float`
        The X component.
    y: :class:`float`
        The Y component.
    z: :class:`float`
        The Z component.
    """

    x: float
    """:class:`float`: The X component."""
    y: float
    """:class:`float`: The Y component."""
    z: float
    """:class:`float`: The Z component."""

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None: ...

    def __eq__(self, other: object, /) -> bool: ...

    def __ne__(self, other: object, /) -> bool: ...

    def __hash__(self) -> int: ...

    def __reduce__(self) -> tuple[type[Self], tuple[float, float, float]]:
        """Returns the class and the components, used by :mod:`copy` and :mod:`pickle`."""
        ...

    def add(self, vec: Vector, /) -> Self:
        """Adds a vector to this one.

        Parameters
        -----------
        vec: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def subtract(self, vec: Vector, /) -> Self:
        """Subtracts a vector from this one.

        Parameters
        -----------
        vec: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def multiply(self, m: Vector | float, /) -> Self:
        """Multiplies the vector by another, or performs scalar multiplication,
        multiplying all components with a scalar.

        Parameters
        -----------
        m: Union[:class:`Vector`, :class:`float`]
            The other vector or the factor.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def divide(self, vec: Vector, /) -> Self:
        """Divides the vector by another.

        Parameters
        -----------
        vec: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def copy(self, vec: Vector, /) -> Self:
        """Copies another vector.

        Parameters
        -----------
        vec: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def length(self) -> float:
        """Gets the magnitude of the vector, defined as sqrt(x^2 + y^2 + z^2).

        The value of this method is not cached and uses a costly square-root
        function, so do not repeatedly call this method to get the vector's
        magnitude. Infinity will be returned if the inner result overflows, which
        will be caused if the length is too long.

        Returns
        --------
        :class:`float`
            The magnitude.
        """
        ...

    def length_squared(self) -> float:
        """Gets the magnitude of the vector squared.

        Returns
        --------
        :class:`float`
            The magnitude.
        """
        ...

    def distance(self, o: Vector, /) -> float:
        """Gets the distance between this vector and another.

        The value of this method is not cached and uses a costly square-root
        function, so do not repeatedly call this method to get the distance.
        Infinity will be returned if the inner result overflows, which will be
        caused if the distance is too long.

        Parameters
        -----------
        o: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`float`
            The distance.
        """
        ...

    def distance_squared(self, o: Vector, /) -> float:
        """Gets the squared distance between this vector and another.

        Parameters
        -----------
        o: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`float`
            The distance.
        """
        ...

    def angle(self, other: Vector, /) -> float:
        """Gets the angle between this vector and another in radians.

        Parameters
        -----------
        other: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`float`
            The angle in radians.
        """
        ...

    def midpoint(self, other: Vector, /) -> Self:
        """Sets this vector to the midpoint between this vector and another.

        Parameters
        -----------
        other: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            This same vector (now a midpoint).
        """
        ...

    def get_midpoint(self, other: Vector, /) -> Vector:
        """Gets a new midpoint vector between this vector and another.

        Parameters
        -----------
        other: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            A new midpoint vector.
        """
        ...

    def dot(self, other: Vector, /) -> float:
        """Calculates the dot product of this vector with another.

        The dot product is defined as x1 * x2 + y1 * y2 + z1 * z2. The returned
        value is a scalar.

        Parameters
        -----------
        other: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`float`
            The dot product.
        """
        ...

    def cross_product(self, o: Vector, /) -> Self:
        """Calculates the cross product of this vector with another.

        The cross product is defined as:

        - x = y1 * z2 - y2 * z1
        - y = z1 * x2 - z2 * x1
        - z = x1 * y2 - x2 * y1

        Parameters
        -----------
        o: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def get_cross_product(self, o: Vector, /) -> Vector:
        """Calculates the cross product of this vector with another without mutating
        the original.

        The cross product is defined as:

        - x = y1 * z2 - y2 * z1
        - y = z1 * x2 - z2 * x1
        - z = x1 * y2 - x2 * y1

        Parameters
        -----------
        o: :class:`Vector`
            The other vector.

        Returns
        --------
        :class:`Vector`
            A new vector.
        """
        ...

    def normalize(self) -> Self:
        """Converts this vector to a unit vector (a vector with length of 1).

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def zero(self) -> Self:
        """Zero this vector's components.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def is_zero(self) -> bool:
        """Check whether or not each component of this vector is equal to 0.

        Returns
        --------
        :class:`bool`
            ``True`` if equal to zero, ``False`` if at least one component is non-zero.
        """
        ...

    def is_in_aabb(self, min: Vector, max: Vector, /) -> bool:
        """Returns whether this vector is in an axis-aligned bounding box.

        The minimum and maximum vectors given must be truly the minimum and
        maximum X, Y and Z components.

        Parameters
        -----------
        min: :class:`Vector`
            The minimum vector.
        max: :class:`Vector`
            The maximum vector.

        Returns
        --------
        :class:`bool`
            Whether this vector is in the AABB.
        """
        ...

    def is_in_sphere(self, origin: Vector, radius: float, /) -> bool:
        """Returns whether this vector is within a sphere.

        Parameters
        -----------
        origin: :class:`Vector`
            The sphere origin.
        radius: :class:`float`
            The sphere radius.

        Returns
        --------
        :class:`bool`
            Whether this vector is in the sphere.
        """
        ...

    def is_normalized(self) -> bool:
        """Returns if a vector is normalized.

        Returns
        --------
        :class:`bool`
            Whether the vector is normalised.
        """
        ...

    def rotate_around_x(self, angle: float, /) -> Self:
        """Rotates the vector around the x axis.

        This piece of math is based on the standard rotation matrix for vectors
        in three dimensional space.

        Parameters
        -----------
        angle: :class:`float`
            The angle to rotate the vector about. This angle is passed in radians.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def rotate_around_y(self, angle: float, /) -> Self:
        """Rotates the vector around the y axis.

        This piece of math is based on the standard rotation matrix for vectors
        in three dimensional space.

        Parameters
        -----------
        angle: :class:`float`
            The angle to rotate the vector about. This angle is passed in radians.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def rotate_around_z(self, angle: float, /) -> Self:
        """Rotates the vector around the z axis.

        This piece of math is based on the standard rotation matrix for vectors
        in three dimensional space.

        Parameters
        -----------
        angle: :class:`float`
            The angle to rotate the vector about. This angle is passed in radians.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def rotate_around_axis(self, axis: Vector, angle: float, /) -> Self:
        """Rotates the vector around a given arbitrary axis in 3 dimensional space.

        Rotation will follow the general Right-Hand-Rule, which means rotation
        will be counterclockwise when the axis is pointing towards the observer.

        This method will always make sure the provided axis is a unit vector, to
        not modify the length of the vector when rotating. If you are experienced
        with the scaling of a non-unit axis vector, you can use
        :meth:`rotate_around_non_unit_axis`.

        Parameters
        -----------
        axis: :class:`Vector`
            The axis to rotate the vector around. If the passed vector is not of
            length 1, it gets copied and normalized before using it for the
            rotation.
        angle: :class:`float`
            The angle to rotate the vector around the axis.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    def rotate_around_non_unit_axis(self, axis: Vector, angle: float, /) -> Self:
        """Rotates the vector around a given arbitrary axis in 3 dimensional space.

        Rotation will follow the general Right-Hand-Rule, which means rotation
        will be counterclockwise when the axis is pointing towards the observer.

        Note that the vector length will change accordingly to the axis vector
        length. If the provided axis is not a unit vector, the rotated vector
        will not have its previous length. The scaled length of the resulting
        vector will be related to the axis vector. If you are not perfectly sure
        about the scaling of the vector, use :meth:`rotate_around_axis`.

        Parameters
        -----------
        axis: :class:`Vector`
            The axis to rotate the vector around.
        angle: :class:`float`
            The angle to rotate the vector around the axis.

        Returns
        --------
        :class:`Vector`
            The same vector.
        """
        ...

    @property
    def block_x(self) -> int:
        """:class:`int`: The floored value of the X component, indicating the block that this vector is contained with."""
        ...

    @property
    def block_y(self) -> int:
        """:class:`int`: The floored value of the Y component, indicating the block that this vector is contained with."""
        ...

    @property
    def block_z(self) -> int:
        """:class:`int`: The floored value of the Z component, indicating the block that this vector is contained with."""
        ...

    def clone(self) -> Self:
        """Get a new vector.

        Returns
        --------
        :class:`Vector`
            A new vector of the same class with the same components.
        """
        ...

    def to_block_vector(self) -> BlockVector:
        """Get the block vector of this vector.

        Returns
        --------
        :class:`BlockVector`
            A block vector.
        """
        ...

    def check_finite(self) -> None:
        """Check if each component of this vector is finite.

        Raises
        -------
        ValueError
            Any component is not finite.
        """
        ...

    @staticmethod
    def get_epsilon() -> float:
        """Get the threshold used for ``==``.

        Returns
        --------
        :class:`float`
            The epsilon.
        """
        ...

    @staticmethod
    def get_minimum(v1: Vector, v2: Vector, /) -> Vector:
        """Gets the minimum components of two vectors.

        Parameters
        -----------
        v1: :class:`Vector`
            The first vector.
        v2: :class:`Vector`
            The second vector.

        Returns
        --------
        :class:`Vector`
            The minimum.
        """
        ...

    @staticmethod
    def get_maximum(v1: Vector, v2: Vector, /) -> Vector:
        """Gets the maximum components of two vectors.

        Parameters
        -----------
        v1: :class:`Vector`
            The first vector.
        v2: :class:`Vector`
            The second vector.

        Returns
        --------
        :class:`Vector`
            The maximum.
        """
        ...

    @staticmethod
    def get_random() -> Vector:
        """Gets a random vector with components having a random value between 0 and 1.

        Returns
        --------
        :class:`Vector`
            A random vector.
        """
        ...

    def serialize(self) -> dict[str, Any]:
        """Creates a :class:`dict` representation of this class.

        Returns
        --------
        Dict[:class:`str`, Any]
            The components under the keys ``x``, ``y`` and ``z``.
        """
        ...

    @classmethod
    def deserialize(cls, args: Mapping[str, Any], /) -> Self:
        """Creates a vector from its :class:`dict` representation.

        Parameters
        -----------
        args: Mapping[:class:`str`, Any]
            The components under the keys ``x``, ``y`` and ``z``, missing components are 0.

        Returns
        --------
        :class:`Vector`
            A new vector of the class the method was called on.
        """
        ...

class BlockVector(Vector):
    """A vector with a hash function that truncates the X, Y, Z components, a la
    BlockVector in WorldEdit.

    BlockVectors can be used in sets and as keys of dictionaries. Be aware that
    BlockVectors are mutable, but it is important that BlockVectors are never
    changed once put into a set or a dictionary.

    Instead of the components a single :class:`Vector` may be passed, whose
    components are copied.

    .. container:: operations

        .. describe:: x == y

            Checks if another block vector is equivalent. Two block vectors are
            equivalent if their components are equal after they have been
            truncated towards zero.

        .. describe:: x != y

            Checks if another block vector is not equivalent.

        .. describe:: hash(x)

            Returns the hash code of the block vector, identical to the one of Java.

    Parameters
    -----------
    x: :class:`float`
        The X component.
    y: :class:`float`
        The Y component.
    z: :class:`float`
        The Z component.
    """

    @overload
    def __init__(self, vec: Vector, /) -> None: ...

    @overload
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None: ...

    def __eq__(self, other: object, /) -> bool: ...

    def __ne__(self, other: object, /) -> bool: ...

    def __hash__(self) -> int: ...

__all__ = ('Vector', 'BlockVector')
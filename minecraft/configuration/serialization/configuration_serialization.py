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

import importlib
import inspect
import logging
from collections.abc import Callable, Mapping
from types import ClassMethodDescriptorType
from typing import Any, ClassVar, get_origin

from .configuration_serializable import ConfigurationSerializable
from .delegate_deserialization import DelegateDeserialization
from .serializable_as import SerializableAs

_log: logging.Logger = logging.getLogger(__name__)

class ConfigurationSerialization:
    """Utility class for storing and retrieving classes for :class:`~minecraft.configuration.Configuration`.

    Parameters
    -----------
    clazz: Type[:class:`ConfigurationSerializable`]
        The class to deserialize into.

    Attributes
    -----------
    SERIALIZED_TYPE_KEY: :class:`str`
        The key under which the alias of a serialized object is stored, ``==``.
    """

    SERIALIZED_TYPE_KEY: ClassVar[str] = '=='
    _aliases: ClassVar[dict[str, type[ConfigurationSerializable]]] = {}
    _builtins: ClassVar[dict[str, str]] = {
        'Vector': 'minecraft.util.Vector',
        'org.bukkit.util.Vector': 'minecraft.util.Vector',
        'BlockVector': 'minecraft.util.BlockVector',
        'org.bukkit.util.BlockVector': 'minecraft.util.BlockVector',
        'BoundingBox': 'minecraft.util.BoundingBox',
        'org.bukkit.util.BoundingBox': 'minecraft.util.BoundingBox',
        'Color': 'minecraft.Color',
        'org.bukkit.Color': 'minecraft.Color',
        'Firework': 'minecraft.FireworkEffect',
        'org.bukkit.FireworkEffect': 'minecraft.FireworkEffect',
        'org.bukkit.Location': 'minecraft.Location',
        'Pattern': 'minecraft.block.banner.Pattern',
        'org.bukkit.block.banner.Pattern': 'minecraft.block.banner.Pattern',
        'SpawnRule': 'minecraft.block.spawner.SpawnRule',
        'org.bukkit.block.spawner.SpawnRule': 'minecraft.block.spawner.SpawnRule',
        'PotionEffect': 'minecraft.potion.PotionEffect',
        'org.bukkit.potion.PotionEffect': 'minecraft.potion.PotionEffect',
        'org.bukkit.inventory.ItemStack': 'minecraft.inventory.ItemStack',
        'org.bukkit.attribute.AttributeModifier': 'minecraft.attribute.AttributeModifier'
    }

    __slots__ = ['clazz']

    def __init__(self, clazz: type[ConfigurationSerializable]) -> None:
        self.clazz: type[ConfigurationSerializable] = clazz

    def __repr__(self) -> str:
        return f'<ConfigurationSerialization clazz={self.clazz!r}>'

    def _get_method(self, name: str) -> Callable[[Mapping[str, Any]], object] | None:
        attribute: object = inspect.getattr_static(self.clazz, name, None)
        if isinstance(attribute, (staticmethod, classmethod, ClassMethodDescriptorType)):
            return getattr(self.clazz, name)

        return None

    def _get_constructor(self) -> Callable[[Mapping[str, Any]], ConfigurationSerializable] | None:
        try:
            signature: inspect.Signature = inspect.signature(self.clazz)
        except (TypeError, ValueError):
            return None

        positional: list[inspect.Parameter] = [
            parameter for parameter in signature.parameters.values()
            if parameter.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD)
        ]
        if len(positional) != 1:
            return None

        annotation: object = positional[0].annotation
        origin: object = get_origin(annotation) or annotation
        if isinstance(origin, type) and issubclass(origin, Mapping):
            return self.clazz

        return None

    def _deserialize_via_method(
        self, method: Callable[[Mapping[str, Any]], object], args: Mapping[str, Any]
    ) -> ConfigurationSerializable | None:
        try:
            result: object = method(args)
        except Exception:
            _log.exception('Could not call method %r of %r for deserialization', method, self.clazz)
            return None

        if isinstance(result, ConfigurationSerializable):
            return result

        _log.error('Could not call method %r of %r for deserialization: method returned %r', method, self.clazz, result)
        return None

    def _deserialize_via_ctor(
        self, constructor: Callable[[Mapping[str, Any]], ConfigurationSerializable], args: Mapping[str, Any]
    ) -> ConfigurationSerializable | None:
        try:
            return constructor(args)
        except Exception:
            _log.exception('Could not call constructor of %r for deserialization', self.clazz)
            return None

    def deserialize(self, args: Mapping[str, Any]) -> ConfigurationSerializable | None:
        """Attempts to deserialize the given arguments into a new instance of the wrapped class.

        A static ``deserialize`` method is tried first, then a static ``value_of``
        method and finally a constructor whose single parameter is annotated as a mapping.

        Parameters
        -----------
        args: Mapping[:class:`str`, Any]
            Arguments for deserialization.

        Returns
        --------
        Optional[:class:`ConfigurationSerializable`]
            The new instance, or ``None`` if none of the ways succeeded.
        """
        for name in ('deserialize', 'value_of'):
            method: Callable[[Mapping[str, Any]], object] | None = self._get_method(name)
            if method is not None:
                result: ConfigurationSerializable | None = self._deserialize_via_method(method, args)
                if result is not None:
                    return result

        constructor: Callable[[Mapping[str, Any]], ConfigurationSerializable] | None = self._get_constructor()
        if constructor is not None:
            return self._deserialize_via_ctor(constructor, args)

        return None

    @staticmethod
    def _class_name(clazz: type) -> str:
        return f'{clazz.__module__}.{clazz.__qualname__}'

    @classmethod
    def _import_class(cls, path: str) -> type[ConfigurationSerializable] | None:
        module_name, _, qualname = path.rpartition('.')
        try:
            clazz: object = getattr(importlib.import_module(module_name), qualname)
        except (ImportError, AttributeError):
            return None

        if isinstance(clazz, type) and issubclass(clazz, ConfigurationSerializable):
            return clazz

        return None

    @classmethod
    def deserialize_object(
        cls, args: Mapping[str, Any], clazz: type[ConfigurationSerializable] | None = None
    ) -> ConfigurationSerializable | None:
        """Attempts to deserialize the given arguments into a new instance of the given class.

        The class must subclass :class:`ConfigurationSerializable`, including the
        extra methods as specified in its documentation.

        If a new instance could not be made, an example being the class not fully
        implementing the interface, ``None`` will be returned.

        Parameters
        -----------
        args: Mapping[:class:`str`, Any]
            Arguments for deserialization.
        clazz: Optional[Type[:class:`ConfigurationSerializable`]]
            Class to deserialize into. If omitted, it is looked up from the
            :attr:`SERIALIZED_TYPE_KEY` entry of ``args``.

        Raises
        -------
        ValueError
            ``clazz`` was omitted and ``args`` has no type key or the alias is unknown.
        TypeError
            The alias stored in ``args`` is not a string.

        Returns
        --------
        Optional[:class:`ConfigurationSerializable`]
            New instance of the specified class.
        """
        if clazz is None:
            if cls.SERIALIZED_TYPE_KEY not in args:
                raise ValueError(f"Args doesn't contain type key ('{cls.SERIALIZED_TYPE_KEY}')")

            alias: object = args[cls.SERIALIZED_TYPE_KEY]
            if alias is None:
                raise ValueError('Cannot have null alias')
            if not isinstance(alias, str):
                raise TypeError(f'Alias must be a string, not {type(alias).__name__}')

            clazz = cls.get_class_by_alias(alias)
            if clazz is None:
                raise ValueError(f"Specified class does not exist ('{alias}')")

        return cls(clazz).deserialize(args)

    @classmethod
    def register_class(cls, clazz: type[ConfigurationSerializable], alias: str | None = None) -> None:
        """Registers the given :class:`ConfigurationSerializable` class by its alias.

        Without an explicit alias the class is registered under both its
        :meth:`get_alias` result and its fully qualified name, unless it
        delegates its deserialization with :class:`DelegateDeserialization`.

        Parameters
        -----------
        clazz: Type[:class:`ConfigurationSerializable`]
            Class to register.
        alias: Optional[:class:`str`]
            Alias to register as.

        .. seealso::

            :class:`SerializableAs`
        """
        if alias is not None:
            cls._aliases[alias] = clazz
            return

        if DelegateDeserialization.of(clazz) is None:
            cls._aliases[cls.get_alias(clazz)] = clazz
            cls._aliases[cls._class_name(clazz)] = clazz

    @classmethod
    def unregister_class(cls, target: str | type[ConfigurationSerializable]) -> None:
        """Unregisters the specified alias, or every alias of the specified class.

        Parameters
        -----------
        target: Union[:class:`str`, Type[:class:`ConfigurationSerializable`]]
            Alias or class to unregister.
        """
        if isinstance(target, str):
            cls._aliases.pop(target, None)
            return

        for alias in [alias for alias, registered in cls._aliases.items() if registered is target]:
            del cls._aliases[alias]

    @classmethod
    def get_class_by_alias(cls, alias: str) -> type[ConfigurationSerializable] | None:
        """Attempts to get a registered :class:`ConfigurationSerializable` class by its alias.

        The built-in classes of the API are registered on first use under their
        alias and their fully qualified name, the aliases their Java counterparts
        are stored under are accepted as well. A class of this package that was
        stored under its fully qualified name is imported on demand.

        Parameters
        -----------
        alias: :class:`str`
            Alias of the serializable.

        Returns
        --------
        Optional[Type[:class:`ConfigurationSerializable`]]
            Registered class, or ``None`` if not found.
        """
        registered: type[ConfigurationSerializable] | None = cls._aliases.get(alias)
        if registered is not None:
            return registered

        path: str | None = cls._builtins.get(alias)
        if path is None and alias.startswith('minecraft.'):
            path = alias
        if path is None:
            return None

        clazz: type[ConfigurationSerializable] | None = cls._import_class(path)
        if clazz is None:
            return None

        for key in [key for key, value in cls._builtins.items() if value == path]:
            del cls._builtins[key]
            cls._aliases[key] = clazz

        cls.register_class(clazz)
        return clazz

    @classmethod
    def get_alias(cls, clazz: type[ConfigurationSerializable]) -> str:
        """Gets the correct alias for the given :class:`ConfigurationSerializable` class.

        Parameters
        -----------
        clazz: Type[:class:`ConfigurationSerializable`]
            Class to get alias for.

        Returns
        --------
        :class:`str`
            Alias to use for the class.
        """
        delegate: DelegateDeserialization | None = DelegateDeserialization.of(clazz)
        if delegate is not None and delegate.value is not clazz:
            return cls.get_alias(delegate.value)

        declared: SerializableAs | None = SerializableAs.of(clazz)
        if declared is not None:
            return declared.value

        return cls._class_name(clazz)

__all__ = ['ConfigurationSerialization']
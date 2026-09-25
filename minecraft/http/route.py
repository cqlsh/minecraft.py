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

from __future__ import annotations

from urllib.parse import quote

class Route:
    """
    An endpoint of a web API: a method and a URL with placeholders.

    Routes are made once at import time, so every URL the framework
    talks to is written down in one place, and they are filled in per
    request with values quoted for the URL.

    Parameters
    -----------
    method: :class:`str`
        The HTTP method.
    template: :class:`str`
        The URL with ``{name}`` placeholders for the values that change.

    Attributes
    -----------
    method: :class:`str`
        The HTTP method.
    template: :class:`str`
        The URL template.
    """

    __slots__ = ("method", "template")

    def __init__(self, method: str, template: str, /) -> None:
        self.method = method
        self.template = template

    def __repr__(self) -> str:
        return f"<Route {self.method} {self.template}>"

    def build(self, **values: str) -> str:
        """
        Fills the placeholders in and returns the URL.

        Every value is quoted, so a player name with odd characters ends
        up as one path segment or query value and nothing more.

        Raises
        -------
        KeyError
            A placeholder in the template was not given a value.
        """
        return self.template.format(**{name: quote(value, safe="") for name, value in values.items()})

__all__ = ["Route"]
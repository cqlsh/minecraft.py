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

import asyncio
import json
import ssl

from urllib.parse import urlsplit
from typing import Any, Final

from ..errors.base import InvalidData
from ..errors.http import HttpError
from .. import __version__

_LINE_LIMIT: Final = 65536

class HttpResponse:
    """
    What a server answered to a request.

    Attributes
    -----------
    status: :class:`int`
        The status code, such as ``200`` or ``204``.
    reason: :class:`str`
        The reason phrase next to the status, which may be empty.
    headers: Dict[:class:`str`, :class:`str`]
        The response headers with lowercase names. Repeated headers are
        joined with commas.
    body: :class:`bytes`
        The response body, already de-chunked.
    """

    __slots__ = ("status", "reason", "headers", "body")

    def __init__(self, *, status: int, reason: str, headers: dict[str, str], body: bytes) -> None:
        self.status = status
        self.reason = reason
        self.headers = headers
        self.body = body

    def __repr__(self) -> str:
        return f"<HttpResponse status={self.status} bytes={len(self.body)}>"

    @property
    def ok(self) -> bool:
        """
        :class:`bool`: Whether the status is in the 2xx range.
        """
        return 200 <= self.status < 300

    def json(self) -> Any:
        """
        Decodes the body as JSON.

        Raises
        -------
        ~minecraft.errors.InvalidData
            The body is not valid JSON.
        """
        try:
            return json.loads(self.body)
        except ValueError as error:
            raise InvalidData(f"response body is not JSON: {error}") from error

class HttpClient:
    """
    A small HTTP/1.1 client over TLS for the few calls a login makes.

    Every request opens its own connection and closes it afterwards,
    which keeps the client free of state and is plenty for a handful of
    calls per login. Bodies are read fully before the response is
    returned, whether they come with a length or in chunks.

    Parameters
    -----------
    timeout: :class:`float`
        Seconds a whole request may take, from connecting to the last
        body byte.
    limit: :class:`int`
        The largest body accepted, so a misbehaving server cannot fill
        memory.
    user_agent: Optional[:class:`str`]
        The ``User-Agent`` header to send. Defaults to the framework's
        name and version.
    ssl_context: Optional[:class:`ssl.SSLContext`]
        The TLS settings to use. Defaults to the system's trust store.

    Attributes
    -----------
    timeout: :class:`float`
        Seconds a whole request may take.
    limit: :class:`int`
        The largest body accepted.
    user_agent: :class:`str`
        The ``User-Agent`` header that is sent.
    """

    __slots__ = ("timeout", "limit", "user_agent", "_ssl_context")

    def __init__(
        self,
        *,
        timeout: float = 10.0,
        limit: int = 1048576,
        user_agent: str | None = None,
        ssl_context: ssl.SSLContext | None = None
    ) -> None:
        self.timeout = timeout
        self.limit = limit
        self.user_agent = user_agent or f"minecraftpy/{__version__}"
        self._ssl_context = ssl_context or ssl.create_default_context()

    async def get(self, url: str, /, *, headers: dict[str, str] | None = None) -> HttpResponse:
        """
        |coro|

        Sends a ``GET`` request.

        Raises
        -------
        HttpError
            The request could not be completed.
        """
        return await self.request("GET", url, headers=headers)

    async def post(self, url: str, /, *, body: bytes, content_type: str = "application/json", headers: dict[str, str] | None = None) -> HttpResponse:
        """
        |coro|

        Sends a ``POST`` request with a body.

        Raises
        -------
        HttpError
            The request could not be completed.
        """
        merged = {"content-type": content_type}
        if headers:
            merged.update(headers)

        return await self.request("POST", url, headers=merged, body=body)

    async def request(self, method: str, url: str, *, headers: dict[str, str] | None = None, body: bytes | None = None) -> HttpResponse:
        """
        |coro|

        Sends a request and reads the whole response.

        Parameters
        -----------
        method: :class:`str`
            The HTTP method.
        url: :class:`str`
            An ``https`` or ``http`` URL.
        headers: Optional[Dict[:class:`str`, :class:`str`]]
            Extra headers. ``Host``, ``User-Agent``, ``Content-Length``
            and ``Connection`` are set by the client.
        body: Optional[:class:`bytes`]
            The body to send, if any.

        Raises
        -------
        HttpError
            The URL is not usable, the connection failed, the request
            timed out or the response is not HTTP.
        """
        parts = urlsplit(url)
        if parts.scheme not in ("https", "http") or not parts.hostname:
            raise HttpError(f"{url!r} is not an http or https URL with a host")

        secure = parts.scheme == "https"
        host = parts.hostname
        port = parts.port or (443 if secure else 80)
        target = parts.path or "/"
        if parts.query:
            target = f"{target}?{parts.query}"

        request = self._build(method=method, host=host, port=port, secure=secure, target=target, headers=headers, body=body)

        try:
            async with asyncio.timeout(self.timeout):
                return await self._exchange(host=host, port=port, secure=secure, request=request)
        except TimeoutError as error:
            raise HttpError(f"{method} {url} timed out after {self.timeout} seconds") from error
        except (OSError, asyncio.IncompleteReadError, asyncio.LimitOverrunError) as error:
            raise HttpError(f"{method} {url} failed: {error}") from error

    def _build(
        self,
        *,
        method: str,
        host: str,
        port: int,
        secure: bool,
        target: str,
        headers: dict[str, str] | None,
        body: bytes | None
    ) -> bytes:
        """
        Assembles the request line, the headers and the body.
        """
        authority = host if port == (443 if secure else 80) else f"{host}:{port}"
        lines = [f"{method} {target} HTTP/1.1", f"Host: {authority}", f"User-Agent: {self.user_agent}", "Accept: */*", "Connection: close"]

        if headers:
            lines.extend(f"{name}: {value}" for name, value in headers.items())

        if body is not None:
            lines.append(f"Content-Length: {len(body)}")

        head = "\r\n".join(lines).encode("latin-1") + b"\r\n\r\n"

        return head + body if body is not None else head

    async def _exchange(self, *, host: str, port: int, secure: bool, request: bytes) -> HttpResponse:
        """
        Connects, sends the request and reads the response.
        """
        reader, writer = await asyncio.open_connection(host, port, ssl=self._ssl_context if secure else None, limit=_LINE_LIMIT)

        try:
            writer.write(request)
            await writer.drain()

            return await self._read(reader)
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass

    async def _read(self, reader: asyncio.StreamReader, /) -> HttpResponse:
        """
        Parses the status line and the headers, then reads the body the
        way the headers describe it.
        """
        status_line = (await reader.readline()).decode("latin-1").rstrip("\r\n")
        version, _, rest = status_line.partition(" ")
        code, _, reason = rest.partition(" ")
        if not version.startswith("HTTP/1.") or not code.isdigit():
            raise HttpError(f"response does not start with a status line: {status_line!r}")

        headers: dict[str, str] = {}

        while True:
            line = (await reader.readline()).decode("latin-1").rstrip("\r\n")
            if not line:
                break

            name, separator, value = line.partition(":")
            if not separator:
                raise HttpError(f"response header is malformed: {line!r}")

            name = name.strip().lower()
            value = value.strip()
            headers[name] = f"{headers[name]}, {value}" if name in headers else value

        status = int(code)
        if status == 204 or status == 304 or 100 <= status < 200:
            body = b""
        elif headers.get("transfer-encoding", "").lower() == "chunked":
            body = await self._read_chunked(reader)
        elif "content-length" in headers:
            body = await self._read_sized(reader, headers["content-length"])
        else:
            body = await reader.read(self.limit + 1)
            if len(body) > self.limit:
                raise HttpError(f"response body exceeds the limit of {self.limit} bytes")

        return HttpResponse(status=status, reason=reason, headers=headers, body=body)

    async def _read_sized(self, reader: asyncio.StreamReader, length: str, /) -> bytes:
        """
        Reads a body whose size the headers announced.
        """
        if not length.isdigit():
            raise HttpError(f"content-length is not a number: {length!r}")

        size = int(length)
        if size > self.limit:
            raise HttpError(f"response body of {size} bytes exceeds the limit of {self.limit} bytes")

        return await reader.readexactly(size)

    async def _read_chunked(self, reader: asyncio.StreamReader, /) -> bytes:
        """
        Reads a chunked body chunk by chunk, up to the limit.
        """
        chunks: list[bytes] = []
        total = 0

        while True:
            size_line = (await reader.readline()).decode("latin-1").strip()
            size_text, _, _ = size_line.partition(";")
            try:
                size = int(size_text, 16)
            except ValueError as error:
                raise HttpError(f"chunk size is malformed: {size_line!r}") from error

            if size == 0:
                break

            total += size
            if total > self.limit:
                raise HttpError(f"response body exceeds the limit of {self.limit} bytes")

            chunks.append(await reader.readexactly(size))
            await reader.readexactly(2)

        while (await reader.readline()).rstrip(b"\r\n"):
            pass

        return b"".join(chunks)

__all__ = ["HttpResponse", "HttpClient"]
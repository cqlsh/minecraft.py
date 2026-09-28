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

import argparse
import base64
import hashlib
import io
import os
import re
import shlex
import shutil
import subprocess
import sys
import sysconfig
import tarfile
import threading
import tomllib
import zipfile
from abc import ABC, abstractmethod
from collections.abc import Iterator, Mapping, Sequence
from concurrent.futures import Future, ThreadPoolExecutor
from pathlib import Path
from typing import Any

ROOT: Path = Path(__file__).resolve().parent.parent
ZIP_EPOCH: tuple[int, int, int, int, int, int] = (1980, 1, 1, 0, 0, 0)
VERSION_PATTERN: re.Pattern[str] = re.compile(r"^__version__\s*(?::[^=]+)?=\s*['\"]([^'\"]+)['\"]", re.MULTILINE)
SOURCE_SUFFIXES: frozenset[str] = frozenset({'.py', '.pyi'})
HEADER_SUFFIXES: frozenset[str] = frozenset({'.h', '.hpp'})
README_CONTENT_TYPES: dict[str, str] = {'.rst': 'text/x-rst', '.md': 'text/markdown'}
EDITABLE_FINDER: str = '''import sys
from collections.abc import Sequence
from importlib.abc import MetaPathFinder
from importlib.machinery import ModuleSpec, PathFinder
from types import ModuleType

class EditableFinder(MetaPathFinder):
    """Resolves the top-level packages of an editable install to their source tree."""

    __slots__ = ('_root', '_packages')

    def __init__(self, root: str, packages: frozenset[str]) -> None:
        self._root: str = root
        self._packages: frozenset[str] = packages

    def find_spec(
        self, fullname: str, path: Sequence[str] | None = None, target: ModuleType | None = None
    ) -> ModuleSpec | None:
        if fullname not in self._packages:
            return None

        return PathFinder.find_spec(fullname, [self._root])

sys.meta_path.insert(0, EditableFinder({root!r}, frozenset({packages!r})))'''

class ProjectMetadata:
    """The ``[project]`` table of ``pyproject.toml`` rendered as core metadata 2.4.

    Parameters
    -----------
    project: Mapping[:class:`str`, Any]
        The raw ``[project]`` table.
    version: :class:`str`
        The resolved version, read from the ``version-file``.
    """

    __slots__ = (
        'name',
        'version',
        'summary',
        'readme',
        'license',
        'license_files',
        'requires_python',
        'authors',
        'keywords',
        'classifiers',
        'urls'
    )

    def __init__(self, project: Mapping[str, Any], version: str) -> None:
        self.name: str = project['name']
        self.version: str = version
        self.summary: str | None = project.get('description')
        self.readme: str | None = project.get('readme')
        self.license: str | None = project.get('license')
        self.license_files: list[str] = list(project.get('license-files', []))
        self.requires_python: str | None = project.get('requires-python')
        self.authors: list[dict[str, str]] = list(project.get('authors', []))
        self.keywords: list[str] = list(project.get('keywords', []))
        self.classifiers: list[str] = list(project.get('classifiers', []))
        self.urls: dict[str, str] = dict(project.get('urls', {}))

    @property
    def distribution_name(self) -> str:
        """:class:`str`: The name normalised for file names, e.g. ``minecraft_py``."""
        return re.sub(r'[-_.]+', '_', self.name).lower()

    @property
    def dist_info_name(self) -> str:
        """:class:`str`: The name of the ``.dist-info`` directory."""
        return f'{self.distribution_name}-{self.version}.dist-info'

    def render(self, root: Path) -> str:
        """Renders the ``METADATA`` / ``PKG-INFO`` file.

        Parameters
        -----------
        root: :class:`~pathlib.Path`
            The project root, used to read the readme.

        Returns
        --------
        :class:`str`
            The rendered metadata including the long description.
        """
        lines: list[str] = ['Metadata-Version: 2.4', f'Name: {self.name}', f'Version: {self.version}']
        if self.summary is not None:
            lines.append(f'Summary: {self.summary}')

        for author in self.authors:
            name: str | None = author.get('name')
            email: str | None = author.get('email')
            if email is not None:
                lines.append(f'Author-email: {name} <{email}>' if name is not None else f'Author-email: {email}')
            elif name is not None:
                lines.append(f'Author: {name}')

        if self.license is not None:
            lines.append(f'License-Expression: {self.license}')
        for license_file in self.license_files:
            lines.append(f'License-File: {Path(license_file).name}')
        if self.keywords:
            lines.append(f'Keywords: {",".join(self.keywords)}')
        for classifier in self.classifiers:
            lines.append(f'Classifier: {classifier}')
        if self.requires_python is not None:
            lines.append(f'Requires-Python: {self.requires_python}')
        for label, url in self.urls.items():
            lines.append(f'Project-URL: {label}, {url}')
        if self.readme is None:
            return '\n'.join(lines) + '\n'

        content_type: str = README_CONTENT_TYPES.get(Path(self.readme).suffix, 'text/plain')
        lines.append(f'Description-Content-Type: {content_type}')
        return '\n'.join(lines) + '\n\n' + (root / self.readme).read_text(encoding='utf-8')

class Extension:
    """A C++ extension module described by ``[[tool.minecraft.build.extensions]]``.

    Parameters
    -----------
    table: Mapping[:class:`str`, Any]
        The raw extension table.
    root: :class:`~pathlib.Path`
        The project root that the globs are resolved against.
    """

    __slots__ = ('root', 'name', 'standard', 'sources', 'include_dirs')

    def __init__(self, table: Mapping[str, Any], root: Path) -> None:
        self.root: Path = root
        self.name: str = table['name']
        self.standard: str = table.get('standard', 'c++20')
        patterns: list[str] = list(table['sources'])
        self.sources: list[Path] = sorted({path for pattern in patterns for path in root.glob(pattern)})
        self.include_dirs: list[Path] = [root / directory for directory in table.get('include-dirs', [])]
        if not self.sources:
            raise FileNotFoundError(f'extension {self.name!r} has no sources matching {patterns!r}')

    @property
    def module_name(self) -> str:
        """:class:`str`: The last component of the dotted name, e.g. ``_speedups``."""
        return self.name.rpartition('.')[2]

    @property
    def package_path(self) -> Path:
        """:class:`~pathlib.Path`: The relative directory of the containing package."""
        return Path(*self.name.split('.')[:-1])

    @property
    def filename(self) -> str:
        """:class:`str`: The platform specific file name, e.g. ``_speedups.cp314-win_amd64.pyd``."""
        return self.module_name + sysconfig.get_config_var('EXT_SUFFIX')

    @property
    def relative_path(self) -> Path:
        """:class:`~pathlib.Path`: The file path inside a wheel or the source tree."""
        return self.package_path / self.filename

    def object_name(self, source: Path) -> str:
        """Returns the object file name of a source without suffix, e.g. ``native.speedups.util.vector``.

        The name is derived from the path relative to the project root, so
        sources with the same file name in different directories never collide.
        """
        return '.'.join(source.relative_to(self.root).with_suffix('').parts)

    def headers(self) -> Iterator[Path]:
        """Yields every header below the include directories."""
        for directory in self.include_dirs:
            for path in directory.rglob('*'):
                if path.suffix in HEADER_SUFFIXES:
                    yield path

class BuildConfig:
    """The fully parsed ``pyproject.toml``.

    Parameters
    -----------
    root: :class:`~pathlib.Path`
        The project root containing ``pyproject.toml``.
    """

    __slots__ = ('root', 'project', 'packages', 'extensions')

    def __init__(self, root: Path) -> None:
        with (root / 'pyproject.toml').open('rb') as fp:
            pyproject: dict[str, Any] = tomllib.load(fp)
        build: dict[str, Any] = pyproject['tool']['minecraft']['build']
        version: str = self._read_version(root / build['version-file'])
        self.root: Path = root
        self.project: ProjectMetadata = ProjectMetadata(pyproject['project'], version)
        self.packages: list[str] = list(build['packages'])
        self.extensions: list[Extension] = [Extension(table, root) for table in build.get('extensions', [])]

    @staticmethod
    def _read_version(path: Path) -> str:
        match: re.Match[str] | None = VERSION_PATTERN.search(path.read_text(encoding='utf-8'))
        if match is None:
            raise ValueError(f'no __version__ found in {path}')

        return match.group(1)

    def package_files(self) -> Iterator[Path]:
        """Yields every Python source, stub and ``py.typed`` marker of the configured packages."""
        for package in self.packages:
            for path in sorted((self.root / package).rglob('*')):
                if not path.is_file() or '__pycache__' in path.parts:
                    continue
                if path.suffix in SOURCE_SUFFIXES or path.name == 'py.typed':
                    yield path

class WheelTag:
    """A ``python-abi-platform`` compatibility tag.

    Parameters
    -----------
    python: :class:`str`
        The python tag, e.g. ``cp314``.
    abi: :class:`str`
        The ABI tag, e.g. ``cp314t`` for free-threaded builds.
    platform: :class:`str`
        The platform tag, e.g. ``win_amd64``.
    """

    __slots__ = ('python', 'abi', 'platform')

    def __init__(self, python: str, abi: str, platform: str) -> None:
        self.python: str = python
        self.abi: str = abi
        self.platform: str = platform

    def __str__(self) -> str:
        return f'{self.python}-{self.abi}-{self.platform}'

    @classmethod
    def pure(cls) -> WheelTag:
        """Returns the tag of a pure Python wheel."""
        return cls('py3', 'none', 'any')

    @classmethod
    def current(cls) -> WheelTag:
        """Returns the tag of the running interpreter, honouring free-threaded and debug builds."""
        python: str = f'cp{sys.version_info.major}{sys.version_info.minor}'
        abi: str = python
        if sysconfig.get_config_var('Py_GIL_DISABLED'):
            abi += 't'
        if hasattr(sys, 'gettotalrefcount'):
            abi += 'd'
        platform: str = sysconfig.get_platform().replace('-', '_').replace('.', '_')
        return cls(python, abi, platform)

class Compiler(ABC):
    """Base class of the C++ toolchain drivers.

    Parameters
    -----------
    build_dir: :class:`~pathlib.Path`
        The directory for object files and other intermediates.
    """

    __slots__ = ('build_dir', '_environment', '_lock')

    def __init__(self, build_dir: Path) -> None:
        self.build_dir: Path = build_dir
        self._environment: dict[str, str] | None = None
        self._lock: threading.Lock = threading.Lock()

    @staticmethod
    def detect(build_dir: Path) -> Compiler:
        """Returns the compiler driver matching the running platform."""
        if sys.platform == 'win32':
            return MSVCCompiler(build_dir)
        return UnixCompiler(build_dir)

    @property
    def environment(self) -> dict[str, str]:
        """Dict[:class:`str`, :class:`str`]: The environment the toolchain runs in, created on first use.

        The sources are compiled in several threads, the lock makes sure the environment is only created once.
        """
        with self._lock:
            if self._environment is None:
                self._environment = self.create_environment()
            return self._environment

    def create_environment(self) -> dict[str, str]:
        """Creates the toolchain environment, by default a copy of the current one."""
        return dict(os.environ)

    @property
    @abstractmethod
    def object_suffix(self) -> str:
        """:class:`str`: The file suffix of object files."""
        raise NotImplementedError

    @abstractmethod
    def compile_command(self, extension: Extension, source: Path, output: Path) -> list[str]:
        """Returns the command line that compiles a single source file."""
        raise NotImplementedError

    @abstractmethod
    def link_command(self, extension: Extension, objects: Sequence[Path], output: Path) -> list[str]:
        """Returns the command line that links the object files into the extension module."""
        raise NotImplementedError

    def include_dirs(self, extension: Extension) -> list[Path]:
        """Returns the include directories of the extension followed by the Python headers."""
        paths: dict[str, str] = sysconfig.get_paths()
        return [*extension.include_dirs, Path(paths['include']), Path(paths['platinclude'])]

    def build(self, extension: Extension, output_dir: Path) -> Path:
        """Compiles and links an extension, skipping object files that are still up to date.

        Parameters
        -----------
        extension: :class:`Extension`
            The extension to build.
        output_dir: :class:`~pathlib.Path`
            The directory that receives the finished module file.

        Returns
        --------
        :class:`~pathlib.Path`
            The path of the built module.
        """
        temp_dir: Path = self.build_dir / 'temp' / extension.name
        temp_dir.mkdir(parents=True, exist_ok=True)
        output: Path = output_dir / extension.filename
        output.parent.mkdir(parents=True, exist_ok=True)
        newest_header: float = max((header.stat().st_mtime for header in extension.headers()), default=0.0)
        objects: list[Path] = [
            temp_dir / (extension.object_name(source) + self.object_suffix) for source in extension.sources
        ]
        stale: list[tuple[Path, Path]] = [
            (source, obj) for source, obj in zip(extension.sources, objects)
            if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, newest_header)
        ]
        if not stale and output.exists() and all(output.stat().st_mtime >= obj.stat().st_mtime for obj in objects):
            return output

        with ThreadPoolExecutor() as executor:
            futures: list[Future[None]] = [executor.submit(self._compile, extension, source, obj) for source, obj in stale]
            for future in futures:
                future.result()

        print(f'linking {output.relative_to(ROOT) if output.is_relative_to(ROOT) else output}')
        self.run(self.link_command(extension, objects, output))
        return output

    def _compile(self, extension: Extension, source: Path, output: Path) -> None:
        print(f'compiling {source.relative_to(ROOT) if source.is_relative_to(ROOT) else source}')
        self.run(self.compile_command(extension, source, output))

    def run(self, command: Sequence[str]) -> None:
        """Runs a toolchain command in the compiler environment and fails loudly on errors."""
        executable: str | None = shutil.which(command[0], path=self.environment.get('PATH'))
        if executable is None:
            raise FileNotFoundError(f'{command[0]!r} was not found on PATH')

        result: subprocess.CompletedProcess[str] = subprocess.run(
            [executable, *command[1:]],
            env=self.environment,
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace'
        )

        if result.returncode != 0:
            output: str = result.stdout + result.stderr
            raise RuntimeError(f'command failed with exit code {result.returncode}:\n{shlex.join(command)}\n{output}')

class MSVCCompiler(Compiler):
    """Drives ``cl.exe`` and ``link.exe`` of Visual Studio 2022, locating them through ``vswhere`` when needed."""

    __slots__ = ()

    @property
    def object_suffix(self) -> str:
        return '.obj'

    def create_environment(self) -> dict[str, str]:
        if shutil.which('cl') is not None:
            return dict(os.environ)

        return self._developer_environment()

    @property
    def python_library(self) -> str:
        """:class:`str`: The import library of the running interpreter, e.g. ``python314t.lib``."""
        name: str = f'python{sys.version_info.major}{sys.version_info.minor}'
        if sysconfig.get_config_var('Py_GIL_DISABLED'):
            name += 't'
        if hasattr(sys, 'gettotalrefcount'):
            name += '_d'
        return name + '.lib'

    @staticmethod
    def _architecture() -> tuple[str, str]:
        platform: str = sysconfig.get_platform()
        if platform == 'win-arm64':
            return 'arm64', 'Microsoft.VisualStudio.Component.VC.Tools.ARM64'
        if platform == 'win32':
            return 'x86', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'

        return 'x64', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'

    @classmethod
    def _developer_environment(cls) -> dict[str, str]:
        architecture, component = cls._architecture()
        program_files: str = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
        vswhere: Path = Path(program_files) / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
        if not vswhere.exists():
            raise FileNotFoundError(
                'Visual Studio 2022 with the "Desktop development with C++" workload is required to build minecraft.py'
            )

        query: list[str] = [
            str(vswhere), '-latest', '-prerelease', '-products', '*', '-requires', component, '-property', 'installationPath'
        ]
        installation: str = subprocess.run(query, capture_output=True, text=True, check=True).stdout.strip()
        if not installation:
            raise FileNotFoundError('no Visual Studio installation with the C++ build tools was found')

        vcvarsall: Path = Path(installation) / 'VC' / 'Auxiliary' / 'Build' / 'vcvarsall.bat'
        output: str = subprocess.run(
            f'"{vcvarsall}" {architecture} >nul && set', shell=True, capture_output=True, text=True, check=True
        ).stdout
        environment: dict[str, str] = {}

        # Windows names are case-insensitive and terminals call the search path "Path", not "PATH"
        for line in output.splitlines():
            key, separator, value = line.partition('=')
            if separator and key:
                environment[key.upper()] = value

        return environment

    def compile_command(self, extension: Extension, source: Path, output: Path) -> list[str]:
        includes: list[str] = [f'/I{directory}' for directory in self.include_dirs(extension)]
        return [
            'cl', '/nologo', '/c', '/EHsc', '/O2', '/Oi', '/W4', '/permissive-', '/utf-8', '/MD', '/DNDEBUG',
            f'/std:{extension.standard}', '/Zc:__cplusplus', *includes, f'/Fo{output}', str(source)
        ]

    def link_command(self, extension: Extension, objects: Sequence[Path], output: Path) -> list[str]:
        libs: Path = Path(sys.base_prefix) / 'libs'
        implib: Path = self.build_dir / 'temp' / extension.name / f'{extension.module_name}.lib'
        return [
            'link', '/nologo', '/DLL', '/INCREMENTAL:NO', f'/OUT:{output}', f'/IMPLIB:{implib}', f'/LIBPATH:{libs}',
            self.python_library, *map(str, objects)
        ]

class UnixCompiler(Compiler):
    """Drives GCC or Clang through the ``CXX`` environment variable or the interpreter's build configuration."""

    __slots__ = ['cxx']

    def __init__(self, build_dir: Path) -> None:
        super().__init__(build_dir)
        cxx: str | None = os.environ.get('CXX') or sysconfig.get_config_var('CXX')
        self.cxx: list[str] = shlex.split(cxx) if cxx else ['c++']

    @property
    def object_suffix(self) -> str:
        return '.o'

    def compile_command(self, extension: Extension, source: Path, output: Path) -> list[str]:
        includes: list[str] = [f'-I{directory}' for directory in self.include_dirs(extension)]
        return [
            *self.cxx, f'-std={extension.standard}', '-O3', '-fPIC', '-fvisibility=hidden', '-ffp-contract=off', '-Wall',
            '-Wextra', '-DNDEBUG', *includes, '-c', str(source), '-o', str(output)
        ]

    def link_command(self, extension: Extension, objects: Sequence[Path], output: Path) -> list[str]:
        shared: list[str] = ['-bundle', '-undefined', 'dynamic_lookup'] if sys.platform == 'darwin' else ['-shared']
        return [*self.cxx, *shared, *map(str, objects), '-o', str(output)]

class WheelWriter:
    """Writes files into a wheel archive while accumulating the ``RECORD`` entries.

    Parameters
    -----------
    archive: :class:`~zipfile.ZipFile`
        The open wheel archive.
    """

    __slots__ = ('archive', 'records')

    def __init__(self, archive: zipfile.ZipFile) -> None:
        self.archive: zipfile.ZipFile = archive
        self.records: list[str] = []

    def add_bytes(self, name: str, data: bytes) -> None:
        """Adds in-memory data under the given archive name."""
        info: zipfile.ZipInfo = zipfile.ZipInfo(name, date_time=ZIP_EPOCH)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        self.archive.writestr(info, data)
        digest: str = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b'=').decode('ascii')
        self.records.append(f'{name},sha256={digest},{len(data)}')

    def add_text(self, name: str, text: str) -> None:
        """Adds UTF-8 text under the given archive name."""
        self.add_bytes(name, text.encode('utf-8'))

    def add_file(self, name: str, path: Path) -> None:
        """Adds a file from disk under the given archive name."""
        self.add_bytes(name, path.read_bytes())

    def finish(self, dist_info: str) -> None:
        """Writes the ``RECORD`` file, which must be the last entry of the archive."""
        name: str = f'{dist_info}/RECORD'
        self.archive.writestr(zipfile.ZipInfo(name, date_time=ZIP_EPOCH), '\n'.join([*self.records, f'{name},,']) + '\n')

class WheelBuilder:
    """Builds regular and editable wheels.

    Parameters
    -----------
    config: :class:`BuildConfig`
        The parsed project configuration.
    """

    __slots__ = ('config', 'compiler')

    def __init__(self, config: BuildConfig) -> None:
        self.config: BuildConfig = config
        self.compiler: Compiler = Compiler.detect(config.root / 'build')

    def build_extensions(self, output_root: Path) -> list[Path]:
        """Builds every extension below ``output_root`` and returns the module paths."""
        return [self.compiler.build(extension, output_root / extension.package_path) for extension in self.config.extensions]

    def write_dist_info(self, writer: WheelWriter, tag: WheelTag) -> None:
        """Writes ``METADATA``, ``WHEEL`` and the license files."""
        project: ProjectMetadata = self.config.project
        dist_info: str = project.dist_info_name
        writer.add_text(f'{dist_info}/METADATA', project.render(self.config.root))
        purelib: str = 'true' if tag.abi == 'none' else 'false'
        wheel: str = f'Wheel-Version: 1.0\nGenerator: minecraft.py\nRoot-Is-Purelib: {purelib}\nTag: {tag}\n'
        writer.add_text(f'{dist_info}/WHEEL', wheel)
        for license_file in project.license_files:
            writer.add_file(f'{dist_info}/licenses/{Path(license_file).name}', self.config.root / license_file)
        writer.finish(dist_info)

    def build(self, wheel_directory: Path) -> str:
        """Builds a binary wheel and returns its file name."""
        tag: WheelTag = WheelTag.current()
        filename: str = f'{self.config.project.distribution_name}-{self.config.project.version}-{tag}.whl'
        wheel_directory.mkdir(parents=True, exist_ok=True)
        lib_dir: Path = self.config.root / 'build' / 'lib'

        with zipfile.ZipFile(wheel_directory / filename, 'w', zipfile.ZIP_DEFLATED) as archive:
            writer: WheelWriter = WheelWriter(archive)
            for path in self.config.package_files():
                writer.add_file(path.relative_to(self.config.root).as_posix(), path)
            for module in self.build_extensions(lib_dir):
                writer.add_file(module.relative_to(lib_dir).as_posix(), module)
            self.write_dist_info(writer, tag)

        return filename

    def build_editable(self, wheel_directory: Path) -> str:
        """Builds the extensions in place and returns the file name of a wheel that links to the source tree."""
        tag: WheelTag = WheelTag.pure()
        project: ProjectMetadata = self.config.project
        filename: str = f'{project.distribution_name}-{project.version}-{tag}.whl'
        wheel_directory.mkdir(parents=True, exist_ok=True)
        self.build_extensions(self.config.root)
        module: str = f'_{project.distribution_name}_editable'
        finder: str = EDITABLE_FINDER.format(root=str(self.config.root), packages=set(self.config.packages))

        with zipfile.ZipFile(wheel_directory / filename, 'w', zipfile.ZIP_DEFLATED) as archive:
            writer: WheelWriter = WheelWriter(archive)
            writer.add_text(f'{module}.py', finder)
            writer.add_text(f'{module}.pth', f'import {module}\n')
            self.write_dist_info(writer, tag)

        return filename

    def prepare_metadata(self, metadata_directory: Path) -> str:
        """Writes only the ``.dist-info`` directory and returns its name."""
        dist_info: Path = metadata_directory / self.config.project.dist_info_name
        dist_info.mkdir(parents=True, exist_ok=True)
        (dist_info / 'METADATA').write_text(self.config.project.render(self.config.root), encoding='utf-8')
        return dist_info.name

class SdistBuilder:
    """Builds source distributions.

    Parameters
    -----------
    config: :class:`BuildConfig`
        The parsed project configuration.
    """

    __slots__ = ['config']

    def __init__(self, config: BuildConfig) -> None:
        self.config: BuildConfig = config

    def files(self) -> Iterator[Path]:
        """Yields every file that belongs into the source distribution."""
        root: Path = self.config.root
        yield root / 'pyproject.toml'

        for name in (self.config.project.readme, *self.config.project.license_files):
            if name is not None:
                yield root / name

        yield from self.config.package_files()

        for extension in self.config.extensions:
            yield from extension.sources
            yield from extension.headers()

        for directory in ('tools', 'tests'):
            if (root / directory).is_dir():
                yield from sorted(path for path in (root / directory).rglob('*.py') if '__pycache__' not in path.parts)

    def build(self, sdist_directory: Path) -> str:
        """Builds the ``.tar.gz`` archive and returns its file name."""
        project: ProjectMetadata = self.config.project
        base: str = f'{project.distribution_name}-{project.version}'
        filename: str = f'{base}.tar.gz'
        sdist_directory.mkdir(parents=True, exist_ok=True)

        with tarfile.open(sdist_directory / filename, 'w:gz', format=tarfile.PAX_FORMAT) as archive:
            for path in self.files():
                archive.add(path, arcname=f'{base}/{path.relative_to(self.config.root).as_posix()}')
            metadata: bytes = project.render(self.config.root).encode('utf-8')
            info: tarfile.TarInfo = tarfile.TarInfo(f'{base}/PKG-INFO')
            info.size = len(metadata)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(metadata))

        return filename

def get_requires_for_build_wheel(config_settings: Mapping[str, Any] | None = None) -> list[str]:
    return []

def get_requires_for_build_sdist(config_settings: Mapping[str, Any] | None = None) -> list[str]:
    return []

def get_requires_for_build_editable(config_settings: Mapping[str, Any] | None = None) -> list[str]:
    return []

def prepare_metadata_for_build_wheel(metadata_directory: str, config_settings: Mapping[str, Any] | None = None) -> str:
    return WheelBuilder(BuildConfig(ROOT)).prepare_metadata(Path(metadata_directory))

def prepare_metadata_for_build_editable(metadata_directory: str, config_settings: Mapping[str, Any] | None = None) -> str:
    return WheelBuilder(BuildConfig(ROOT)).prepare_metadata(Path(metadata_directory))

def build_wheel(
    wheel_directory: str, config_settings: Mapping[str, Any] | None = None, metadata_directory: str | None = None
) -> str:
    return WheelBuilder(BuildConfig(ROOT)).build(Path(wheel_directory))

def build_editable(
    wheel_directory: str, config_settings: Mapping[str, Any] | None = None, metadata_directory: str | None = None
) -> str:
    return WheelBuilder(BuildConfig(ROOT)).build_editable(Path(wheel_directory))

def build_sdist(sdist_directory: str, config_settings: Mapping[str, Any] | None = None) -> str:
    return SdistBuilder(BuildConfig(ROOT)).build(Path(sdist_directory))

def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description='Build minecraft.py without pip.')
    parser.add_argument('target', nargs='?', choices=('inplace', 'wheel', 'sdist'), default='inplace')
    parser.add_argument('--out', type=Path, default=ROOT / 'dist')
    arguments: argparse.Namespace = parser.parse_args()
    config: BuildConfig = BuildConfig(ROOT)

    if arguments.target == 'sdist':
        print(SdistBuilder(config).build(arguments.out))
    elif arguments.target == 'wheel':
        print(WheelBuilder(config).build(arguments.out))
    else:
        for module in WheelBuilder(config).build_extensions(ROOT):
            print(module.relative_to(ROOT))

if __name__ == '__main__':
    main()

__all__ = (
    'build_wheel',
    'build_sdist',
    'build_editable',
    'get_requires_for_build_wheel',
    'get_requires_for_build_sdist',
    'get_requires_for_build_editable',
    'prepare_metadata_for_build_wheel',
    'prepare_metadata_for_build_editable'
)
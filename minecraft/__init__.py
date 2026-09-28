"""
Minecraft Plugin Framework
~~~~~~~~~~~~~~~~~~~~~~~~~~~

A framework for writing Minecraft server plugins in Python, for Java and Bedrock Edition.

:copyright: (c) 2026-present cqlsh
:license: MIT, see LICENSE for more details.
"""

import logging
from typing import Literal, NamedTuple

__title__: str = 'minecraft'
__author__: str = 'cqlsh'
__license__: str = 'MIT'
__copyright__: str = 'Copyright 2026-present cqlsh'
__version__: str = '0.1.0a0'

class VersionInfo(NamedTuple):
    """The version of minecraft.py as a comparable tuple, like :data:`sys.version_info`."""

    major: int
    minor: int
    micro: int
    releaselevel: Literal['alpha', 'beta', 'candidate', 'final']
    serial: int

version_info: VersionInfo = VersionInfo(major=0, minor=1, micro=0, releaselevel='alpha', serial=0)

logging.getLogger(__name__).addHandler(logging.NullHandler())
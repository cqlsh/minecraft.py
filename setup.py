import sys

from setuptools import Extension, setup
from glob import glob

standard = ["/std:c++20"] if sys.platform == "win32" else ["-std=c++20"]

setup(
    ext_modules=[
        Extension(
            name="minecraft._native",
            sources=sorted(glob("native/src/*.cpp")),
            depends=sorted(glob("native/include/minecraft/*.hpp")),
            include_dirs=["native/include"],
            language="c++",
            extra_compile_args=standard
        )
    ]
)
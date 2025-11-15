"""
Setup script for dash-em Python package
"""

from setuptools import setup, Extension
import os
import platform
import sys

def get_compile_args():
    """Get platform-specific compile arguments"""
    args = ['-O3', '-Wall', '-Wextra']

    # Use portable x86-64 baseline instead of -march=native for wheel compatibility
    # This ensures wheels can run on any x86-64 CPU, not just the build machine
    if platform.machine() in ('x86_64', 'AMD64'):
        args.append('-march=x86-64')

    # Windows/MSVC compatibility
    if sys.platform == 'win32':
        # MSVC doesn't support GCC-style flags
        args = ['/O2', '/W3']
        if platform.machine() in ('x86_64', 'AMD64'):
            args.append('/arch:AVX2')
        return args

    return args

# Resolve paths relative to this file
base_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.join(base_dir, '..', '..', 'src')

# Build the C extension
ext_modules = [
    Extension(
        'dashem_native',
        sources=[
            os.path.join(src_dir, 'dashem.c'),
            os.path.join(base_dir, 'dashem_native.c'),
        ],
        include_dirs=[src_dir],
        extra_compile_args=get_compile_args(),
        language='c',
    )
]

setup(
    py_modules=['dashem'],
    ext_modules=ext_modules,
)

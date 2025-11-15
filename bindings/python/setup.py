"""
Setup script for dash-em Python package
"""

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import os
import sys

# Build the C extension
ext_modules = [
    Extension(
        'dashem_native',
        sources=['../../src/dashem.c', 'dashem_native.c'],
        include_dirs=['../../src'],
        extra_compile_args=['-O3', '-march=native', '-Wall', '-Wextra'],
        language='c',
    )
]

setup(
    name='dash-em',
    version='1.0.0',
    description='Enterprise-Grade Em-Dash Removal Library — SIMD-Accelerated String Processing',
    long_description=open(os.path.join(os.path.dirname(__file__), '..', '..', 'README.md')).read(),
    long_description_content_type='text/markdown',
    author='Gaurav Gosain',
    license='MIT',
    py_modules=['dashem'],
    ext_modules=ext_modules,
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Operating System :: OS Independent',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    python_requires='>=3.6',
    keywords=['em-dash', 'string', 'simd', 'performance', 'unicode'],
)

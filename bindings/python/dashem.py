"""
@module dashem
@brief Enterprise-Grade Em-Dash Removal Library for Python

High-performance, SIMD-accelerated string processing library for removing
em-dashes (U+2014) from UTF-8 encoded text.
"""

import ctypes
import os
import platform
import sys
from typing import Optional

__version__ = "1.0.0"


class DashemError(Exception):
    """Base exception for dashem library errors"""
    pass


class _DashemLib:
    """Wrapper for the native C library"""

    def __init__(self):
        """Initialize the library by loading the shared object"""
        self.lib = self._load_library()

        # Set up function signatures
        self.lib.dashem_remove.argtypes = [
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self.lib.dashem_remove.restype = ctypes.c_int

        self.lib.dashem_version.argtypes = []
        self.lib.dashem_version.restype = ctypes.c_char_p

        self.lib.dashem_implementation_name.argtypes = []
        self.lib.dashem_implementation_name.restype = ctypes.c_char_p

        self.lib.dashem_detect_cpu_features.argtypes = []
        self.lib.dashem_detect_cpu_features.restype = ctypes.c_uint32

    @staticmethod
    def _load_library() -> ctypes.CDLL:
        """Load the native library"""
        system = platform.system()
        machine = platform.machine()

        # Determine library name and paths
        if system == "Windows":
            lib_name = "dashem.dll"
        elif system == "Darwin":
            lib_name = "libdashem.dylib"
        else:
            lib_name = "libdashem.so"

        # Try common install locations
        search_paths = [
            os.path.dirname(__file__),
            os.path.join(os.path.dirname(__file__), "..", "..", "build"),
            "/usr/local/lib",
            "/usr/lib",
        ]

        for path in search_paths:
            lib_path = os.path.join(path, lib_name)
            if os.path.exists(lib_path):
                try:
                    return ctypes.CDLL(lib_path)
                except OSError:
                    continue

        raise DashemError(
            f"Could not find native library {lib_name}. "
            "Make sure dash-em is properly installed."
        )

    def remove(self, input_str: str) -> str:
        """Remove em-dashes from a string"""
        if not isinstance(input_str, str):
            raise TypeError("Input must be a string")

        # Encode to UTF-8
        input_bytes = input_str.encode("utf-8")
        output_buffer = ctypes.create_string_buffer(len(input_bytes))
        output_len = ctypes.c_size_t()

        # Call C function
        result = self.lib.dashem_remove(
            input_bytes,
            len(input_bytes),
            output_buffer,
            len(input_bytes),
            ctypes.byref(output_len),
        )

        if result != 0:
            raise DashemError(f"dashem_remove failed with code {result}")

        # Extract result
        return output_buffer.raw[: output_len.value].decode("utf-8")

    def version(self) -> str:
        """Get library version"""
        version_bytes = self.lib.dashem_version()
        return version_bytes.decode("utf-8")

    def implementation_name(self) -> str:
        """Get implementation name"""
        impl_bytes = self.lib.dashem_implementation_name()
        return impl_bytes.decode("utf-8")

    def detect_cpu_features(self) -> int:
        """Detect available CPU features"""
        return self.lib.dashem_detect_cpu_features()


# Global library instance
_lib = _DashemLib()


def remove(input_str: str) -> str:
    """
    Remove em-dashes from a UTF-8 string.

    Args:
        input_str: Input string

    Returns:
        String with em-dashes removed

    Raises:
        TypeError: If input is not a string
        DashemError: If removal fails
    """
    return _lib.remove(input_str)


def version() -> str:
    """Get library version"""
    return _lib.version()


def implementation_name() -> str:
    """Get implementation name (e.g., "AVX2", "SSE4.2")"""
    return _lib.implementation_name()


def detect_cpu_features() -> int:
    """Detect available CPU features"""
    return _lib.detect_cpu_features()

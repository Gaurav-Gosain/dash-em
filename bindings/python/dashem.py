"""
@module dashem
@brief Enterprise-Grade Em-Dash Removal Library for Python

High-performance, SIMD-accelerated string processing library for removing
em-dashes (U+2014) from UTF-8 encoded text.
"""

import dashem_native

__version__ = "1.0.0"


def remove(input_str: str) -> str:
    """
    Remove em-dashes from a UTF-8 string.

    Args:
        input_str: Input string (must be a string, not bytes)

    Returns:
        String with em-dashes removed

    Raises:
        TypeError: If input is not a string
        RuntimeError: If removal fails (should not happen with valid UTF-8)
    """
    if not isinstance(input_str, str):
        raise TypeError(f"Input must be a string, not {type(input_str).__name__}")
    return dashem_native.remove(input_str)


def version() -> str:
    """
    Get the library version.

    Returns:
        Version string (e.g., "1.0.0")
    """
    return dashem_native.version()


def implementation_name() -> str:
    """
    Get the SIMD implementation used at runtime.

    Returns:
        Implementation name (e.g., "AVX2", "SSE4.2", "NEON", "scalar")
    """
    return dashem_native.implementation_name()


def detect_cpu_features() -> int:
    """
    Detect available CPU features.

    Returns:
        Bitfield of detected CPU features
    """
    return dashem_native.detect_cpu_features()


__all__ = ['remove', 'version', 'implementation_name', 'detect_cpu_features']

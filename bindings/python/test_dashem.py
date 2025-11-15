#!/usr/bin/env python3
"""
Test suite for dash-em Python binding
"""

import sys
import dashem


def test_version():
    """Test: Get version"""
    print("Test 1: Version")
    version = dashem.version()
    print(f"[PASS] Version: {version}\n")
    assert version, "Version should not be empty"


def test_implementation():
    """Test: Get implementation name"""
    print("Test 2: Implementation")
    impl = dashem.implementation_name()
    print(f"[PASS] Implementation: {impl}\n")
    valid_impls = ("AVX2", "SSE4.2", "NEON", "scalar", "Scalar")
    assert impl in valid_impls, f"Unknown implementation: {impl}"


def test_single_em_dash():
    """Test: Remove single em-dash"""
    print("Test 3: Remove single em-dash")
    result = dashem.remove("Hello—world")
    print(f'Input:  "Hello—world"')
    print(f'Output: "{result}"')
    assert result == "Helloworld", "Should remove em-dash"
    print("[PASS] Pass\n")


def test_multiple_em_dashes():
    """Test: Remove multiple em-dashes"""
    print("Test 4: Remove multiple em-dashes")
    result = dashem.remove("First—second—third—fourth")
    print(f'Input:  "First—second—third—fourth"')
    print(f'Output: "{result}"')
    assert result == "Firstsecondthirdfourth", "Should remove all em-dashes"
    print("[PASS] Pass\n")


def test_no_em_dashes():
    """Test: String without em-dashes"""
    print("Test 5: String without em-dashes")
    result = dashem.remove("Hello, world!")
    print(f'Input:  "Hello, world!"')
    print(f'Output: "{result}"')
    assert result == "Hello, world!", "Should not modify string without em-dashes"
    print("[PASS] Pass\n")


def test_empty_string():
    """Test: Empty string"""
    print("Test 6: Empty string")
    result = dashem.remove("")
    print(f'Input:  ""')
    print(f'Output: "{result}"')
    assert result == "", "Should handle empty string"
    print("[PASS] Pass\n")


def test_unicode_text():
    """Test: Unicode text with em-dashes"""
    print("Test 7: Unicode text with em-dashes")
    result = dashem.remove("Hello—世界—мир")
    print(f'Input:  "Hello—世界—мир"')
    print(f'Output: "{result}"')
    assert result == "Hello世界мир", "Should remove em-dashes while preserving other Unicode"
    print("[PASS] Pass\n")


def test_type_error():
    """Test: Type checking"""
    print("Test 8: Type error handling")
    try:
        dashem.remove(123)
        assert False, "Should raise TypeError for non-string input"
    except TypeError as e:
        print(f"[PASS] Correctly raised TypeError: {e}\n")


def test_cpu_features():
    """Test: CPU feature detection"""
    print("Test 9: CPU features")
    features = dashem.detect_cpu_features()
    print(f"[PASS] CPU Features: 0x{features:08x}\n")
    assert isinstance(features, int), "Features should be an integer"


if __name__ == "__main__":
    try:
        print("Testing dash-em Python binding...\n")
        test_version()
        test_implementation()
        test_single_em_dash()
        test_multiple_em_dashes()
        test_no_em_dashes()
        test_empty_string()
        test_unicode_text()
        test_type_error()
        test_cpu_features()
        print("=" * 32)
        print("[PASS] All tests passed!\n")
        sys.exit(0)
    except AssertionError as e:
        print(f"\n[FAIL] Test failed: {e}\n")
        sys.exit(1)
    except Exception as e:
        print(f"\n[FAIL] Error: {e}\n")
        import traceback
        traceback.print_exc()
        sys.exit(1)

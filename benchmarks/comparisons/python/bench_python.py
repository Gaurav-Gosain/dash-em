#!/usr/bin/env python3
"""
Python native implementation benchmarks for dash-em comparison.

This benchmark compares:
1. Native Python str.replace()
2. Python regex implementation
3. Manual loop implementation
4. dash-em Python binding (if available)

Outputs JSON results compatible with the main benchmark suite.
"""

import json
import time
import re
import statistics
import sys
import os
from typing import List, Dict, Any, Tuple
import argparse

# Try to import dash-em Python binding
try:
    # Add the build directory to path for testing
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../bindings/python'))
    import dashem
    DASHEM_AVAILABLE = True
except ImportError:
    DASHEM_AVAILABLE = False
    print("Warning: dash-em Python binding not available. Install with: pip install dash-em", file=sys.stderr)

# Em-dash character
EM_DASH = '\u2014'
EM_DASH_UTF8 = EM_DASH.encode('utf-8')

# Benchmark configuration
WARMUP_RUNS = 10
MIN_RUNS = 100
MAX_RUNS = 1000
MIN_DURATION_MS = 1000  # Run for at least 1 second


class BenchmarkResult:
    """Container for benchmark results with statistical analysis."""

    def __init__(self, name: str, method: str):
        self.name = name
        self.method = method
        self.timings: List[float] = []
        self.input_size = 0
        self.output_size = 0
        self.emdash_count = 0
        self.correct = True
        self.error = None

    def add_timing(self, duration: float):
        """Add a timing measurement in seconds."""
        self.timings.append(duration * 1000000)  # Convert to microseconds

    def calculate_statistics(self):
        """Calculate statistical metrics from timings."""
        if not self.timings:
            return {}

        sorted_timings = sorted(self.timings)
        n = len(sorted_timings)

        return {
            'mean': statistics.mean(sorted_timings),
            'median': statistics.median(sorted_timings),
            'stddev': statistics.stdev(sorted_timings) if n > 1 else 0,
            'min': sorted_timings[0],
            'max': sorted_timings[-1],
            'p95': sorted_timings[int(n * 0.95)],
            'p99': sorted_timings[int(n * 0.99)],
            'p999': sorted_timings[int(n * 0.999)] if n >= 1000 else sorted_timings[-1],
        }

    def calculate_throughput(self, timing_us: float) -> float:
        """Calculate throughput in GB/s."""
        if timing_us <= 0:
            return 0
        bytes_per_gb = 1024 ** 3
        return (self.input_size / bytes_per_gb) / (timing_us / 1000000)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON output."""
        stats = self.calculate_statistics()

        return {
            'name': self.name,
            'method': self.method,
            'input_size': self.input_size,
            'output_size': self.output_size,
            'emdash_count': self.emdash_count,
            'runs': len(self.timings),
            'correct': self.correct,
            'error': self.error,
            'timing_us': stats,
            'throughput_gbps': {
                'mean': self.calculate_throughput(stats.get('mean', 0)),
                'p50': self.calculate_throughput(stats.get('median', 0)),
                'p95': self.calculate_throughput(stats.get('p95', 0)),
            }
        }


def remove_emdash_str_replace(text: str) -> str:
    """Remove em-dashes using str.replace()."""
    return text.replace(EM_DASH, '')


def remove_emdash_regex(text: str, pattern: re.Pattern) -> str:
    """Remove em-dashes using regex."""
    return pattern.sub('', text)


def remove_emdash_manual(text: str) -> str:
    """Remove em-dashes using manual character iteration."""
    result = []
    skip = 0

    for i, char in enumerate(text):
        if skip > 0:
            skip -= 1
            continue

        if char == EM_DASH:
            # Em-dash found, skip it
            continue
        else:
            result.append(char)

    return ''.join(result)


def remove_emdash_manual_bytes(data: bytes) -> bytes:
    """Remove em-dashes at the byte level."""
    result = bytearray()
    i = 0

    while i < len(data):
        if i + 3 <= len(data) and data[i:i+3] == EM_DASH_UTF8:
            i += 3  # Skip the em-dash
        else:
            result.append(data[i])
            i += 1

    return bytes(result)


def remove_emdash_dashem(data: bytes) -> bytes:
    """Remove em-dashes using dash-em binding."""
    if not DASHEM_AVAILABLE:
        raise ImportError("dash-em not available")
    # dashem.remove() expects str and returns str, so decode/encode
    text = data.decode('utf-8')
    result = dashem.remove(text)
    return result.encode('utf-8')


def count_emdashes(text: str) -> int:
    """Count em-dashes in text."""
    return text.count(EM_DASH)


def generate_test_data(pattern: str) -> Tuple[str, bytes, int]:
    """Generate test data for a specific pattern."""

    if pattern == 'no_emdash':
        # No em-dashes - pure ASCII text
        text = 'a' * 1000000

    elif pattern == 'sparse':
        # 0.1% em-dashes
        parts = []
        for i in range(1000):
            parts.append('a' * 999)
            if i % 10 == 0:
                parts.append(EM_DASH)
        text = ''.join(parts)

    elif pattern == 'moderate':
        # 1% em-dashes
        parts = []
        for i in range(1000):
            parts.append('a' * 99)
            parts.append(EM_DASH)
        text = ''.join(parts)

    elif pattern == 'dense':
        # 25% em-dashes
        parts = []
        for i in range(10000):
            parts.append(EM_DASH)
            parts.append('abc')
        text = ''.join(parts)

    elif pattern == 'alternating':
        # Worst case - alternating
        parts = []
        for i in range(10000):
            parts.append(EM_DASH)
            parts.append('a')
        text = ''.join(parts)

    elif pattern == 'real_text':
        # Realistic text with natural em-dash usage
        text = """
        The history of computing—from its humble beginnings to today—is fascinating.
        Charles Babbage—often called the 'father of computing'—designed the first
        mechanical computer. His machine—though never fully built—contained all
        the fundamental principles we use today.

        Modern computers—whether desktop, laptop, or mobile—all share common
        architectures. The von Neumann architecture—named after John von Neumann—
        remains the basis for most computers. This design—which separates memory
        from processing—has proven remarkably durable.
        """ * 1000

    else:
        raise ValueError(f"Unknown pattern: {pattern}")

    emdash_count = count_emdashes(text)
    text_bytes = text.encode('utf-8')

    return text, text_bytes, emdash_count


def benchmark_method(method_name: str, method_func, input_data,
                    expected_output=None) -> BenchmarkResult:
    """Benchmark a single method."""

    result = BenchmarkResult(name=method_name, method=method_name)

    # Determine input size
    if isinstance(input_data, str):
        result.input_size = len(input_data.encode('utf-8'))
    else:
        result.input_size = len(input_data)

    # Warmup
    for _ in range(WARMUP_RUNS):
        try:
            output = method_func(input_data)
        except Exception as e:
            result.correct = False
            result.error = str(e)
            return result

    # Benchmark
    total_time = 0
    start_batch = time.perf_counter()

    while len(result.timings) < MAX_RUNS:
        start = time.perf_counter()
        output = method_func(input_data)
        end = time.perf_counter()

        result.add_timing(end - start)

        total_time = (time.perf_counter() - start_batch) * 1000  # ms

        if len(result.timings) >= MIN_RUNS and total_time >= MIN_DURATION_MS:
            break

    # Store output size
    if isinstance(output, str):
        result.output_size = len(output.encode('utf-8'))
    else:
        result.output_size = len(output)

    # Verify correctness if expected output provided
    if expected_output is not None:
        if output != expected_output:
            result.correct = False
            result.error = f"Output mismatch: got {len(output)} bytes/chars"

    return result


def run_benchmarks(pattern: str, verbose: bool = False) -> List[BenchmarkResult]:
    """Run all benchmarks for a given pattern."""

    if verbose:
        print(f"Generating test data for pattern: {pattern}")

    text, text_bytes, emdash_count = generate_test_data(pattern)

    if verbose:
        print(f"  Input size: {len(text_bytes)} bytes")
        print(f"  Em-dash count: {emdash_count}")

    # Expected output for validation
    expected_str = text.replace(EM_DASH, '')
    expected_bytes = expected_str.encode('utf-8')

    results = []

    # Benchmark str.replace()
    if verbose:
        print("  Benchmarking str.replace()...")
    result = benchmark_method(
        f"{pattern}_str_replace",
        lambda t: remove_emdash_str_replace(t),
        text,
        expected_str
    )
    result.emdash_count = emdash_count
    results.append(result)

    # Benchmark regex
    if verbose:
        print("  Benchmarking regex...")
    regex_pattern = re.compile(EM_DASH)
    result = benchmark_method(
        f"{pattern}_regex",
        lambda t: remove_emdash_regex(t, regex_pattern),
        text,
        expected_str
    )
    result.emdash_count = emdash_count
    results.append(result)

    # Benchmark manual string iteration
    if verbose:
        print("  Benchmarking manual string...")
    result = benchmark_method(
        f"{pattern}_manual_str",
        lambda t: remove_emdash_manual(t),
        text,
        expected_str
    )
    result.emdash_count = emdash_count
    results.append(result)

    # Benchmark manual bytes iteration
    if verbose:
        print("  Benchmarking manual bytes...")
    result = benchmark_method(
        f"{pattern}_manual_bytes",
        lambda b: remove_emdash_manual_bytes(b),
        text_bytes,
        expected_bytes
    )
    result.emdash_count = emdash_count
    results.append(result)

    # Benchmark dash-em if available
    if DASHEM_AVAILABLE:
        if verbose:
            print("  Benchmarking dash-em...")
        result = benchmark_method(
            f"{pattern}_dashem",
            lambda b: remove_emdash_dashem(b),
            text_bytes,
            expected_bytes
        )
        result.emdash_count = emdash_count
        results.append(result)

    return results


def output_json(results: List[BenchmarkResult], pretty: bool = False):
    """Output results as JSON."""

    output = {
        'language': 'python',
        'version': sys.version.split()[0],
        'dashem_available': DASHEM_AVAILABLE,
        'timestamp': int(time.time()),
        'benchmarks': [r.to_dict() for r in results]
    }

    if pretty:
        print(json.dumps(output, indent=2))
    else:
        print(json.dumps(output))


def output_table(results: List[BenchmarkResult]):
    """Output results as a formatted table."""

    print("\nPython Em-dash Removal Benchmarks")
    print("==================================")
    print(f"Python version: {sys.version.split()[0]}")
    print(f"dash-em available: {DASHEM_AVAILABLE}\n")

    print(f"{'Test':<25} {'Method':<15} {'Size':>10} {'Em-dash':>8} "
          f"{'Mean (μs)':>12} {'P95 (μs)':>12} {'GB/s':>8} {'Valid':>6}")
    print("-" * 100)

    for result in results:
        stats = result.calculate_statistics()
        print(f"{result.name:<25} {result.method:<15} {result.input_size:>10} "
              f"{result.emdash_count:>8} {stats['mean']:>12.1f} "
              f"{stats['p95']:>12.1f} {result.calculate_throughput(stats['mean']):>8.2f} "
              f"{'PASS' if result.correct else 'FAIL':>6}")

    # Calculate speedups if dash-em is available
    if DASHEM_AVAILABLE:
        print("\nSpeedup vs native methods (using dash-em):")
        print("-" * 50)

        dashem_results = {r.name: r for r in results if 'dashem' in r.method}
        native_results = {r.name.replace('_str_replace', ''): r
                         for r in results if 'str_replace' in r.method}

        for pattern in ['no_emdash', 'sparse', 'moderate', 'dense', 'alternating']:
            if f"{pattern}_dashem" in dashem_results and pattern in native_results:
                dashem_stats = dashem_results[f"{pattern}_dashem"].calculate_statistics()
                native_stats = native_results[pattern].calculate_statistics()

                speedup = native_stats['median'] / dashem_stats['median']
                print(f"  {pattern:<15}: {speedup:>6.2f}x faster")


def main():
    """Main entry point."""

    parser = argparse.ArgumentParser(description='Python em-dash removal benchmarks')
    parser.add_argument('--json', action='store_true',
                       help='Output results as compact JSON')
    parser.add_argument('--json-pretty', action='store_true',
                       help='Output results as formatted JSON')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show progress information')
    parser.add_argument('--patterns', nargs='+',
                       default=['no_emdash', 'sparse', 'moderate', 'dense', 'alternating'],
                       help='Patterns to test')

    args = parser.parse_args()

    # Run benchmarks for all patterns
    all_results = []
    for pattern in args.patterns:
        if args.verbose:
            print(f"\nRunning pattern: {pattern}")
        results = run_benchmarks(pattern, verbose=args.verbose)
        all_results.extend(results)

    # Output results
    if args.json or args.json_pretty:
        output_json(all_results, pretty=args.json_pretty)
    else:
        output_table(all_results)


if __name__ == '__main__':
    main()
#!/usr/bin/env python3
"""
Check for performance regressions between baseline and current results.

Exit with non-zero status if regression exceeds threshold.
"""

import json
import argparse
import sys
from pathlib import Path
from typing import Dict, Any, List, Tuple


def load_results(filepath: Path) -> Dict[str, Any]:
    """Load benchmark results from JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def extract_benchmarks(data: Dict[str, Any]) -> Dict[str, float]:
    """Extract benchmark throughput values by test name."""
    benchmarks = {}

    if 'benchmarks' in data:
        for bench in data['benchmarks']:
            name = bench['name']
            throughput = bench.get('throughput_gbps', {}).get('mean', 0)
            benchmarks[name] = throughput

    elif 'architectures' in data:
        # Aggregated results format
        for arch_name, arch_data in data['architectures'].items():
            if 'data' in arch_data and 'benchmarks' in arch_data['data']:
                for bench in arch_data['data']['benchmarks']:
                    name = bench['name']
                    throughput = bench.get('throughput_gbps', {}).get('mean', 0)
                    # Use first architecture found as baseline
                    if name not in benchmarks:
                        benchmarks[name] = throughput

    return benchmarks


def check_regressions(baseline: Dict[str, float],
                     current: Dict[str, float],
                     threshold: float) -> List[Tuple[str, float, float, float]]:
    """Check for performance regressions exceeding threshold.

    Returns list of (test_name, baseline_value, current_value, regression_pct).
    """
    regressions = []

    for test_name in baseline:
        if test_name in current:
            baseline_val = baseline[test_name]
            current_val = current[test_name]

            if baseline_val > 0:
                # Calculate regression percentage
                regression_pct = ((baseline_val - current_val) / baseline_val) * 100

                if regression_pct > threshold:
                    regressions.append((test_name, baseline_val, current_val, regression_pct))

    return regressions


def format_regression_report(regressions: List[Tuple[str, float, float, float]]) -> str:
    """Format regression report for display."""
    if not regressions:
        return "No performance regressions detected."

    report = ["Performance Regressions Detected:", ""]
    report.append(f"{'Test':<30} {'Baseline':>12} {'Current':>12} {'Regression':>12}")
    report.append("-" * 70)

    for test, baseline, current, regression in regressions:
        report.append(f"{test:<30} {baseline:>11.2f}  {current:>11.2f}  {regression:>11.1f}%")

    report.append("")
    report.append(f"Total regressions: {len(regressions)}")

    return "\n".join(report)


def main():
    parser = argparse.ArgumentParser(description='Check for performance regressions')
    parser.add_argument('--baseline', required=True, help='Baseline results JSON file')
    parser.add_argument('--current', required=True, help='Current results JSON file')
    parser.add_argument('--threshold', type=float, default=5.0,
                       help='Regression threshold percentage (default: 5.0)')
    parser.add_argument('--output', help='Output report file (optional)')

    args = parser.parse_args()

    # Load results
    baseline_path = Path(args.baseline)
    current_path = Path(args.current)

    if not baseline_path.exists():
        print(f"Error: Baseline file {baseline_path} does not exist")
        return 1

    if not current_path.exists():
        print(f"Error: Current file {current_path} does not exist")
        return 1

    baseline_data = load_results(baseline_path)
    current_data = load_results(current_path)

    # Extract benchmark values
    baseline_benchmarks = extract_benchmarks(baseline_data)
    current_benchmarks = extract_benchmarks(current_data)

    if not baseline_benchmarks:
        print("Warning: No benchmarks found in baseline results")
        return 0

    if not current_benchmarks:
        print("Error: No benchmarks found in current results")
        return 1

    # Check for regressions
    regressions = check_regressions(baseline_benchmarks, current_benchmarks, args.threshold)

    # Format report
    report = format_regression_report(regressions)
    print(report)

    # Write to file if specified
    if args.output:
        output_path = Path(args.output)
        with open(output_path, 'w') as f:
            f.write(report)

    # Exit with error if regressions found
    if regressions:
        print(f"\n[FAIL] Performance check failed: {len(regressions)} regression(s) exceed {args.threshold}% threshold")
        return 1
    else:
        print(f"\n[PASS] Performance check passed: No regressions exceed {args.threshold}% threshold")
        return 0


if __name__ == '__main__':
    sys.exit(main())
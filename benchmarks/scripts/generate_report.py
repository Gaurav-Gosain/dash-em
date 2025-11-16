#!/usr/bin/env python3
"""
Generate a markdown report from benchmark results.

Creates a comprehensive report with tables, charts, and analysis.
"""

import json
import argparse
from pathlib import Path
from typing import Dict, Any, List
from datetime import datetime


def load_results(filepath: Path) -> Dict[str, Any]:
    """Load benchmark results from JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def generate_header(data: Dict[str, Any]) -> List[str]:
    """Generate report header."""
    lines = [
        "# dash-em Performance Report",
        "",
        f"Generated: {datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S UTC')}",
        ""
    ]

    if 'summary' in data:
        summary = data['summary']
        lines.extend([
            "## Summary",
            "",
            f"- Architectures tested: {summary.get('architecture_count', 0)}",
            f"- Languages compared: {summary.get('language_count', 0)}",
            ""
        ])

        if 'best_throughput' in summary and summary['best_throughput']['value'] > 0:
            best = summary['best_throughput']
            lines.append(f"- Best throughput: **{best['value']:.2f} GB/s** "
                        f"({best['test']} on {best['architecture']})")

        if 'worst_throughput' in summary and summary['worst_throughput']['value'] < float('inf'):
            worst = summary['worst_throughput']
            lines.append(f"- Worst throughput: **{worst['value']:.2f} GB/s** "
                        f"({worst['test']} on {worst['architecture']})")

        lines.append("")

    return lines


def generate_architecture_table(data: Dict[str, Any]) -> List[str]:
    """Generate architecture comparison table."""
    lines = ["## Architecture Comparison", ""]

    if 'architectures' not in data or not data['architectures']:
        lines.append("*No architecture data available*")
        return lines

    # Collect all unique test names
    all_tests = set()
    for arch_data in data['architectures'].values():
        if 'data' in arch_data and 'benchmarks' in arch_data['data']:
            for bench in arch_data['data']['benchmarks']:
                all_tests.add(bench['name'])

    all_tests = sorted(all_tests)

    # Create table header
    header = ["Test"] + list(data['architectures'].keys())
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["-" * (len(h) + 2) for h in header]) + "|")

    # Fill table rows
    for test in all_tests:
        row = [test]

        for arch_name in data['architectures'].keys():
            arch_data = data['architectures'][arch_name]
            value = "N/A"

            if 'data' in arch_data and 'benchmarks' in arch_data['data']:
                for bench in arch_data['data']['benchmarks']:
                    if bench['name'] == test:
                        throughput = bench.get('throughput_gbps', {}).get('mean', 0)
                        speedup = bench.get('speedup_vs_naive', 1.0)
                        value = f"{throughput:.2f} GB/s ({speedup:.2f}x)"
                        break

            row.append(value)

        lines.append("| " + " | ".join(row) + " |")

    lines.append("")
    return lines


def format_time_us(time_us: float) -> str:
    """Format microsecond time with appropriate precision."""
    if time_us == 0:
        return "0.0"
    elif time_us < 0.01:
        return f"{time_us:.4f}"
    elif time_us < 1:
        return f"{time_us:.3f}"
    elif time_us < 10:
        return f"{time_us:.2f}"
    else:
        return f"{time_us:.1f}"


def generate_language_comparison(data: Dict[str, Any]) -> List[str]:
    """Generate language comparison section."""
    lines = ["## Language Comparison", ""]

    if 'languages' not in data or not data['languages']:
        lines.append("*No language comparison data available*")
        return lines

    # Create comparison table
    lines.extend([
        "### Native Method Performance vs dash-em",
        "",
        "| Language | Test Pattern | Native (μs) | dash-em (μs) | Speedup |",
        "|----------|--------------|-------------|--------------|---------|"
    ])

    for lang_name, lang_data in sorted(data['languages'].items()):
        if 'benchmarks' not in lang_data:
            continue

        # Find pairs of native and dashem results
        # Prefer bytes-level methods for fairer comparison
        native_results = {}
        dashem_results = {}
        str_replace_results = {}

        for bench in lang_data['benchmarks']:
            pattern = bench['name'].split('_')[0]
            if 'dashem' in bench['method']:
                dashem_results[pattern] = bench
            elif 'manual_bytes' in bench['method'] or 'buffer' in bench['method']:
                # Prefer bytes-level comparison (manual_bytes for Python, buffer for JS)
                native_results[pattern] = bench
            elif 'str_replace' in bench['method'] or 'replace' in bench['method']:
                str_replace_results[pattern] = bench  # Fallback to string replace

        # Use string replace as fallback if bytes-level not available
        for pattern, bench in str_replace_results.items():
            if pattern not in native_results:
                native_results[pattern] = bench

        # Compare results
        for pattern in sorted(set(native_results.keys()) & set(dashem_results.keys())):
            native = native_results[pattern]
            dashem = dashem_results[pattern]

            native_time = native.get('timing_us', {}).get('mean', 0)
            dashem_time = dashem.get('timing_us', {}).get('mean', 0)
            speedup = native_time / dashem_time if dashem_time > 0 else 0

            lines.append(f"| {lang_name} | {pattern} | {format_time_us(native_time)} | "
                        f"{format_time_us(dashem_time)} | {speedup:.2f}x |")

    lines.append("")
    return lines


def generate_pattern_analysis(data: Dict[str, Any]) -> List[str]:
    """Generate pattern-specific analysis."""
    lines = ["## Pattern Performance Analysis", ""]

    if 'summary' not in data or 'average_speedup' not in data['summary']:
        lines.append("*No pattern analysis available*")
        return lines

    lines.extend([
        "### Average Speedup by Pattern",
        "",
        "| Pattern | Average Speedup |",
        "|---------|-----------------|"
    ])

    for pattern, speedup in sorted(data['summary']['average_speedup'].items()):
        lines.append(f"| {pattern} | {speedup:.2f}x |")

    lines.append("")
    return lines


def generate_recommendations(data: Dict[str, Any]) -> List[str]:
    """Generate performance recommendations."""
    lines = ["## Recommendations", ""]

    recommendations = []

    # Analyze speedups
    if 'summary' in data and 'average_speedup' in data['summary']:
        speedups = data['summary']['average_speedup']

        # Check for poor performance patterns
        poor_patterns = [p for p, s in speedups.items() if s < 1.0]
        if poor_patterns:
            recommendations.append(f"- WARNING: Performance below naive implementation for patterns: {', '.join(poor_patterns)}")

        # Check for excellent performance
        excellent_patterns = [p for p, s in speedups.items() if s > 5.0]
        if excellent_patterns:
            recommendations.append(f"- EXCELLENT: Performance >5x speedup for patterns: {', '.join(excellent_patterns)}")

    # Architecture-specific recommendations
    if 'architectures' in data:
        arch_throughputs = {}
        for arch_name, arch_data in data['architectures'].items():
            if 'data' in arch_data and 'benchmarks' in arch_data['data']:
                avg_throughput = sum(b.get('throughput_gbps', {}).get('mean', 0)
                                   for b in arch_data['data']['benchmarks'])
                avg_throughput /= len(arch_data['data']['benchmarks'])
                arch_throughputs[arch_name] = avg_throughput

        if arch_throughputs:
            best_arch = max(arch_throughputs, key=arch_throughputs.get)
            worst_arch = min(arch_throughputs, key=arch_throughputs.get)

            recommendations.append(f"- BEST: Overall architecture: **{best_arch}** "
                                 f"({arch_throughputs[best_arch]:.2f} GB/s avg)")

            if best_arch != worst_arch:
                recommendations.append(f"- WORST: Performing architecture: **{worst_arch}** "
                                     f"({arch_throughputs[worst_arch]:.2f} GB/s avg)")

    if recommendations:
        lines.extend(recommendations)
    else:
        lines.append("*No specific recommendations at this time.*")

    lines.append("")
    return lines


def generate_report(data: Dict[str, Any]) -> str:
    """Generate complete markdown report."""
    sections = []

    sections.extend(generate_header(data))
    sections.extend(generate_architecture_table(data))
    sections.extend(generate_language_comparison(data))
    sections.extend(generate_pattern_analysis(data))
    sections.extend(generate_recommendations(data))

    # Footer
    sections.extend([
        "---",
        "*Generated by dash-em benchmark suite*"
    ])

    return "\n".join(sections)


def main():
    parser = argparse.ArgumentParser(description='Generate benchmark report')
    parser.add_argument('--input', required=True, help='Input JSON results file')
    parser.add_argument('--output', required=True, help='Output markdown report file')

    args = parser.parse_args()

    # Load results
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file {input_path} does not exist")
        return 1

    data = load_results(input_path)

    # Generate report
    report = generate_report(data)

    # Write output
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w') as f:
        f.write(report)

    print(f"Report generated: {output_path}")
    return 0


if __name__ == '__main__':
    exit(main())
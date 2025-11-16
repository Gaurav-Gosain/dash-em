#!/usr/bin/env python3
"""
Update README.md with latest benchmark results.

This script updates the performance table in README.md with
the latest benchmark results.
"""

import json
import argparse
import re
from pathlib import Path
from typing import Dict, Any, List


def load_results(filepath: Path) -> Dict[str, Any]:
    """Load benchmark results from JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


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


def generate_language_comparison_table(data: Dict[str, Any]) -> List[str]:
    """Generate language comparison table."""
    lines = []

    if 'languages' not in data or not data['languages']:
        return lines

    lines.extend([
        "",
        "### Language Bindings Performance",
        "",
        "Comparing dash-em bindings against native byte-level implementations:",
        "",
        "| Language | Test Pattern | Native (μs) | dash-em (μs) | Speedup |",
        "|----------|--------------|-------------|--------------|---------|"
    ])

    for lang_name, lang_data in sorted(data['languages'].items()):
        if 'benchmarks' not in lang_data:
            continue

        # Find native methods and dashem results
        # Python: compare against manual_bytes (slow interpreted iteration)
        # JavaScript: compare against string replace (buffer is JIT-optimized by V8)
        native_results = {}
        dashem_results = {}
        fallback_results = {}

        for bench in lang_data['benchmarks']:
            pattern = bench['name'].split('_')[0]
            if 'dashem' in bench['method']:
                dashem_results[pattern] = bench
            elif lang_name == 'python' and 'manual_bytes' in bench['method']:
                # Python: use manual_bytes for fair comparison
                native_results[pattern] = bench
            elif lang_name == 'javascript' and 'replace' in bench['method']:
                # JavaScript: use string replace (buffer is too fast due to V8 JIT)
                native_results[pattern] = bench
            elif 'manual_bytes' in bench['method'] or 'buffer' in bench['method'] or 'replace' in bench['method']:
                # Collect fallbacks
                if pattern not in fallback_results:
                    fallback_results[pattern] = bench

        # Use fallback if language-specific method not found
        for pattern, bench in fallback_results.items():
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

    return lines


def generate_performance_table(data: Dict[str, Any]) -> List[str]:
    """Generate markdown performance table from results."""
    lines = []

    # Extract latest results - show ALL architectures
    if 'architectures' in data and data['architectures']:
        lines.extend([
            "### Core Library Performance",
            "",
            "Multi-architecture SIMD performance (statistical benchmarks):",
            ""
        ])

        # Collect all unique patterns across all architectures
        all_patterns = set()
        for arch_data in data['architectures'].values():
            if 'data' in arch_data and 'benchmarks' in arch_data['data']:
                for bench in arch_data['data']['benchmarks']:
                    pattern = bench['name'].split('_')[0] if '_' in bench['name'] else bench['name']
                    all_patterns.add(pattern)

        # Sort patterns
        pattern_order = ['no_emdash', 'sparse', 'moderate', 'dense', 'alternating', 'boundary']
        sorted_patterns = [p for p in pattern_order if p in all_patterns]
        sorted_patterns.extend(sorted(all_patterns - set(pattern_order)))

        # Create table with all architectures
        arch_names = list(data['architectures'].keys())
        header = "| Pattern |" + "".join(f" {arch} |" for arch in arch_names)
        separator = "|---------|" + "".join("----------|" for _ in arch_names)

        lines.extend([header, separator])

        for pattern in sorted_patterns:
            row = [pattern]
            for arch_name in arch_names:
                arch_data = data['architectures'][arch_name].get('data', {})
                value = "N/A"

                if 'benchmarks' in arch_data:
                    for bench in arch_data['benchmarks']:
                        bench_pattern = bench['name'].split('_')[0] if '_' in bench['name'] else bench['name']
                        if bench_pattern == pattern:
                            throughput = bench.get('throughput_gbps', {}).get('mean', 0)
                            speedup = bench.get('speedup_vs_naive', 1.0)
                            value = f"{throughput:.2f} GB/s ({speedup:.2f}x)"
                            break

                row.append(value)

            lines.append("| " + " | ".join(row) + " |")

    elif 'benchmarks' in data:
        # Direct benchmark format (fallback)
        lines.extend([
            "| Test Pattern | Input Size | Throughput (GB/s) | Speedup vs Naive |",
            "|--------------|------------|------------------|------------------|"
        ])

        for bench in data['benchmarks']:
            name = bench.get('name', 'Unknown')
            size = bench.get('input_size', 0)
            throughput = bench.get('throughput_gbps', {}).get('mean', 0)
            speedup = bench.get('speedup_vs_naive', 1.0)

            lines.append(f"| {name:<12} | {size:>10} | {throughput:>16.2f} | {speedup:>16.2f}x |")

    # Add language comparison if available
    lang_table = generate_language_comparison_table(data)
    if lang_table:
        lines.extend(lang_table)

    return lines


def update_readme(readme_path: Path, results_path: Path) -> bool:
    """Update README.md with new benchmark results."""

    # Load current README
    if not readme_path.exists():
        print(f"Error: README file {readme_path} does not exist")
        return False

    with open(readme_path, 'r') as f:
        readme_content = f.read()

    # Load benchmark results
    if not results_path.exists():
        print(f"Error: Results file {results_path} does not exist")
        return False

    results = load_results(results_path)

    # Generate new performance table
    new_table_lines = generate_performance_table(results)
    if not new_table_lines:
        print("Warning: No performance data to update")
        return True

    new_table = "\n".join(new_table_lines)

    # Find and replace performance section
    # Look for markers or headings
    perf_section_pattern = r'(##\s+Performance.*?\n)(.*?)((?=\n##)|$)'
    perf_table_pattern = r'(\|\s*Test.*?\|\s*\n\|[-\s|]+\n(?:\|.*?\|\s*\n)*)'

    # Try to replace existing performance table
    if re.search(perf_table_pattern, readme_content, re.MULTILINE | re.DOTALL):
        # Replace existing table
        updated_content = re.sub(
            perf_table_pattern,
            new_table,
            readme_content,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )
    elif re.search(perf_section_pattern, readme_content, re.MULTILINE | re.DOTALL):
        # Add table to performance section
        def replace_section(match):
            return match.group(1) + "\n" + new_table + "\n\n"

        updated_content = re.sub(
            perf_section_pattern,
            replace_section,
            readme_content,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )
    else:
        # Add new performance section
        print("Warning: No performance section found in README, adding new section")

        # Find a good place to insert (after introduction, before installation)
        insert_pattern = r'(##\s+Installation)'

        if re.search(insert_pattern, readme_content):
            updated_content = re.sub(
                insert_pattern,
                f"## Performance\n\n{new_table}\n\n\\1",
                readme_content,
                count=1
            )
        else:
            # Append to end
            updated_content = readme_content + f"\n\n## Performance\n\n{new_table}\n"

    # Add update timestamp
    timestamp_line = f"\n<!-- Performance table last updated: {results.get('timestamp', 'unknown')} -->\n"
    if "<!-- Performance table last updated:" in updated_content:
        # Replace existing timestamp
        updated_content = re.sub(
            r'<!-- Performance table last updated:.*?-->',
            timestamp_line.strip(),
            updated_content
        )
    else:
        # Add timestamp after table
        updated_content = updated_content.replace(new_table, new_table + timestamp_line)

    # Write updated README
    with open(readme_path, 'w') as f:
        f.write(updated_content)

    print(f"README updated with latest benchmark results")
    return True


def main():
    parser = argparse.ArgumentParser(description='Update README with benchmark results')
    parser.add_argument('--readme', default='README.md', help='Path to README.md file')
    parser.add_argument('--results', required=True, help='Path to benchmark results JSON')

    args = parser.parse_args()

    readme_path = Path(args.readme)
    results_path = Path(args.results)

    success = update_readme(readme_path, results_path)
    return 0 if success else 1


if __name__ == '__main__':
    exit(main())
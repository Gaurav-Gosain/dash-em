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


def generate_performance_table(data: Dict[str, Any]) -> List[str]:
    """Generate markdown performance table from results."""
    lines = []

    # Extract latest results
    if 'architectures' in data:
        # Use first architecture as default
        arch_name = list(data['architectures'].keys())[0] if data['architectures'] else None

        if arch_name and 'data' in data['architectures'][arch_name]:
            arch_data = data['architectures'][arch_name]['data']

            lines.extend([
                "| Test Pattern | Input Size | Throughput (GB/s) | Speedup vs Naive | CPU Features |",
                "|--------------|------------|------------------|------------------|--------------|"
            ])

            if 'benchmarks' in arch_data:
                # Group benchmarks by pattern
                patterns = {}
                for bench in arch_data['benchmarks']:
                    pattern = bench['name'].split('_')[0] if '_' in bench['name'] else bench['name']
                    if pattern not in patterns:
                        patterns[pattern] = bench

                # Output in consistent order
                pattern_order = ['no_emdash', 'sparse', 'moderate', 'dense', 'alternating']
                for pattern in pattern_order:
                    if pattern in patterns:
                        bench = patterns[pattern]
                        size = bench.get('input_size', 0)
                        throughput = bench.get('throughput_gbps', {}).get('mean', 0)
                        speedup = bench.get('speedup_vs_naive', 1.0)
                        impl = arch_data.get('implementation', 'Unknown')

                        lines.append(f"| {pattern:<12} | {size:>10} | {throughput:>16.2f} | "
                                   f"{speedup:>16.2f}x | {impl:<12} |")

    elif 'benchmarks' in data:
        # Direct benchmark format
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
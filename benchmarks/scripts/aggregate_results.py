#!/usr/bin/env python3
"""
Aggregate benchmark results from multiple architecture runs.

This script combines JSON results from different architectures and
creates a unified benchmark report.
"""

import json
import argparse
import os
from pathlib import Path
from typing import Dict, List, Any
from datetime import datetime


def load_json_file(filepath: Path) -> Dict[str, Any]:
    """Load a JSON file and return its contents."""
    with open(filepath, 'r') as f:
        return json.load(f)


def aggregate_results(input_dir: Path) -> Dict[str, Any]:
    """Aggregate all benchmark results from the input directory."""

    aggregated = {
        'timestamp': datetime.utcnow().isoformat(),
        'architectures': {},
        'languages': {},
        'summary': {}
    }

    # Process all JSON files
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.json'):
                filepath = Path(root) / file
                data = load_json_file(filepath)

                # Determine result type
                if 'language' in data:
                    # Language comparison result
                    lang = data['language']
                    aggregated['languages'][lang] = data

                elif 'implementation' in data:
                    # Architecture-specific C benchmark
                    # Extract architecture info from filename
                    parts = file.replace('.json', '').split('-')

                    arch_info = {
                        'os': parts[0] if len(parts) > 0 else 'unknown',
                        'compiler': parts[1] if len(parts) > 1 else 'unknown',
                        'arch': parts[2] if len(parts) > 2 else 'unknown',
                        'data': data
                    }

                    arch_key = f"{arch_info['os']}-{arch_info['compiler']}-{arch_info['arch']}"
                    aggregated['architectures'][arch_key] = arch_info

    # Calculate summary statistics
    aggregated['summary'] = calculate_summary(aggregated)

    return aggregated


def calculate_summary(data: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate summary statistics across all benchmarks."""

    summary = {
        'best_throughput': {'value': 0, 'architecture': '', 'test': ''},
        'worst_throughput': {'value': float('inf'), 'architecture': '', 'test': ''},
        'average_speedup': {},
        'architecture_count': len(data['architectures']),
        'language_count': len(data['languages'])
    }

    # Find best and worst throughput
    for arch_name, arch_data in data['architectures'].items():
        if 'benchmarks' in arch_data['data']:
            for bench in arch_data['data']['benchmarks']:
                throughput = bench.get('throughput_gbps', {}).get('mean', 0)

                if throughput > summary['best_throughput']['value']:
                    summary['best_throughput'] = {
                        'value': throughput,
                        'architecture': arch_name,
                        'test': bench['name']
                    }

                if throughput < summary['worst_throughput']['value'] and throughput > 0:
                    summary['worst_throughput'] = {
                        'value': throughput,
                        'architecture': arch_name,
                        'test': bench['name']
                    }

    # Calculate average speedups by pattern
    speedup_by_pattern = {}
    speedup_counts = {}

    for arch_name, arch_data in data['architectures'].items():
        if 'benchmarks' in arch_data['data']:
            for bench in arch_data['data']['benchmarks']:
                pattern = bench['name'].split('_')[0] if '_' in bench['name'] else bench['name']
                speedup = bench.get('speedup_vs_naive', 1.0)

                if pattern not in speedup_by_pattern:
                    speedup_by_pattern[pattern] = 0
                    speedup_counts[pattern] = 0

                speedup_by_pattern[pattern] += speedup
                speedup_counts[pattern] += 1

    for pattern in speedup_by_pattern:
        if speedup_counts[pattern] > 0:
            summary['average_speedup'][pattern] = speedup_by_pattern[pattern] / speedup_counts[pattern]

    return summary


def main():
    parser = argparse.ArgumentParser(description='Aggregate benchmark results')
    parser.add_argument('--input', required=True, help='Input directory with JSON results')
    parser.add_argument('--output', required=True, help='Output JSON file')
    parser.add_argument('--pretty', action='store_true', help='Pretty print JSON')

    args = parser.parse_args()

    input_dir = Path(args.input)
    if not input_dir.exists():
        print(f"Error: Input directory {input_dir} does not exist")
        return 1

    # Aggregate results
    aggregated = aggregate_results(input_dir)

    # Write output
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w') as f:
        if args.pretty:
            json.dump(aggregated, f, indent=2, sort_keys=True)
        else:
            json.dump(aggregated, f)

    print(f"Aggregated {aggregated['summary']['architecture_count']} architectures and "
          f"{aggregated['summary']['language_count']} languages")
    print(f"Results written to {output_path}")

    return 0


if __name__ == '__main__':
    exit(main())
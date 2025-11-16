#!/usr/bin/env node

/**
 * JavaScript/Node.js native implementation benchmarks for dash-em comparison.
 *
 * This benchmark compares:
 * 1. Native String.replace() with string
 * 2. Native String.replace() with regex
 * 3. String.replaceAll() (Node.js 15+)
 * 4. Manual character iteration
 * 5. Buffer-level byte manipulation
 * 6. dash-em Node.js binding (if available)
 *
 * Outputs JSON results compatible with the main benchmark suite.
 */

const fs = require('fs');
const path = require('path');
const { performance } = require('perf_hooks');

// Try to import dash-em Node binding
let dashem = null;
let DASHEM_AVAILABLE = false;

try {
    // Try local build first
    dashem = require(path.join(__dirname, '../../../bindings/node/build/Release/dashem.node'));
    DASHEM_AVAILABLE = true;
} catch (e1) {
    try {
        // Try installed package
        dashem = require('dash-em');
        DASHEM_AVAILABLE = true;
    } catch (e2) {
        console.error('Warning: dash-em Node binding not available. Install with: npm install dash-em');
    }
}

// Em-dash character
const EM_DASH = '\u2014';
const EM_DASH_BYTES = Buffer.from(EM_DASH, 'utf8');

// Benchmark configuration
const WARMUP_RUNS = 10;
const MIN_RUNS = 100;
const MAX_RUNS = 1000;
const MIN_DURATION_MS = 1000; // Run for at least 1 second

class BenchmarkResult {
    constructor(name, method) {
        this.name = name;
        this.method = method;
        this.timings = [];
        this.inputSize = 0;
        this.outputSize = 0;
        this.emdashCount = 0;
        this.correct = true;
        this.error = null;
    }

    addTiming(durationMs) {
        this.timings.push(durationMs * 1000); // Convert to microseconds
    }

    calculateStatistics() {
        if (this.timings.length === 0) {
            return {};
        }

        const sorted = [...this.timings].sort((a, b) => a - b);
        const n = sorted.length;

        const sum = sorted.reduce((a, b) => a + b, 0);
        const mean = sum / n;

        // Calculate standard deviation
        const squaredDiffs = sorted.map(x => Math.pow(x - mean, 2));
        const avgSquaredDiff = squaredDiffs.reduce((a, b) => a + b, 0) / n;
        const stddev = Math.sqrt(avgSquaredDiff);

        return {
            mean: mean,
            median: sorted[Math.floor(n / 2)],
            stddev: stddev,
            min: sorted[0],
            max: sorted[n - 1],
            p95: sorted[Math.floor(n * 0.95)],
            p99: sorted[Math.floor(n * 0.99)],
            p999: n >= 1000 ? sorted[Math.floor(n * 0.999)] : sorted[n - 1]
        };
    }

    calculateThroughput(timingUs) {
        if (timingUs <= 0) return 0;
        const bytesPerGb = Math.pow(1024, 3);
        return (this.inputSize / bytesPerGb) / (timingUs / 1000000);
    }

    toJSON() {
        const stats = this.calculateStatistics();

        return {
            name: this.name,
            method: this.method,
            input_size: this.inputSize,
            output_size: this.outputSize,
            emdash_count: this.emdashCount,
            runs: this.timings.length,
            correct: this.correct,
            error: this.error,
            timing_us: stats,
            throughput_gbps: {
                mean: this.calculateThroughput(stats.mean || 0),
                p50: this.calculateThroughput(stats.median || 0),
                p95: this.calculateThroughput(stats.p95 || 0)
            }
        };
    }
}

// Implementation methods

function removeEmdashStringReplace(text) {
    return text.replace(new RegExp(EM_DASH, 'g'), '');
}

function removeEmdashReplaceAll(text) {
    // String.replaceAll is available in Node.js 15+
    if (typeof text.replaceAll === 'function') {
        return text.replaceAll(EM_DASH, '');
    }
    return text.split(EM_DASH).join('');
}

function removeEmdashRegex(text) {
    return text.replace(/\u2014/g, '');
}

function removeEmdashManual(text) {
    let result = '';
    for (let i = 0; i < text.length; i++) {
        if (text[i] !== EM_DASH) {
            result += text[i];
        }
    }
    return result;
}

function removeEmdashManualOptimized(text) {
    const chars = [];
    for (let i = 0; i < text.length; i++) {
        if (text[i] !== EM_DASH) {
            chars.push(text[i]);
        }
    }
    return chars.join('');
}

function removeEmdashBuffer(buffer) {
    const result = Buffer.allocUnsafe(buffer.length);
    let writePos = 0;
    let i = 0;

    while (i < buffer.length) {
        // Check if we have an em-dash (E2 80 94)
        if (i + 3 <= buffer.length &&
            buffer[i] === 0xE2 &&
            buffer[i + 1] === 0x80 &&
            buffer[i + 2] === 0x94) {
            i += 3; // Skip the em-dash
        } else {
            result[writePos++] = buffer[i++];
        }
    }

    return result.slice(0, writePos);
}

function removeEmdashDashem(buffer) {
    if (!DASHEM_AVAILABLE) {
        throw new Error('dash-em not available');
    }
    // dashem.remove() expects a string and returns a string
    const text = buffer.toString('utf-8');
    const result = dashem.remove(text);
    return Buffer.from(result, 'utf-8');
}

// Test data generation

function countEmdashes(text) {
    let count = 0;
    for (let i = 0; i < text.length; i++) {
        if (text[i] === EM_DASH) count++;
    }
    return count;
}

function generateTestData(pattern) {
    let text;

    switch (pattern) {
        case 'no_emdash':
            // No em-dashes - pure ASCII text
            text = 'a'.repeat(1000000);
            break;

        case 'sparse':
            // 0.1% em-dashes
            const sparseParts = [];
            for (let i = 0; i < 1000; i++) {
                sparseParts.push('a'.repeat(999));
                if (i % 10 === 0) {
                    sparseParts.push(EM_DASH);
                }
            }
            text = sparseParts.join('');
            break;

        case 'moderate':
            // 1% em-dashes
            const modParts = [];
            for (let i = 0; i < 1000; i++) {
                modParts.push('a'.repeat(99));
                modParts.push(EM_DASH);
            }
            text = modParts.join('');
            break;

        case 'dense':
            // 25% em-dashes
            const denseParts = [];
            for (let i = 0; i < 10000; i++) {
                denseParts.push(EM_DASH);
                denseParts.push('abc');
            }
            text = denseParts.join('');
            break;

        case 'alternating':
            // Worst case - alternating
            const altParts = [];
            for (let i = 0; i < 10000; i++) {
                altParts.push(EM_DASH);
                altParts.push('a');
            }
            text = altParts.join('');
            break;

        case 'real_text':
            // Realistic text with natural em-dash usage
            const template = `
                The history of computing—from its humble beginnings to today—is fascinating.
                Charles Babbage—often called the 'father of computing'—designed the first
                mechanical computer. His machine—though never fully built—contained all
                the fundamental principles we use today.

                Modern computers—whether desktop, laptop, or mobile—all share common
                architectures. The von Neumann architecture—named after John von Neumann—
                remains the basis for most computers. This design—which separates memory
                from processing—has proven remarkably durable.
            `;
            text = template.repeat(1000);
            break;

        default:
            throw new Error(`Unknown pattern: ${pattern}`);
    }

    const emdashCount = countEmdashes(text);
    const buffer = Buffer.from(text, 'utf8');

    return { text, buffer, emdashCount };
}

// Benchmarking functions

function benchmarkMethod(name, method, input, expectedOutput = null) {
    const result = new BenchmarkResult(name, name.split('_').pop());

    // Determine input size
    if (Buffer.isBuffer(input)) {
        result.inputSize = input.length;
    } else {
        result.inputSize = Buffer.byteLength(input, 'utf8');
    }

    // Warmup
    try {
        for (let i = 0; i < WARMUP_RUNS; i++) {
            method(input);
        }
    } catch (error) {
        result.correct = false;
        result.error = error.message;
        return result;
    }

    // Benchmark
    const startBatch = performance.now();
    let output;

    while (result.timings.length < MAX_RUNS) {
        const start = performance.now();
        output = method(input);
        const end = performance.now();

        result.addTiming(end - start);

        const totalTime = performance.now() - startBatch;
        if (result.timings.length >= MIN_RUNS && totalTime >= MIN_DURATION_MS) {
            break;
        }
    }

    // Store output size
    if (Buffer.isBuffer(output)) {
        result.outputSize = output.length;
    } else {
        result.outputSize = Buffer.byteLength(output, 'utf8');
    }

    // Verify correctness
    if (expectedOutput !== null) {
        const outputStr = Buffer.isBuffer(output) ? output.toString('utf8') : output;
        const expectedStr = Buffer.isBuffer(expectedOutput) ? expectedOutput.toString('utf8') : expectedOutput;

        if (outputStr !== expectedStr) {
            result.correct = false;
            result.error = `Output mismatch: got ${outputStr.length} chars`;
        }
    }

    return result;
}

function runBenchmarks(pattern, verbose = false) {
    if (verbose) {
        console.error(`Generating test data for pattern: ${pattern}`);
    }

    const { text, buffer, emdashCount } = generateTestData(pattern);

    if (verbose) {
        console.error(`  Input size: ${buffer.length} bytes`);
        console.error(`  Em-dash count: ${emdashCount}`);
    }

    // Expected output for validation
    const expectedText = text.replace(new RegExp(EM_DASH, 'g'), '');
    const expectedBuffer = Buffer.from(expectedText, 'utf8');

    const results = [];

    // Benchmark String.replace with regex
    if (verbose) console.error('  Benchmarking String.replace (regex)...');
    let result = benchmarkMethod(
        `${pattern}_replace_regex`,
        removeEmdashRegex,
        text,
        expectedText
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark String.replace with global flag
    if (verbose) console.error('  Benchmarking String.replace (global)...');
    result = benchmarkMethod(
        `${pattern}_replace_global`,
        removeEmdashStringReplace,
        text,
        expectedText
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark String.replaceAll
    if (verbose) console.error('  Benchmarking String.replaceAll...');
    result = benchmarkMethod(
        `${pattern}_replaceAll`,
        removeEmdashReplaceAll,
        text,
        expectedText
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark manual iteration
    if (verbose) console.error('  Benchmarking manual iteration...');
    result = benchmarkMethod(
        `${pattern}_manual`,
        removeEmdashManual,
        text,
        expectedText
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark manual iteration (optimized)
    if (verbose) console.error('  Benchmarking manual iteration (optimized)...');
    result = benchmarkMethod(
        `${pattern}_manual_opt`,
        removeEmdashManualOptimized,
        text,
        expectedText
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark Buffer manipulation
    if (verbose) console.error('  Benchmarking Buffer manipulation...');
    result = benchmarkMethod(
        `${pattern}_buffer`,
        removeEmdashBuffer,
        buffer,
        expectedBuffer
    );
    result.emdashCount = emdashCount;
    results.push(result);

    // Benchmark dash-em if available
    if (DASHEM_AVAILABLE) {
        if (verbose) console.error('  Benchmarking dash-em...');
        result = benchmarkMethod(
            `${pattern}_dashem`,
            removeEmdashDashem,
            buffer,
            expectedBuffer
        );
        result.emdashCount = emdashCount;
        results.push(result);
    }

    return results;
}

// Output functions

function outputJSON(results, pretty = false) {
    const output = {
        language: 'javascript',
        version: process.version,
        dashem_available: DASHEM_AVAILABLE,
        timestamp: Math.floor(Date.now() / 1000),
        benchmarks: results.map(r => r.toJSON())
    };

    if (pretty) {
        console.log(JSON.stringify(output, null, 2));
    } else {
        console.log(JSON.stringify(output));
    }
}

function outputTable(results) {
    console.log('\nJavaScript/Node.js Em-dash Removal Benchmarks');
    console.log('=============================================');
    console.log(`Node.js version: ${process.version}`);
    console.log(`dash-em available: ${DASHEM_AVAILABLE}\n`);

    console.log('Test'.padEnd(25) + ' ' +
                'Method'.padEnd(15) + ' ' +
                'Size'.padStart(10) + ' ' +
                'Em-dash'.padStart(8) + ' ' +
                'Mean (μs)'.padStart(12) + ' ' +
                'P95 (μs)'.padStart(12) + ' ' +
                'GB/s'.padStart(8) + ' ' +
                'Valid'.padStart(6));
    console.log('-'.repeat(100));

    for (const result of results) {
        const stats = result.calculateStatistics();
        console.log(
            result.name.padEnd(25) + ' ' +
            result.method.padEnd(15) + ' ' +
            result.inputSize.toString().padStart(10) + ' ' +
            result.emdashCount.toString().padStart(8) + ' ' +
            (stats.mean ? stats.mean.toFixed(1) : 'N/A').padStart(12) + ' ' +
            (stats.p95 ? stats.p95.toFixed(1) : 'N/A').padStart(12) + ' ' +
            result.calculateThroughput(stats.mean || 0).toFixed(2).padStart(8) + ' ' +
            (result.correct ? 'PASS' : 'FAIL').padStart(6)
        );
    }

    // Calculate speedups if dash-em is available
    if (DASHEM_AVAILABLE) {
        console.log('\nSpeedup vs native methods (using dash-em):');
        console.log('-'.repeat(50));

        const dashemResults = {};
        const nativeResults = {};

        for (const result of results) {
            if (result.method === 'dashem') {
                const pattern = result.name.replace('_dashem', '');
                dashemResults[pattern] = result;
            } else if (result.method === 'regex') {
                const pattern = result.name.replace('_replace_regex', '');
                nativeResults[pattern] = result;
            }
        }

        for (const pattern of ['no_emdash', 'sparse', 'moderate', 'dense', 'alternating']) {
            if (dashemResults[pattern] && nativeResults[pattern]) {
                const dashemStats = dashemResults[pattern].calculateStatistics();
                const nativeStats = nativeResults[pattern].calculateStatistics();

                const speedup = nativeStats.median / dashemStats.median;
                console.log(`  ${pattern.padEnd(15)}: ${speedup.toFixed(2)}x faster`);
            }
        }
    }
}

// Main function

function main() {
    const args = process.argv.slice(2);
    let outputFormat = 'table';
    let verbose = false;
    let patterns = ['no_emdash', 'sparse', 'moderate', 'dense', 'alternating'];

    // Parse arguments
    for (let i = 0; i < args.length; i++) {
        switch (args[i]) {
            case '--json':
                outputFormat = 'json';
                break;
            case '--json-pretty':
                outputFormat = 'json-pretty';
                break;
            case '--verbose':
            case '-v':
                verbose = true;
                break;
            case '--patterns':
                patterns = args[++i].split(',');
                break;
            case '--help':
            case '-h':
                console.log('Usage: node bench_javascript.js [OPTIONS]');
                console.log('Options:');
                console.log('  --json         Output results as compact JSON');
                console.log('  --json-pretty  Output results as formatted JSON');
                console.log('  --verbose, -v  Show progress information');
                console.log('  --patterns     Comma-separated list of patterns');
                console.log('  --help, -h     Show this help message');
                process.exit(0);
        }
    }

    // Run benchmarks
    const allResults = [];
    for (const pattern of patterns) {
        if (verbose) {
            console.error(`\nRunning pattern: ${pattern}`);
        }
        const results = runBenchmarks(pattern, verbose);
        allResults.push(...results);
    }

    // Output results
    if (outputFormat === 'json') {
        outputJSON(allResults, false);
    } else if (outputFormat === 'json-pretty') {
        outputJSON(allResults, true);
    } else {
        outputTable(allResults);
    }
}

// Run if executed directly
if (require.main === module) {
    main();
}
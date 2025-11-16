use std::collections::HashMap;
use std::env;
use std::time::Instant;
use serde::Serialize;
use regex::Regex;

/// Rust native implementation benchmarks for dash-em comparison.
///
/// This benchmark compares:
/// 1. str::replace()
/// 2. Regex replacement
/// 3. Manual char iteration
/// 4. Manual byte iteration
/// 5. Itertools-based approach
/// 6. dash-em Rust binding (if available)
///
/// Outputs JSON results compatible with the main benchmark suite.

// Em-dash character
const EM_DASH: &str = "\u{2014}";
const EM_DASH_BYTES: &[u8] = "\u{2014}".as_bytes();

// Benchmark configuration
const WARMUP_RUNS: usize = 10;
const MIN_RUNS: usize = 100;
const MAX_RUNS: usize = 1000;
const MIN_DURATION_MS: u64 = 1000; // Run for at least 1 second

#[derive(Debug, Serialize)]
struct BenchmarkResult {
    name: String,
    method: String,
    input_size: usize,
    output_size: usize,
    emdash_count: usize,
    runs: usize,
    correct: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
    timing_us: HashMap<String, f64>,
    throughput_gbps: HashMap<String, f64>,
}

impl BenchmarkResult {
    fn new(name: String, method: String) -> Self {
        BenchmarkResult {
            name,
            method,
            input_size: 0,
            output_size: 0,
            emdash_count: 0,
            runs: 0,
            correct: true,
            error: None,
            timing_us: HashMap::new(),
            throughput_gbps: HashMap::new(),
        }
    }

    fn calculate_statistics(&mut self, timings: &[f64]) {
        if timings.is_empty() {
            return;
        }

        let mut sorted = timings.to_vec();
        sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());

        let n = sorted.len();
        let sum: f64 = sorted.iter().sum();
        let mean = sum / n as f64;

        // Calculate standard deviation
        let variance: f64 = sorted.iter()
            .map(|&x| {
                let diff = x - mean;
                diff * diff
            })
            .sum::<f64>() / (n - 1) as f64;
        let stddev = variance.sqrt();

        self.timing_us.insert("mean".to_string(), mean);
        self.timing_us.insert("median".to_string(), sorted[n / 2]);
        self.timing_us.insert("stddev".to_string(), stddev);
        self.timing_us.insert("min".to_string(), sorted[0]);
        self.timing_us.insert("max".to_string(), sorted[n - 1]);
        self.timing_us.insert("p95".to_string(), sorted[(n as f64 * 0.95) as usize]);
        self.timing_us.insert("p99".to_string(), sorted[(n as f64 * 0.99) as usize]);

        let p999 = if n >= 1000 {
            sorted[(n as f64 * 0.999) as usize]
        } else {
            sorted[n - 1]
        };
        self.timing_us.insert("p999".to_string(), p999);

        // Calculate throughput
        let bytes_per_gb = 1024.0_f64.powi(3);
        let throughput_mean = (self.input_size as f64 / bytes_per_gb) / (mean / 1_000_000.0);
        let throughput_p50 = (self.input_size as f64 / bytes_per_gb) / (sorted[n / 2] / 1_000_000.0);
        let throughput_p95 = (self.input_size as f64 / bytes_per_gb) / (sorted[(n as f64 * 0.95) as usize] / 1_000_000.0);

        self.throughput_gbps.insert("mean".to_string(), throughput_mean);
        self.throughput_gbps.insert("p50".to_string(), throughput_p50);
        self.throughput_gbps.insert("p95".to_string(), throughput_p95);

        self.runs = n;
    }
}

// Implementation methods

fn remove_emdash_str_replace(text: &str) -> String {
    text.replace(EM_DASH, "")
}

fn remove_emdash_regex(text: &str, re: &Regex) -> String {
    re.replace_all(text, "").into_owned()
}

fn remove_emdash_manual_chars(text: &str) -> String {
    text.chars()
        .filter(|&c| c != '\u{2014}')
        .collect()
}

fn remove_emdash_manual_bytes(data: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(data.len());
    let mut i = 0;

    while i < data.len() {
        // Check for em-dash (E2 80 94)
        if i + 3 <= data.len() &&
           data[i] == 0xE2 &&
           data[i + 1] == 0x80 &&
           data[i + 2] == 0x94 {
            i += 3; // Skip the em-dash
        } else {
            result.push(data[i]);
            i += 1;
        }
    }

    result
}

fn remove_emdash_iterator(text: &str) -> String {
    let mut result = String::with_capacity(text.len());
    let mut chars = text.chars();

    while let Some(ch) = chars.next() {
        if ch != '\u{2014}' {
            result.push(ch);
        }
    }

    result
}

// Test data generation

fn count_emdashes(text: &str) -> usize {
    text.matches(EM_DASH).count()
}

fn generate_test_data(pattern: &str) -> (String, Vec<u8>, usize) {
    let text = match pattern {
        "no_emdash" => {
            // No em-dashes - pure ASCII text
            "a".repeat(1_000_000)
        }
        "sparse" => {
            // 0.1% em-dashes
            let mut parts = Vec::new();
            for i in 0..1000 {
                parts.push("a".repeat(999));
                if i % 10 == 0 {
                    parts.push(EM_DASH.to_string());
                }
            }
            parts.join("")
        }
        "moderate" => {
            // 1% em-dashes
            let mut parts = Vec::new();
            for _ in 0..1000 {
                parts.push("a".repeat(99));
                parts.push(EM_DASH.to_string());
            }
            parts.join("")
        }
        "dense" => {
            // 25% em-dashes
            let mut parts = Vec::new();
            for _ in 0..10000 {
                parts.push(EM_DASH.to_string());
                parts.push("abc".to_string());
            }
            parts.join("")
        }
        "alternating" => {
            // Worst case - alternating
            let mut parts = Vec::new();
            for _ in 0..10000 {
                parts.push(EM_DASH.to_string());
                parts.push("a".to_string());
            }
            parts.join("")
        }
        "real_text" => {
            // Realistic text with natural em-dash usage
            let template = r#"
                The history of computing—from its humble beginnings to today—is fascinating.
                Charles Babbage—often called the 'father of computing'—designed the first
                mechanical computer. His machine—though never fully built—contained all
                the fundamental principles we use today.

                Modern computers—whether desktop, laptop, or mobile—all share common
                architectures. The von Neumann architecture—named after John von Neumann—
                remains the basis for most computers. This design—which separates memory
                from processing—has proven remarkably durable.
            "#;
            template.repeat(1000)
        }
        _ => panic!("Unknown pattern: {}", pattern),
    };

    let emdash_count = count_emdashes(&text);
    let data = text.as_bytes().to_vec();

    (text, data, emdash_count)
}

// Benchmarking function
fn benchmark_method<F, T>(
    name: &str,
    method: F,
    input_size: usize,
    expected: Option<T>,
) -> BenchmarkResult
where
    F: Fn() -> T,
    T: AsRef<[u8]> + PartialEq,
{
    let method_name = name.split('_').last().unwrap_or(name);
    let mut result = BenchmarkResult::new(name.to_string(), method_name.to_string());
    result.input_size = input_size;

    // Warmup
    for _ in 0..WARMUP_RUNS {
        method();
    }

    // Benchmark
    let mut timings = Vec::with_capacity(MAX_RUNS);
    let start_batch = Instant::now();

    while timings.len() < MAX_RUNS {
        let start = Instant::now();
        let output = method();
        let elapsed = start.elapsed().as_micros() as f64;

        // Store output size on first run
        if timings.is_empty() {
            result.output_size = output.as_ref().len();
        }

        timings.push(elapsed);

        let total_time = start_batch.elapsed().as_millis();
        if timings.len() >= MIN_RUNS && total_time >= MIN_DURATION_MS as u128 {
            break;
        }
    }

    // Verify correctness
    if let Some(expected_output) = expected {
        let output = method();
        if output != expected_output {
            result.correct = false;
            result.error = Some(format!("Output mismatch: got {} bytes", output.as_ref().len()));
        }
    }

    result.calculate_statistics(&timings);
    result
}

fn run_benchmarks(pattern: &str, verbose: bool) -> Vec<BenchmarkResult> {
    if verbose {
        eprintln!("Generating test data for pattern: {}", pattern);
    }

    let (text, data, emdash_count) = generate_test_data(pattern);

    if verbose {
        eprintln!("  Input size: {} bytes", data.len());
        eprintln!("  Em-dash count: {}", emdash_count);
    }

    // Expected outputs
    let expected_text = text.replace(EM_DASH, "");
    let expected_bytes = expected_text.as_bytes().to_vec();

    let mut results = Vec::new();

    // Benchmark str::replace
    if verbose {
        eprintln!("  Benchmarking str::replace...");
    }
    let text_clone = text.clone();
    let mut result = benchmark_method(
        &format!("{}_str_replace", pattern),
        || remove_emdash_str_replace(&text_clone),
        data.len(),
        Some(expected_text.clone()),
    );
    result.emdash_count = emdash_count;
    results.push(result);

    // Benchmark regex
    if verbose {
        eprintln!("  Benchmarking regex...");
    }
    let re = Regex::new(EM_DASH).unwrap();
    let text_clone = text.clone();
    let mut result = benchmark_method(
        &format!("{}_regex", pattern),
        || remove_emdash_regex(&text_clone, &re),
        data.len(),
        Some(expected_text.clone()),
    );
    result.emdash_count = emdash_count;
    results.push(result);

    // Benchmark manual chars
    if verbose {
        eprintln!("  Benchmarking manual chars...");
    }
    let text_clone = text.clone();
    let mut result = benchmark_method(
        &format!("{}_manual_chars", pattern),
        || remove_emdash_manual_chars(&text_clone),
        data.len(),
        Some(expected_text.clone()),
    );
    result.emdash_count = emdash_count;
    results.push(result);

    // Benchmark manual bytes
    if verbose {
        eprintln!("  Benchmarking manual bytes...");
    }
    let data_clone = data.clone();
    let mut result = benchmark_method(
        &format!("{}_manual_bytes", pattern),
        || remove_emdash_manual_bytes(&data_clone),
        data.len(),
        Some(expected_bytes.clone()),
    );
    result.emdash_count = emdash_count;
    results.push(result);

    // Benchmark iterator
    if verbose {
        eprintln!("  Benchmarking iterator...");
    }
    let text_clone = text.clone();
    let mut result = benchmark_method(
        &format!("{}_iterator", pattern),
        || remove_emdash_iterator(&text_clone),
        data.len(),
        Some(expected_text.clone()),
    );
    result.emdash_count = emdash_count;
    results.push(result);

    // TODO: Add dash-em binding when available

    results
}

#[derive(Serialize)]
struct JsonOutput {
    language: String,
    version: String,
    dashem_available: bool,
    timestamp: u64,
    benchmarks: Vec<BenchmarkResult>,
}

fn output_json(results: Vec<BenchmarkResult>, pretty: bool) {
    // Get Rust version at runtime
    let rustc_version = std::process::Command::new("rustc")
        .arg("--version")
        .output()
        .ok()
        .and_then(|out| String::from_utf8(out.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "unknown".to_string());

    let output = JsonOutput {
        language: "rust".to_string(),
        version: rustc_version,
        dashem_available: false, // TODO: Update when binding available
        timestamp: std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs(),
        benchmarks: results,
    };

    if pretty {
        println!("{}", serde_json::to_string_pretty(&output).unwrap());
    } else {
        println!("{}", serde_json::to_string(&output).unwrap());
    }
}

fn output_table(results: &[BenchmarkResult]) {
    // Get Rust version at runtime
    let rustc_version = std::process::Command::new("rustc")
        .arg("--version")
        .output()
        .ok()
        .and_then(|out| String::from_utf8(out.stdout).ok())
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|| "unknown".to_string());

    println!("\nRust Em-dash Removal Benchmarks");
    println!("================================");
    println!("Rust version: {}", rustc_version);
    println!("dash-em available: false\n"); // TODO: Update when binding available

    println!("{:<25} {:<15} {:>10} {:>8} {:>12} {:>12} {:>8} {:>6}",
             "Test", "Method", "Size", "Em-dash", "Mean (μs)", "P95 (μs)", "GB/s", "Valid");
    println!("{}", "-".repeat(100));

    for result in results {
        let valid = if result.correct { "PASS" } else { "FAIL" };

        let mean = result.timing_us.get("mean").unwrap_or(&0.0);
        let p95 = result.timing_us.get("p95").unwrap_or(&0.0);
        let gbps = result.throughput_gbps.get("mean").unwrap_or(&0.0);

        println!("{:<25} {:<15} {:>10} {:>8} {:>12.1} {:>12.1} {:>8.2} {:>6}",
                 result.name, result.method, result.input_size, result.emdash_count,
                 mean, p95, gbps, valid);
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();

    let mut output_format = "table";
    let mut verbose = false;
    let mut patterns = vec!["no_emdash", "sparse", "moderate", "dense", "alternating"];

    // Parse arguments
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--json" => output_format = "json",
            "--json-pretty" => output_format = "json-pretty",
            "--verbose" | "-v" => verbose = true,
            "--patterns" => {
                i += 1;
                if i < args.len() {
                    patterns = args[i].split(',').map(|s| s.trim()).collect();
                }
            }
            "--help" | "-h" => {
                println!("Usage: {} [OPTIONS]", args[0]);
                println!("Options:");
                println!("  --json         Output results as compact JSON");
                println!("  --json-pretty  Output results as formatted JSON");
                println!("  --verbose, -v  Show progress information");
                println!("  --patterns     Comma-separated list of patterns");
                println!("  --help, -h     Show this help message");
                return;
            }
            _ => {}
        }
        i += 1;
    }

    // Run benchmarks
    let mut all_results = Vec::new();
    for pattern in patterns {
        if verbose {
            eprintln!("\nRunning pattern: {}", pattern);
        }
        let mut results = run_benchmarks(pattern, verbose);
        all_results.append(&mut results);
    }

    // Output results
    match output_format {
        "json" => output_json(all_results, false),
        "json-pretty" => output_json(all_results, true),
        _ => output_table(&all_results),
    }
}
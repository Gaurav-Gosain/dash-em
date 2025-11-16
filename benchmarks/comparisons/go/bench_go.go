package main

/*
Go native implementation benchmarks for dash-em comparison.

This benchmark compares:
1. strings.ReplaceAll()
2. bytes.ReplaceAll()
3. regexp.ReplaceAll()
4. Manual byte iteration
5. Manual rune iteration
6. dash-em Go binding (if available)

Outputs JSON results compatible with the main benchmark suite.
*/

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"math"
	"os"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"time"
)

// Try to import dash-em Go binding
// Note: This would be in a separate file that conditionally compiles
// For now, we'll stub it out

const (
	// Em-dash character
	EmDash = "\u2014"

	// Benchmark configuration
	WarmupRuns     = 10
	MinRuns        = 100
	MaxRuns        = 1000
	MinDurationMs  = 1000 // Run for at least 1 second
)

var EmDashBytes = []byte(EmDash)

// BenchmarkResult stores results for a single benchmark
type BenchmarkResult struct {
	Name        string             `json:"name"`
	Method      string             `json:"method"`
	InputSize   int                `json:"input_size"`
	OutputSize  int                `json:"output_size"`
	EmdashCount int                `json:"emdash_count"`
	Runs        int                `json:"runs"`
	Correct     bool               `json:"correct"`
	Error       string             `json:"error,omitempty"`
	TimingUs    map[string]float64 `json:"timing_us"`
	Throughput  map[string]float64 `json:"throughput_gbps"`
}

// calculateStatistics computes statistical metrics from timings
func calculateStatistics(timings []float64) map[string]float64 {
	if len(timings) == 0 {
		return map[string]float64{}
	}

	// Sort timings
	sorted := make([]float64, len(timings))
	copy(sorted, timings)
	sort.Float64s(sorted)

	n := len(sorted)

	// Calculate mean
	sum := 0.0
	for _, t := range sorted {
		sum += t
	}
	mean := sum / float64(n)

	// Calculate standard deviation
	sumSq := 0.0
	for _, t := range sorted {
		diff := t - mean
		sumSq += diff * diff
	}
	stddev := 0.0
	if n > 1 {
		stddev = math.Sqrt(sumSq / float64(n-1))
	}

	// Calculate percentiles
	p95 := sorted[int(float64(n)*0.95)]
	p99 := sorted[int(float64(n)*0.99)]
	p999 := sorted[n-1]
	if n >= 1000 {
		p999 = sorted[int(float64(n)*0.999)]
	}

	return map[string]float64{
		"mean":   mean,
		"median": sorted[n/2],
		"stddev": stddev,
		"min":    sorted[0],
		"max":    sorted[n-1],
		"p95":    p95,
		"p99":    p99,
		"p999":   p999,
	}
}

// calculateThroughput computes throughput in GB/s
func calculateThroughput(sizeBytes int, timingUs float64) float64 {
	if timingUs <= 0 {
		return 0
	}
	bytesPerGb := math.Pow(1024, 3)
	return (float64(sizeBytes) / bytesPerGb) / (timingUs / 1000000)
}

// Implementation methods

func removeEmdashStringsReplace(text string) string {
	return strings.ReplaceAll(text, EmDash, "")
}

func removeEmdashBytesReplace(data []byte) []byte {
	return bytes.ReplaceAll(data, EmDashBytes, []byte{})
}

func removeEmdashRegex(text string, re *regexp.Regexp) string {
	return re.ReplaceAllString(text, "")
}

func removeEmdashManualBytes(data []byte) []byte {
	result := make([]byte, 0, len(data))
	i := 0

	for i < len(data) {
		// Check for em-dash (E2 80 94)
		if i+3 <= len(data) &&
			data[i] == 0xE2 &&
			data[i+1] == 0x80 &&
			data[i+2] == 0x94 {
			i += 3 // Skip the em-dash
		} else {
			result = append(result, data[i])
			i++
		}
	}

	return result
}

func removeEmdashManualRunes(text string) string {
	var result strings.Builder
	result.Grow(len(text))

	for _, r := range text {
		if r != '—' { // Em-dash rune
			result.WriteRune(r)
		}
	}

	return result.String()
}

// countEmdashes counts em-dashes in text
func countEmdashes(text string) int {
	return strings.Count(text, EmDash)
}

// generateTestData generates test data for a pattern
func generateTestData(pattern string) (string, []byte, int) {
	var text string

	switch pattern {
	case "no_emdash":
		// No em-dashes - pure ASCII text
		text = strings.Repeat("a", 1000000)

	case "sparse":
		// 0.1% em-dashes
		var builder strings.Builder
		for i := 0; i < 1000; i++ {
			builder.WriteString(strings.Repeat("a", 999))
			if i%10 == 0 {
				builder.WriteString(EmDash)
			}
		}
		text = builder.String()

	case "moderate":
		// 1% em-dashes
		var builder strings.Builder
		for i := 0; i < 1000; i++ {
			builder.WriteString(strings.Repeat("a", 99))
			builder.WriteString(EmDash)
		}
		text = builder.String()

	case "dense":
		// 25% em-dashes
		var builder strings.Builder
		for i := 0; i < 10000; i++ {
			builder.WriteString(EmDash)
			builder.WriteString("abc")
		}
		text = builder.String()

	case "alternating":
		// Worst case - alternating
		var builder strings.Builder
		for i := 0; i < 10000; i++ {
			builder.WriteString(EmDash)
			builder.WriteString("a")
		}
		text = builder.String()

	case "real_text":
		// Realistic text with natural em-dash usage
		template := `
		The history of computing—from its humble beginnings to today—is fascinating.
		Charles Babbage—often called the 'father of computing'—designed the first
		mechanical computer. His machine—though never fully built—contained all
		the fundamental principles we use today.

		Modern computers—whether desktop, laptop, or mobile—all share common
		architectures. The von Neumann architecture—named after John von Neumann—
		remains the basis for most computers. This design—which separates memory
		from processing—has proven remarkably durable.
		`
		text = strings.Repeat(template, 1000)

	default:
		panic(fmt.Sprintf("Unknown pattern: %s", pattern))
	}

	emdashCount := countEmdashes(text)
	data := []byte(text)

	return text, data, emdashCount
}

// benchmarkMethod benchmarks a single method
func benchmarkMethod(name string, method func() (interface{}, error),
	inputSize int, expectedOutput interface{}) *BenchmarkResult {

	result := &BenchmarkResult{
		Name:      name,
		Method:    strings.Split(name, "_")[len(strings.Split(name, "_"))-1],
		InputSize: inputSize,
		Correct:   true,
	}

	// Warmup
	for i := 0; i < WarmupRuns; i++ {
		_, err := method()
		if err != nil {
			result.Correct = false
			result.Error = err.Error()
			return result
		}
	}

	// Benchmark
	timings := make([]float64, 0, MaxRuns)
	startBatch := time.Now()

	for len(timings) < MaxRuns {
		start := time.Now()
		output, err := method()
		elapsed := time.Since(start).Microseconds()

		if err != nil {
			result.Correct = false
			result.Error = err.Error()
			return result
		}

		timings = append(timings, float64(elapsed))

		// Store output size on first run
		if len(timings) == 1 {
			switch v := output.(type) {
			case string:
				result.OutputSize = len([]byte(v))
			case []byte:
				result.OutputSize = len(v)
			}
		}

		totalTime := time.Since(startBatch).Milliseconds()
		if len(timings) >= MinRuns && totalTime >= MinDurationMs {
			break
		}
	}

	result.Runs = len(timings)

	// Verify correctness
	if expectedOutput != nil && len(timings) > 0 {
		// Get last output for verification
		output, _ := method()
		switch expected := expectedOutput.(type) {
		case string:
			if str, ok := output.(string); ok {
				if str != expected {
					result.Correct = false
					result.Error = fmt.Sprintf("Output mismatch: got %d chars", len(str))
				}
			}
		case []byte:
			if data, ok := output.([]byte); ok {
				if !bytes.Equal(data, expected) {
					result.Correct = false
					result.Error = fmt.Sprintf("Output mismatch: got %d bytes", len(data))
				}
			}
		}
	}

	// Calculate statistics
	stats := calculateStatistics(timings)
	result.TimingUs = stats

	// Calculate throughput
	result.Throughput = map[string]float64{
		"mean": calculateThroughput(inputSize, stats["mean"]),
		"p50":  calculateThroughput(inputSize, stats["median"]),
		"p95":  calculateThroughput(inputSize, stats["p95"]),
	}

	return result
}

// runBenchmarks runs all benchmarks for a pattern
func runBenchmarks(pattern string, verbose bool) []*BenchmarkResult {
	if verbose {
		log.Printf("Generating test data for pattern: %s", pattern)
	}

	text, data, emdashCount := generateTestData(pattern)

	if verbose {
		log.Printf("  Input size: %d bytes", len(data))
		log.Printf("  Em-dash count: %d", emdashCount)
	}

	// Expected outputs
	expectedText := strings.ReplaceAll(text, EmDash, "")
	expectedBytes := []byte(expectedText)

	results := make([]*BenchmarkResult, 0, 6)

	// Benchmark strings.ReplaceAll
	if verbose {
		log.Printf("  Benchmarking strings.ReplaceAll...")
	}
	result := benchmarkMethod(
		fmt.Sprintf("%s_strings", pattern),
		func() (interface{}, error) {
			return removeEmdashStringsReplace(text), nil
		},
		len(data),
		expectedText,
	)
	result.EmdashCount = emdashCount
	results = append(results, result)

	// Benchmark bytes.ReplaceAll
	if verbose {
		log.Printf("  Benchmarking bytes.ReplaceAll...")
	}
	result = benchmarkMethod(
		fmt.Sprintf("%s_bytes", pattern),
		func() (interface{}, error) {
			return removeEmdashBytesReplace(data), nil
		},
		len(data),
		expectedBytes,
	)
	result.EmdashCount = emdashCount
	results = append(results, result)

	// Benchmark regex
	if verbose {
		log.Printf("  Benchmarking regexp...")
	}
	re := regexp.MustCompile(EmDash)
	result = benchmarkMethod(
		fmt.Sprintf("%s_regex", pattern),
		func() (interface{}, error) {
			return removeEmdashRegex(text, re), nil
		},
		len(data),
		expectedText,
	)
	result.EmdashCount = emdashCount
	results = append(results, result)

	// Benchmark manual bytes
	if verbose {
		log.Printf("  Benchmarking manual bytes...")
	}
	result = benchmarkMethod(
		fmt.Sprintf("%s_manual_bytes", pattern),
		func() (interface{}, error) {
			return removeEmdashManualBytes(data), nil
		},
		len(data),
		expectedBytes,
	)
	result.EmdashCount = emdashCount
	results = append(results, result)

	// Benchmark manual runes
	if verbose {
		log.Printf("  Benchmarking manual runes...")
	}
	result = benchmarkMethod(
		fmt.Sprintf("%s_manual_runes", pattern),
		func() (interface{}, error) {
			return removeEmdashManualRunes(text), nil
		},
		len(data),
		expectedText,
	)
	result.EmdashCount = emdashCount
	results = append(results, result)

	// TODO: Add dash-em binding when available

	return results
}

// outputJSON outputs results as JSON
func outputJSON(results []*BenchmarkResult, pretty bool) {
	output := map[string]interface{}{
		"language":         "go",
		"version":          runtime.Version(),
		"dashem_available": false, // TODO: Update when binding available
		"timestamp":        time.Now().Unix(),
		"benchmarks":       results,
	}

	encoder := json.NewEncoder(os.Stdout)
	if pretty {
		encoder.SetIndent("", "  ")
	}
	encoder.Encode(output)
}

// outputTable outputs results as a formatted table
func outputTable(results []*BenchmarkResult) {
	fmt.Println("\nGo Em-dash Removal Benchmarks")
	fmt.Println("==============================")
	fmt.Printf("Go version: %s\n", runtime.Version())
	fmt.Println("dash-em available: false\n") // TODO: Update when binding available

	fmt.Printf("%-25s %-15s %10s %8s %12s %12s %8s %6s\n",
		"Test", "Method", "Size", "Em-dash", "Mean (μs)", "P95 (μs)", "GB/s", "Valid")
	fmt.Println(strings.Repeat("-", 100))

	for _, result := range results {
		valid := "PASS"
		if !result.Correct {
			valid = "FAIL"
		}

		meanStr := "N/A"
		p95Str := "N/A"
		gbpsStr := "N/A"

		if mean, ok := result.TimingUs["mean"]; ok {
			meanStr = fmt.Sprintf("%.1f", mean)
		}
		if p95, ok := result.TimingUs["p95"]; ok {
			p95Str = fmt.Sprintf("%.1f", p95)
		}
		if gbps, ok := result.Throughput["mean"]; ok {
			gbpsStr = fmt.Sprintf("%.2f", gbps)
		}

		fmt.Printf("%-25s %-15s %10d %8d %12s %12s %8s %6s\n",
			result.Name, result.Method, result.InputSize, result.EmdashCount,
			meanStr, p95Str, gbpsStr, valid)
	}
}

func main() {
	var (
		jsonOutput   = flag.Bool("json", false, "Output results as compact JSON")
		jsonPretty   = flag.Bool("json-pretty", false, "Output results as formatted JSON")
		verbose      = flag.Bool("v", false, "Show progress information")
		patternsFlag = flag.String("patterns", "no_emdash,sparse,moderate,dense,alternating",
			"Comma-separated list of patterns")
	)

	flag.Parse()

	patterns := strings.Split(*patternsFlag, ",")

	// Run benchmarks
	allResults := make([]*BenchmarkResult, 0)
	for _, pattern := range patterns {
		if *verbose {
			log.Printf("\nRunning pattern: %s", pattern)
		}
		results := runBenchmarks(pattern, *verbose)
		allResults = append(allResults, results...)
	}

	// Output results
	if *jsonOutput || *jsonPretty {
		outputJSON(allResults, *jsonPretty)
	} else {
		outputTable(allResults)
	}
}
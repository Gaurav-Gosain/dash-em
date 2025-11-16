/**
 * @file bench_statistical.c
 * @brief Statistical benchmarking suite for dash-em with percentile analysis
 *
 * This benchmark suite provides rigorous statistical analysis including:
 * - Multiple runs with warmup phase
 * - Percentile calculations (p50, p95, p99, p99.9)
 * - Standard deviation and confidence intervals
 * - JSON output for CI integration
 * - Outlier detection using MAD (Median Absolute Deviation)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "../src/dashem.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* Configuration constants */
#define WARMUP_RUNS 10
#define MIN_RUNS 100
#define MAX_RUNS 1000
#define MIN_DURATION_MS 1000  /* Run for at least 1 second */
#define OUTLIER_MAD_THRESHOLD 3.0  /* MAD multiplier for outlier detection */

/* JSON output modes */
typedef enum {
    JSON_NONE = 0,
    JSON_COMPACT = 1,
    JSON_PRETTY = 2
} json_mode_t;

/* Benchmark result structure */
typedef struct {
    const char* name;
    size_t input_size;
    size_t output_size;
    size_t emdash_count;

    /* Timing data */
    double* timings;  /* Array of all timing measurements */
    size_t num_runs;

    /* Statistical metrics */
    double mean;
    double median;
    double stddev;
    double p95;
    double p99;
    double p999;
    double min;
    double max;

    /* Throughput metrics (GB/s) */
    double throughput_mean;
    double throughput_p50;
    double throughput_p95;

    /* Comparison metrics */
    double speedup_vs_naive;

    /* Validation */
    bool correct;
    char error_msg[256];
} benchmark_result_t;

/* Get high-resolution timestamp in microseconds */
static double get_time_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int freq_initialized = 0;
    LARGE_INTEGER counter;

    if (!freq_initialized) {
        QueryPerformanceFrequency(&frequency);
        freq_initialized = 1;
    }

    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart * 1000000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
#endif
}

/* Comparison function for qsort */
static int compare_double(const void* a, const void* b) {
    double diff = *(const double*)a - *(const double*)b;
    return (diff > 0) - (diff < 0);
}

/* Calculate percentile from sorted array */
static double calculate_percentile(double* sorted_data, size_t n, double percentile) {
    if (n == 0) return 0.0;
    if (n == 1) return sorted_data[0];

    double index = (percentile / 100.0) * (n - 1);
    size_t lower = (size_t)floor(index);
    size_t upper = (size_t)ceil(index);

    if (lower == upper) {
        return sorted_data[lower];
    }

    double weight = index - lower;
    return sorted_data[lower] * (1 - weight) + sorted_data[upper] * weight;
}

/* Calculate standard deviation */
static double calculate_stddev(double* data, size_t n, double mean) {
    if (n <= 1) return 0.0;

    double sum_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }

    return sqrt(sum_sq / (n - 1));
}

/* Calculate Median Absolute Deviation for outlier detection */
static double calculate_mad(double* data, size_t n, double median) {
    if (n <= 1) return 0.0;

    double* deviations = malloc(n * sizeof(double));
    for (size_t i = 0; i < n; i++) {
        deviations[i] = fabs(data[i] - median);
    }

    qsort(deviations, n, sizeof(double), compare_double);
    double mad = calculate_percentile(deviations, n, 50);
    free(deviations);

    return mad;
}

/* Remove outliers using MAD */
static size_t remove_outliers(double* data, size_t n) {
    if (n <= 3) return n;  /* Need at least 3 points */

    /* Calculate median */
    double* sorted = malloc(n * sizeof(double));
    memcpy(sorted, data, n * sizeof(double));
    qsort(sorted, n, sizeof(double), compare_double);
    double median = calculate_percentile(sorted, n, 50);

    /* Calculate MAD */
    double mad = calculate_mad(sorted, n, median);
    free(sorted);

    if (mad < 1e-9) return n;  /* No variation, keep all */

    /* Filter outliers */
    size_t kept = 0;
    double threshold = OUTLIER_MAD_THRESHOLD * mad;

    for (size_t i = 0; i < n; i++) {
        if (fabs(data[i] - median) <= threshold) {
            data[kept++] = data[i];
        }
    }

    return kept;
}

/* Naive implementation for comparison */
static size_t remove_emdash_naive(const char* input, size_t input_len,
                                  char* output, size_t output_capacity) {
    size_t out_idx = 0;
    size_t i = 0;

    while (i < input_len && out_idx < output_capacity) {
        if (i + 3 <= input_len &&
            (unsigned char)input[i] == 0xE2 &&
            (unsigned char)input[i+1] == 0x80 &&
            (unsigned char)input[i+2] == 0x94) {
            i += 3;
        } else {
            output[out_idx++] = input[i++];
        }
    }

    return out_idx;
}

/* Run a single benchmark test multiple times */
static void run_benchmark(benchmark_result_t* result,
                         const char* input, size_t input_len,
                         char* output_buffer, size_t output_capacity,
                         bool use_naive) {

    /* Allocate timing array */
    result->timings = malloc(MAX_RUNS * sizeof(double));
    result->num_runs = 0;

    /* Warmup phase */
    for (int i = 0; i < WARMUP_RUNS; i++) {
        size_t output_len;
        if (use_naive) {
            output_len = remove_emdash_naive(input, input_len, output_buffer, output_capacity);
        } else {
            dashem_remove(input, input_len, output_buffer, output_capacity, &output_len);
        }
    }

    /* Benchmark phase - run until we have enough samples or time */
    double total_time = 0.0;
    double start_batch = get_time_us();

    while (result->num_runs < MAX_RUNS) {
        double start = get_time_us();

        size_t output_len;
        if (use_naive) {
            output_len = remove_emdash_naive(input, input_len, output_buffer, output_capacity);
        } else {
            int ret = dashem_remove(input, input_len, output_buffer, output_capacity, &output_len);
            if (ret != 0) {
                result->correct = false;
                snprintf(result->error_msg, sizeof(result->error_msg),
                        "dashem_remove failed with code %d", ret);
                break;
            }
        }

        double end = get_time_us();
        result->timings[result->num_runs++] = end - start;
        result->output_size = output_len;

        total_time = (end - start_batch) / 1000.0;  /* Convert to ms */

        /* Check if we have enough samples */
        if (result->num_runs >= MIN_RUNS && total_time >= MIN_DURATION_MS) {
            break;
        }
    }

    /* Remove outliers */
    size_t filtered_count = remove_outliers(result->timings, result->num_runs);
    result->num_runs = filtered_count;

    /* Sort for percentile calculations */
    qsort(result->timings, result->num_runs, sizeof(double), compare_double);

    /* Calculate statistics */
    double sum = 0.0;
    for (size_t i = 0; i < result->num_runs; i++) {
        sum += result->timings[i];
    }

    result->mean = sum / result->num_runs;
    result->median = calculate_percentile(result->timings, result->num_runs, 50);
    result->p95 = calculate_percentile(result->timings, result->num_runs, 95);
    result->p99 = calculate_percentile(result->timings, result->num_runs, 99);
    result->p999 = calculate_percentile(result->timings, result->num_runs, 99.9);
    result->min = result->timings[0];
    result->max = result->timings[result->num_runs - 1];
    result->stddev = calculate_stddev(result->timings, result->num_runs, result->mean);

    /* Calculate throughput (GB/s) */
    double bytes_per_gb = 1024.0 * 1024.0 * 1024.0;
    result->throughput_mean = (input_len / bytes_per_gb) / (result->mean / 1000000.0);
    result->throughput_p50 = (input_len / bytes_per_gb) / (result->median / 1000000.0);
    result->throughput_p95 = (input_len / bytes_per_gb) / (result->p95 / 1000000.0);
}

/* Generate test data */
static char* generate_test_data(const char* pattern_name, size_t* input_len, size_t* emdash_count) {
    char* data = NULL;
    *emdash_count = 0;

    if (strcmp(pattern_name, "no_emdash") == 0) {
        /* No em-dashes - pure ASCII text */
        *input_len = 1000000;  /* 1MB */
        data = malloc(*input_len);
        for (size_t i = 0; i < *input_len; i++) {
            data[i] = 'a' + (i % 26);
        }
        *emdash_count = 0;

    } else if (strcmp(pattern_name, "sparse") == 0) {
        /* 0.1% em-dashes - typical document */
        *input_len = 1000000;
        data = malloc(*input_len);
        size_t pos = 0;

        while (pos < *input_len) {
            if (pos % 1000 == 500 && pos + 3 <= *input_len) {
                data[pos++] = (char)0xE2;
                data[pos++] = (char)0x80;
                data[pos++] = (char)0x94;
                (*emdash_count)++;
            } else {
                data[pos++] = 'a' + (pos % 26);
            }
        }

    } else if (strcmp(pattern_name, "moderate") == 0) {
        /* 1% em-dashes */
        *input_len = 100000;
        data = malloc(*input_len);
        size_t pos = 0;

        while (pos < *input_len) {
            if (pos % 100 == 50 && pos + 3 <= *input_len) {
                data[pos++] = (char)0xE2;
                data[pos++] = (char)0x80;
                data[pos++] = (char)0x94;
                (*emdash_count)++;
            } else {
                data[pos++] = 'a' + (pos % 26);
            }
        }

    } else if (strcmp(pattern_name, "dense") == 0) {
        /* 25% em-dashes - stress test */
        *input_len = 40000;
        data = malloc(*input_len);
        size_t pos = 0;

        while (pos < *input_len) {
            if (pos % 4 == 0 && pos + 3 <= *input_len) {
                data[pos++] = (char)0xE2;
                data[pos++] = (char)0x80;
                data[pos++] = (char)0x94;
                (*emdash_count)++;
            } else {
                data[pos++] = 'x';
            }
        }

    } else if (strcmp(pattern_name, "alternating") == 0) {
        /* Alternating pattern - worst case */
        *input_len = 20000;
        data = malloc(*input_len);
        size_t pos = 0;

        while (pos + 4 <= *input_len) {
            data[pos++] = (char)0xE2;
            data[pos++] = (char)0x80;
            data[pos++] = (char)0x94;
            data[pos++] = 'a';
            (*emdash_count)++;
        }
        while (pos < *input_len) {
            data[pos++] = 'a';
        }

    } else if (strcmp(pattern_name, "boundary") == 0) {
        /* Em-dashes at SIMD boundaries */
        *input_len = 65536;  /* 64KB */
        data = malloc(*input_len);
        memset(data, 'a', *input_len);

        /* Place em-dashes at various boundaries */
        size_t boundaries[] = {0, 13, 29, 30, 31, 32, 61, 62, 63, 64,
                              509, 510, 511, 512, 1021, 1022, 1023, 1024};

        for (size_t i = 0; i < sizeof(boundaries)/sizeof(boundaries[0]); i++) {
            for (size_t offset = 0; offset < *input_len; offset += 512) {
                size_t pos = offset + boundaries[i];
                if (pos + 3 <= *input_len) {
                    data[pos] = (char)0xE2;
                    data[pos + 1] = (char)0x80;
                    data[pos + 2] = (char)0x94;
                    (*emdash_count)++;
                }
            }
        }
    }

    return data;
}

/* Output results as JSON */
static void output_json(benchmark_result_t* results, size_t num_results, json_mode_t mode) {
    const char* indent = (mode == JSON_PRETTY) ? "  " : "";
    const char* newline = (mode == JSON_PRETTY) ? "\n" : "";

    printf("{%s", newline);
    printf("%s\"implementation\": \"%s\",%s", indent, dashem_implementation_name(), newline);
    printf("%s\"cpu_features\": %u,%s", indent, dashem_detect_cpu_features(), newline);
    printf("%s\"version\": \"%s\",%s", indent, dashem_version(), newline);

    /* System info */
    printf("%s\"system\": {%s", indent, newline);
#ifdef _WIN32
    printf("%s%s\"platform\": \"windows\",%s", indent, indent, newline);
#elif __APPLE__
    printf("%s%s\"platform\": \"macos\",%s", indent, indent, newline);
#else
    printf("%s%s\"platform\": \"linux\",%s", indent, indent, newline);
#endif

#ifdef __x86_64__
    printf("%s%s\"arch\": \"x86_64\",%s", indent, indent, newline);
#elif __aarch64__
    printf("%s%s\"arch\": \"aarch64\",%s", indent, indent, newline);
#elif __arm__
    printf("%s%s\"arch\": \"arm\",%s", indent, indent, newline);
#else
    printf("%s%s\"arch\": \"unknown\",%s", indent, indent, newline);
#endif

    printf("%s%s\"timestamp\": %ld%s", indent, indent, (long)time(NULL), newline);
    printf("%s},%s", indent, newline);

    /* Benchmark results */
    printf("%s\"benchmarks\": [%s", indent, newline);

    for (size_t i = 0; i < num_results; i++) {
        benchmark_result_t* r = &results[i];

        printf("%s%s{%s", indent, indent, newline);
        printf("%s%s%s\"name\": \"%s\",%s", indent, indent, indent, r->name, newline);
        printf("%s%s%s\"input_size\": %zu,%s", indent, indent, indent, r->input_size, newline);
        printf("%s%s%s\"output_size\": %zu,%s", indent, indent, indent, r->output_size, newline);
        printf("%s%s%s\"emdash_count\": %zu,%s", indent, indent, indent, r->emdash_count, newline);
        printf("%s%s%s\"runs\": %zu,%s", indent, indent, indent, r->num_runs, newline);
        printf("%s%s%s\"correct\": %s,%s", indent, indent, indent,
               r->correct ? "true" : "false", newline);

        if (!r->correct) {
            printf("%s%s%s\"error\": \"%s\",%s", indent, indent, indent, r->error_msg, newline);
        }

        /* Timing statistics (microseconds) */
        printf("%s%s%s\"timing_us\": {%s", indent, indent, indent, newline);
        printf("%s%s%s%s\"mean\": %.2f,%s", indent, indent, indent, indent, r->mean, newline);
        printf("%s%s%s%s\"median\": %.2f,%s", indent, indent, indent, indent, r->median, newline);
        printf("%s%s%s%s\"stddev\": %.2f,%s", indent, indent, indent, indent, r->stddev, newline);
        printf("%s%s%s%s\"min\": %.2f,%s", indent, indent, indent, indent, r->min, newline);
        printf("%s%s%s%s\"max\": %.2f,%s", indent, indent, indent, indent, r->max, newline);
        printf("%s%s%s%s\"p95\": %.2f,%s", indent, indent, indent, indent, r->p95, newline);
        printf("%s%s%s%s\"p99\": %.2f,%s", indent, indent, indent, indent, r->p99, newline);
        printf("%s%s%s%s\"p999\": %.2f%s", indent, indent, indent, indent, r->p999, newline);
        printf("%s%s%s},%s", indent, indent, indent, newline);

        /* Throughput (GB/s) */
        printf("%s%s%s\"throughput_gbps\": {%s", indent, indent, indent, newline);
        printf("%s%s%s%s\"mean\": %.2f,%s", indent, indent, indent, indent,
               r->throughput_mean, newline);
        printf("%s%s%s%s\"p50\": %.2f,%s", indent, indent, indent, indent,
               r->throughput_p50, newline);
        printf("%s%s%s%s\"p95\": %.2f%s", indent, indent, indent, indent,
               r->throughput_p95, newline);
        printf("%s%s%s},%s", indent, indent, indent, newline);

        printf("%s%s%s\"speedup_vs_naive\": %.2f%s", indent, indent, indent,
               r->speedup_vs_naive, newline);

        printf("%s%s}", indent, indent);
        if (i < num_results - 1) printf(",");
        printf("%s", newline);
    }

    printf("%s]%s", indent, newline);
    printf("}%s", newline);
}

/* Output results as human-readable table */
static void output_table(benchmark_result_t* results, size_t num_results) {
    printf("\n");
    printf("dash-em Statistical Benchmark Results\n");
    printf("=====================================\n");
    printf("Implementation: %s\n", dashem_implementation_name());
    printf("Version: %s\n", dashem_version());
    printf("CPU Features: 0x%08X\n\n", dashem_detect_cpu_features());

    printf("%-15s %8s %8s %10s | %8s %8s %8s %8s | %8s %8s | %7s\n",
           "Test", "Size", "EmDash", "Runs",
           "Mean", "P50", "P95", "P99",
           "GB/s", "P95 GB/s",
           "Speedup");
    printf("%-15s %8s %8s %10s | %8s %8s %8s %8s | %8s %8s | %7s\n",
           "", "(bytes)", "Count", "",
           "(μs)", "(μs)", "(μs)", "(μs)",
           "", "",
           "vs Naive");
    printf("----------------------------------------");
    printf("----------------------------------------");
    printf("------------------------\n");

    for (size_t i = 0; i < num_results; i++) {
        benchmark_result_t* r = &results[i];

        if (!r->correct) {
            printf("%-15s %8zu %8zu %10zu | %8s %8s %8s %8s | %8s %8s | %7s\n",
                   r->name, r->input_size, r->emdash_count, r->num_runs,
                   "ERROR", "ERROR", "ERROR", "ERROR",
                   "ERROR", "ERROR", "ERROR");
            printf("  Error: %s\n", r->error_msg);
        } else {
            printf("%-15s %8zu %8zu %10zu | %8.1f %8.1f %8.1f %8.1f | %8.2f %8.2f | %7.2fx\n",
                   r->name, r->input_size, r->emdash_count, r->num_runs,
                   r->mean, r->median, r->p95, r->p99,
                   r->throughput_mean, r->throughput_p95,
                   r->speedup_vs_naive);
        }
    }

    printf("\n");
    printf("Statistics:\n");
    printf("-----------\n");
    for (size_t i = 0; i < num_results; i++) {
        benchmark_result_t* r = &results[i];
        if (r->correct) {
            printf("%-15s: StdDev=%.1fμs (%.1f%%), Min=%.1fμs, Max=%.1fμs, P99.9=%.1fμs\n",
                   r->name, r->stddev, (r->stddev / r->mean) * 100,
                   r->min, r->max, r->p999);
        }
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    /* Parse command line arguments */
    json_mode_t json_mode = JSON_NONE;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = JSON_COMPACT;
        } else if (strcmp(argv[i], "--json-pretty") == 0) {
            json_mode = JSON_PRETTY;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --json         Output results as compact JSON\n");
            printf("  --json-pretty  Output results as formatted JSON\n");
            printf("  --verbose, -v  Show detailed progress\n");
            printf("  --help, -h     Show this help message\n");
            return 0;
        }
    }

    /* Test patterns */
    const char* patterns[] = {
        "no_emdash",    /* No em-dashes */
        "sparse",       /* 0.1% density */
        "moderate",     /* 1% density */
        "dense",        /* 25% density */
        "alternating",  /* Worst case */
        "boundary"      /* SIMD boundary tests */
    };
    size_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    /* Allocate results array */
    benchmark_result_t* results = calloc(num_patterns, sizeof(benchmark_result_t));

    /* Allocate output buffer (10MB should be enough) */
    size_t output_capacity = 10 * 1024 * 1024;
    char* output_buffer = malloc(output_capacity);
    char* naive_output = malloc(output_capacity);

    /* Run benchmarks */
    for (size_t i = 0; i < num_patterns; i++) {
        if (verbose && json_mode == JSON_NONE) {
            printf("Running benchmark: %s...\n", patterns[i]);
            fflush(stdout);
        }

        /* Generate test data */
        size_t input_len, emdash_count;
        char* test_data = generate_test_data(patterns[i], &input_len, &emdash_count);

        /* Initialize result */
        results[i].name = patterns[i];
        results[i].input_size = input_len;
        results[i].emdash_count = emdash_count;
        results[i].correct = true;

        /* Run dashem benchmark */
        run_benchmark(&results[i], test_data, input_len,
                     output_buffer, output_capacity, false);

        /* Run naive benchmark for comparison */
        benchmark_result_t naive_result = {0};
        naive_result.correct = true;
        run_benchmark(&naive_result, test_data, input_len,
                     naive_output, output_capacity, true);

        /* Validate correctness */
        if (results[i].output_size != naive_result.output_size ||
            memcmp(output_buffer, naive_output, results[i].output_size) != 0) {
            results[i].correct = false;
            snprintf(results[i].error_msg, sizeof(results[i].error_msg),
                    "Output mismatch: dashem=%zu bytes, naive=%zu bytes",
                    results[i].output_size, naive_result.output_size);
        }

        /* Calculate speedup */
        results[i].speedup_vs_naive = naive_result.median / results[i].median;

        /* Cleanup */
        free(test_data);
        free(naive_result.timings);
    }

    /* Output results */
    if (json_mode != JSON_NONE) {
        output_json(results, num_patterns, json_mode);
    } else {
        output_table(results, num_patterns);
    }

    /* Cleanup */
    for (size_t i = 0; i < num_patterns; i++) {
        free(results[i].timings);
    }
    free(results);
    free(output_buffer);
    free(naive_output);

    return 0;
}
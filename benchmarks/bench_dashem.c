/**
 * @file bench_dashem.c
 * @brief Comprehensive benchmark suite for dash-em
 *
 * This program benchmarks the em-dash removal performance across
 * different input sizes and implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
    #include <windows.h>
#endif
#include "../src/dashem.h"

#define ITERATIONS 1000
#define EMDASH_UTF8 "\xe2\x80\x94"

/**
 * Generate test string with specified number of em-dashes
 */
static char* generate_test_string(size_t num_dashes, size_t* out_len) {
    /* Each dash = 3 bytes (UTF-8), surrounded by spaces */
    size_t total_len = num_dashes * 15; /* 3 bytes dash + 12 bytes text */
    char* str = (char*)malloc(total_len);

    size_t pos = 0;
    for (size_t i = 0; i < num_dashes; i++) {
        int written = snprintf(str + pos, total_len - pos, "Lorem ipsum—");
        if (written < 0 || (size_t)written >= total_len - pos) {
            /* Buffer overflow protection */
            break;
        }
        pos += written;
    }
    str[pos] = '\0';

    *out_len = pos;
    return str;
}

/**
 * Naive string replacement implementation
 */
static char* naive_remove(const char* input, size_t input_len, size_t* out_len) {
    char* output = (char*)malloc(input_len);
    size_t out_idx = 0;

    for (size_t i = 0; i < input_len; ) {
        if (i + 2 < input_len &&
            (unsigned char)input[i] == 0xe2 &&
            (unsigned char)input[i + 1] == 0x80 &&
            (unsigned char)input[i + 2] == 0x94) {
            i += 3;
        } else {
            output[out_idx++] = input[i++];
        }
    }

    *out_len = out_idx;
    return output;
}

/**
 * Measure time in microseconds
 */
static double get_time_us(void) {
#ifdef _WIN32
    /* Windows implementation using QueryPerformanceCounter */
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart * 1e6;
#else
    /* POSIX implementation using clock_gettime */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
#endif
}

/**
 * Benchmark a specific function
 */
typedef struct {
    const char* name;
    double time_us;
    size_t output_size;
} BenchResult;

static BenchResult benchmark_impl(
    const char* name,
    char* (*impl)(const char*, size_t, size_t*),
    const char* input,
    size_t input_len
) {
    BenchResult result = {name, 0, 0};

    double start = get_time_us();
    for (int i = 0; i < ITERATIONS; i++) {
        size_t out_len = 0;
        char* output = impl(input, input_len, &out_len);
        result.output_size = out_len;
        free(output);
    }
    double end = get_time_us();

    result.time_us = (end - start) / ITERATIONS;
    return result;
}

static BenchResult benchmark_dashem(const char* input, size_t input_len) {
    BenchResult result = {"dash-em", 0, 0};

    double start = get_time_us();
    for (int i = 0; i < ITERATIONS; i++) {
        char output_buf[1024 * 1024];
        size_t out_len = 0;
        dashem_remove(input, input_len, output_buf, sizeof(output_buf), &out_len);
        result.output_size = out_len;
    }
    double end = get_time_us();

    result.time_us = (end - start) / ITERATIONS;
    return result;
}

int main(void) {
    printf("=============================================================================\n");
    printf("dash-em Performance Benchmark Suite\n");
    printf("Implementation: %s\n", dashem_implementation_name());
    printf("=============================================================================\n\n");

    /* Test configurations */
    struct {
        size_t num_dashes;
        const char* label;
    } tests[] = {
        {100, "100 em-dashes"},
        {1000, "1,000 em-dashes"},
        {10000, "10,000 em-dashes"},
    };

    for (size_t t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        size_t input_len = 0;
        char* input = generate_test_string(tests[t].num_dashes, &input_len);

        printf("Benchmark: %s (Input size: %zu bytes)\n", tests[t].label, input_len);
        printf("───────────────────────────────────────────────────────────────────────────\n");

        BenchResult naive = benchmark_impl("Naive Implementation", naive_remove, input, input_len);
        BenchResult dashem = benchmark_dashem(input, input_len);

        printf("%-30s | %12.2f µs | %12.2f ms\n", naive.name, naive.time_us, naive.time_us / 1000);
        printf("%-30s | %12.2f µs | %12.2f ms\n", dashem.name, dashem.time_us, dashem.time_us / 1000);

        double speedup = naive.time_us / dashem.time_us;
        printf("%-30s | %12.2fx speedup\n", "Relative Performance", speedup);
        printf("\n");

        free(input);
    }

    printf("=============================================================================\n");
    printf("Benchmark complete!\n");
    printf("=============================================================================\n");

    return 0;
}

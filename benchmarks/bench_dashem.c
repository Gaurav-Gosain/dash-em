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

/* Pragma compatibility for different compilers */
#if defined(_MSC_VER)
    #define PRAGMA_IGNORE_UNUSED_FUNCTION_START
    #define PRAGMA_IGNORE_UNUSED_FUNCTION_END
#else
    #define PRAGMA_IGNORE_UNUSED_FUNCTION_START \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
    #define PRAGMA_IGNORE_UNUSED_FUNCTION_END \
        _Pragma("GCC diagnostic pop")
#endif

#define ITERATIONS 1000
#define EMDASH_UTF8 "\xe2\x80\x94"

/* Adaptive iterations based on input size */
#define GET_ITERATIONS(input_len) ((input_len) > 500000 ? 100 : ITERATIONS)

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
 * Generate test string with specified em-dash density percentage
 * Note: Currently unused, but kept for potential future use in extended benchmarks
 */
PRAGMA_IGNORE_UNUSED_FUNCTION_START
static char* generate_test_with_density(size_t base_size, int density_percent, size_t* out_len) {
    char* str = (char*)malloc(base_size * 2);
    size_t pos = 0;

    for (size_t i = 0; i < base_size; i += 100) {
        if ((rand() % 100) < density_percent) {
            /* Insert em-dash (3 bytes UTF-8) */
            if (pos + 3 <= base_size * 2) {
                memcpy(str + pos, "\xe2\x80\x94", 3);
                pos += 3;
            }
        } else {
            /* Insert regular text */
            for (int j = 0; j < 10 && pos < base_size * 2; j++) {
                str[pos++] = (char)('A' + (rand() % 26));
            }
        }
    }

    *out_len = pos;
    return str;
}
PRAGMA_IGNORE_UNUSED_FUNCTION_END

/**
 * Generate test string with NO em-dashes (fast path test)
 */
static char* generate_no_dashes(size_t size, size_t* out_len) {
    char* str = (char*)malloc(size);
    for (size_t i = 0; i < size - 1; i++) {
        str[i] = (char)('A' + (i % 26));
    }
    str[size - 1] = '\0';
    *out_len = size - 1;
    return str;
}

/**
 * Generate test string with alternating pattern (very dense em-dashes)
 */
static char* generate_alternating_dashes(size_t num_dashes, size_t* out_len) {
    char* str = (char*)malloc(num_dashes * 4);
    size_t pos = 0;

    for (size_t i = 0; i < num_dashes; i++) {
        /* Alternate: em-dash, single char, em-dash, single char, ... */
        memcpy(str + pos, "\xe2\x80\x94", 3);
        pos += 3;
        str[pos++] = 'a';
    }

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
    char* reference_output;  /* For validation */
    size_t reference_len;
} BenchResult;

static BenchResult benchmark_impl(
    const char* name,
    char* (*impl)(const char*, size_t, size_t*),
    const char* input,
    size_t input_len
) {
    BenchResult result = {name, 0, 0, NULL, 0};
    int iterations = GET_ITERATIONS(input_len);

    /* First run: get reference output for validation */
    size_t ref_len = 0;
    char* ref_output = impl(input, input_len, &ref_len);
    result.reference_output = ref_output;
    result.reference_len = ref_len;

    double start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        size_t out_len = 0;
        char* output = impl(input, input_len, &out_len);
        result.output_size = out_len;

        /* Validate output matches reference */
        if (out_len != ref_len || memcmp(output, ref_output, out_len) != 0) {
            fprintf(stderr, "ERROR: %s produced incorrect output!\n", name);
            fprintf(stderr, "  Expected size: %zu, Got: %zu\n", ref_len, out_len);
            free(output);
            exit(1);
        }
        free(output);
    }
    double end = get_time_us();

    result.time_us = (end - start) / iterations;
    return result;
}

static BenchResult benchmark_dashem(const char* input, size_t input_len, const char* reference, size_t ref_len) {
    BenchResult result = {"dash-em", 0, 0, NULL, ref_len};
    int iterations = GET_ITERATIONS(input_len);

    double start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        char output_buf[1024 * 1024];
        size_t out_len = 0;
        dashem_remove(input, input_len, output_buf, sizeof(output_buf), &out_len);
        result.output_size = out_len;

        /* Validate output matches reference */
        if (out_len != ref_len || memcmp(output_buf, reference, out_len) != 0) {
            fprintf(stderr, "ERROR: dash-em produced incorrect output!\n");
            fprintf(stderr, "  Expected size: %zu, Got: %zu\n", ref_len, out_len);
            exit(1);
        }
    }
    double end = get_time_us();

    result.time_us = (end - start) / iterations;
    return result;
}

int main(void) {
    printf("================================================================================\n");
    printf("dash-em: Enhanced Performance Benchmark Suite\n");
    printf("Implementation: %s\n", dashem_implementation_name());
    printf("================================================================================\n\n");

    srand(12345);  /* Deterministic random seed for reproducibility */

    /* Test configurations: various patterns and densities */
    struct BenchConfig {
        char* (*generator)(size_t, size_t*);
        size_t param;
        const char* label;
        const char* description;
    } tests[] = {
        /* Pattern 1: No em-dashes (fast path) */
        {generate_no_dashes, 100000, "NO EM-DASHES", "Fast path: no patterns to match"},

        /* Pattern 2: Regular pattern with standard density */
        {generate_test_string, 1000, "REGULAR PATTERN", "Standard: 1000 em-dashes"},

        /* Pattern 3: Alternating/Dense pattern */
        {generate_alternating_dashes, 5000, "ALTERNATING PATTERN", "Dense: em-dash every 4 bytes"},

        /* Pattern 4: Very large input */
        {generate_test_string, 100000, "LARGE INPUT (100K DASHES)", "Large-scale: 100K em-dashes"},
    };

    double total_dashem_time = 0;
    int test_count = 0;

    for (size_t t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        size_t input_len = 0;
        char* input = tests[t].generator(tests[t].param, &input_len);

        printf("[Test %zu/%zu] %s\n", t + 1, sizeof(tests) / sizeof(tests[0]), tests[t].label);
        printf("             %s\n", tests[t].description);
        printf("             Input size: %zu bytes\n", input_len);
        printf("───────────────────────────────────────────────────────────────────────────\n");

        BenchResult naive = benchmark_impl("Naive Implementation", naive_remove, input, input_len);
        BenchResult dashem = benchmark_dashem(input, input_len, naive.reference_output, naive.reference_len);

        /* Format with adaptive precision for very small times */
        if (naive.time_us < 0.01) {
            printf("  %-28s: %12.6f µs (%10.6f ms)\n", naive.name, naive.time_us, naive.time_us / 1000);
        } else {
            printf("  %-28s: %12.2f µs (%10.3f ms)\n", naive.name, naive.time_us, naive.time_us / 1000);
        }

        if (dashem.time_us < 0.01) {
            printf("  %-28s: %12.6f µs (%10.6f ms)\n", dashem.name, dashem.time_us, dashem.time_us / 1000);
        } else {
            printf("  %-28s: %12.2f µs (%10.3f ms)\n", dashem.name, dashem.time_us, dashem.time_us / 1000);
        }

        double speedup = naive.time_us / dashem.time_us;
        double throughput_gb_s = ((double)input_len / (1024.0 * 1024.0 * 1024.0)) / (dashem.time_us / 1e6);
        printf("  %-28s: %12.2fx speedup\n", "Speedup", speedup);
        printf("  %-28s: %12.2f GB/s\n", "Throughput", throughput_gb_s);
        printf("\n");

        total_dashem_time += dashem.time_us;
        test_count++;
        free(input);
        free(naive.reference_output);  /* Free reference output after validation */
    }

    printf("================================================================================\n");
    printf("Summary Statistics:\n");
    printf("  Total tests run: %d\n", test_count);
    printf("  Average time per test: %.2f µs\n", total_dashem_time / test_count);
    printf("================================================================================\n");
    printf("Benchmark complete!\n");
    printf("================================================================================\n");

    return 0;
}

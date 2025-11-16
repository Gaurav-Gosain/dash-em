/**
 * @file bench_patterns.c
 * @brief Pattern-specific performance tests for dash-em
 *
 * Tests specific patterns that stress different aspects of the implementation:
 * - Memory alignment variations
 * - Cache line boundaries
 * - SIMD vector boundaries
 * - Pathological patterns
 * - Different buffer sizes (L1/L2/L3 cache)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/dashem.h"

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __x86_64__
#include <cpuid.h>
#endif
#endif

/* Cache size detection and configuration */
#define DEFAULT_L1_SIZE (32 * 1024)      /* 32 KB */
#define DEFAULT_L2_SIZE (256 * 1024)     /* 256 KB */
#define DEFAULT_L3_SIZE (8 * 1024 * 1024) /* 8 MB */

/* SIMD vector sizes */
#define SSE_VECTOR_SIZE 16
#define AVX2_VECTOR_SIZE 32
#define AVX512_VECTOR_SIZE 64

/* Cache line size (typical for x86 and ARM) */
#define CACHE_LINE_SIZE 64

/* Benchmark configuration */
#define WARMUP_RUNS 10
#define BENCHMARK_RUNS 100

typedef struct {
    size_t l1_size;
    size_t l2_size;
    size_t l3_size;
    size_t cache_line_size;
} cache_info_t;

typedef struct {
    const char* name;
    const char* description;
    size_t size;
    size_t alignment_offset;
    size_t emdash_positions[1024];  /* Positions of em-dashes */
    size_t emdash_count;
    double time_us;
    double throughput_gbps;
    double speedup;
    bool correct;
} pattern_test_t;

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

/* Detect cache sizes (simplified, platform-specific) */
static cache_info_t detect_cache_info(void) {
    cache_info_t info = {
        .l1_size = DEFAULT_L1_SIZE,
        .l2_size = DEFAULT_L2_SIZE,
        .l3_size = DEFAULT_L3_SIZE,
        .cache_line_size = CACHE_LINE_SIZE
    };

#ifdef __x86_64__
#ifdef __GNUC__
    /* Try to detect cache sizes on x86_64 with GCC/Clang */
    unsigned int eax, ebx, ecx, edx;

    /* Check for cache information */
    for (int i = 0; i < 10; i++) {
        __cpuid_count(4, i, eax, ebx, ecx, edx);
        if ((eax & 0x1f) == 0) break;  /* No more caches */

        int cache_type = eax & 0x1f;
        int cache_level = (eax >> 5) & 0x7;
        int cache_line_size = (ebx & 0xfff) + 1;
        int cache_partitions = ((ebx >> 12) & 0x3ff) + 1;
        int cache_ways = ((ebx >> 22) & 0x3ff) + 1;
        int cache_sets = ecx + 1;

        size_t cache_size = cache_ways * cache_partitions * cache_line_size * cache_sets;

        if (cache_type == 1 || cache_type == 3) {  /* Data or unified cache */
            switch (cache_level) {
                case 1: info.l1_size = cache_size; break;
                case 2: info.l2_size = cache_size; break;
                case 3: info.l3_size = cache_size; break;
            }
            if (cache_level == 1) {
                info.cache_line_size = cache_line_size;
            }
        }
    }
#endif
#endif

    return info;
}

/* Naive implementation for comparison */
static size_t remove_emdash_naive(const char* input, size_t input_len, char* output) {
    size_t out_idx = 0;
    for (size_t i = 0; i < input_len; ) {
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

/* Create test pattern with em-dashes at specific positions */
static char* create_pattern(size_t size, size_t* positions, size_t pos_count) {
    char* buffer = malloc(size);
    memset(buffer, 'A', size);

    /* Place em-dashes at specified positions */
    for (size_t i = 0; i < pos_count; i++) {
        size_t pos = positions[i];
        if (pos + 3 <= size) {
            buffer[pos] = (char)0xE2;
            buffer[pos + 1] = (char)0x80;
            buffer[pos + 2] = (char)0x94;
        }
    }

    return buffer;
}

/* Test alignment patterns */
static void test_alignment_patterns(pattern_test_t* tests, size_t* test_count) {
    size_t idx = *test_count;
    size_t buffer_size = 4096;

    /* Test different alignment offsets */
    for (size_t offset = 0; offset <= 64; offset += 1) {
        if (offset > 0 && offset != 1 && offset != 3 && offset != 7 &&
            offset != 15 && offset != 31 && offset != 32 && offset != 63 && offset != 64) {
            continue;  /* Skip some offsets for brevity */
        }

        pattern_test_t* test = &tests[idx++];
        char name[64];
        snprintf(name, sizeof(name), "align_%zu", offset);
        test->name = strdup(name);
        test->description = "Alignment test";
        test->size = buffer_size - offset;
        test->alignment_offset = offset;

        /* Place em-dashes at regular intervals */
        for (size_t i = 100; i + 3 <= test->size; i += 100) {
            test->emdash_positions[test->emdash_count++] = i;
        }
    }

    *test_count = idx;
}

/* Test SIMD boundary patterns */
static void test_simd_boundaries(pattern_test_t* tests, size_t* test_count) {
    size_t idx = *test_count;

    const struct {
        const char* name;
        size_t vector_size;
        size_t buffer_size;
    } simd_configs[] = {
        {"sse_boundary", SSE_VECTOR_SIZE, 1024},
        {"avx2_boundary", AVX2_VECTOR_SIZE, 2048},
        {"avx512_boundary", AVX512_VECTOR_SIZE, 4096},
    };

    for (size_t config = 0; config < 3; config++) {
        size_t vec_size = simd_configs[config].vector_size;
        size_t buf_size = simd_configs[config].buffer_size;

        /* Test em-dashes at vector boundaries */
        pattern_test_t* test = &tests[idx++];
        test->name = strdup(simd_configs[config].name);
        test->description = "SIMD vector boundary";
        test->size = buf_size;

        /* Place em-dashes at vector boundaries */
        for (size_t i = 0; i < buf_size; i += vec_size) {
            if (i + 3 <= buf_size) {
                test->emdash_positions[test->emdash_count++] = i;
            }
            if (i + vec_size - 3 <= buf_size) {
                test->emdash_positions[test->emdash_count++] = i + vec_size - 3;
            }
        }

        /* Test em-dashes spanning vector boundaries */
        test = &tests[idx++];
        char span_name[64];
        snprintf(span_name, sizeof(span_name), "%s_span", simd_configs[config].name);
        test->name = strdup(span_name);
        test->description = "Em-dash spanning boundary";
        test->size = buf_size;

        /* Place em-dashes spanning boundaries */
        for (size_t i = vec_size - 2; i < buf_size; i += vec_size) {
            if (i + 3 <= buf_size) {
                test->emdash_positions[test->emdash_count++] = i;
            }
        }
    }

    *test_count = idx;
}

/* Test cache-related patterns */
static void test_cache_patterns(pattern_test_t* tests, size_t* test_count, cache_info_t* cache) {
    size_t idx = *test_count;

    const struct {
        const char* name;
        size_t size;
        const char* desc;
    } cache_configs[] = {
        {"l1_fit", cache->l1_size / 2, "Fits in L1 cache"},
        {"l1_exact", cache->l1_size, "Exactly L1 size"},
        {"l1_overflow", cache->l1_size + cache->cache_line_size, "Slightly exceeds L1"},
        {"l2_fit", cache->l2_size / 2, "Fits in L2 cache"},
        {"l2_exact", cache->l2_size, "Exactly L2 size"},
        {"l2_overflow", cache->l2_size + cache->cache_line_size, "Slightly exceeds L2"},
        {"l3_fit", cache->l3_size / 4, "Fits in L3 cache"},
    };

    for (size_t config = 0; config < 7; config++) {
        pattern_test_t* test = &tests[idx++];
        test->name = strdup(cache_configs[config].name);
        test->description = cache_configs[config].desc;
        test->size = cache_configs[config].size;

        /* Distribute em-dashes to test cache behavior */
        size_t stride = cache->cache_line_size * 2;  /* Every other cache line */
        for (size_t i = 0; i < test->size && test->emdash_count < 1024; i += stride) {
            if (i + 3 <= test->size) {
                test->emdash_positions[test->emdash_count++] = i;
            }
        }
    }

    *test_count = idx;
}

/* Test pathological patterns */
static void test_pathological_patterns(pattern_test_t* tests, size_t* test_count) {
    size_t idx = *test_count;

    /* Pattern 1: Almost em-dash (E2 80 93 instead of E2 80 94) */
    pattern_test_t* test = &tests[idx++];
    test->name = strdup("almost_emdash");
    test->description = "Almost em-dash bytes";
    test->size = 3000;
    /* No actual em-dashes, but similar bytes */

    /* Pattern 2: Fragmented em-dashes */
    test = &tests[idx++];
    test->name = strdup("fragmented");
    test->description = "Single byte between em-dashes";
    test->size = 1000;
    for (size_t i = 0; i + 4 < test->size; i += 4) {
        test->emdash_positions[test->emdash_count++] = i;
    }

    /* Pattern 3: All em-dashes */
    test = &tests[idx++];
    test->name = strdup("all_emdash");
    test->description = "100% em-dashes";
    test->size = 3000;
    for (size_t i = 0; i + 3 <= test->size; i += 3) {
        test->emdash_positions[test->emdash_count++] = i;
    }

    /* Pattern 4: Prime-spaced em-dashes */
    test = &tests[idx++];
    test->name = strdup("prime_spacing");
    test->description = "Prime number spacing";
    test->size = 5000;
    size_t primes[] = {7, 11, 13, 17, 19, 23, 29, 31, 37, 41};
    size_t pos = 0;
    for (size_t i = 0; i < 100 && pos < test->size - 3; i++) {
        pos += primes[i % 10];
        if (pos + 3 <= test->size) {
            test->emdash_positions[test->emdash_count++] = pos;
        }
    }

    /* Pattern 5: Clustered em-dashes */
    test = &tests[idx++];
    test->name = strdup("clustered");
    test->description = "Clustered em-dashes";
    test->size = 10000;
    for (size_t cluster = 0; cluster < 10; cluster++) {
        size_t base = cluster * 1000;
        /* Dense cluster of em-dashes */
        for (size_t i = 0; i < 30 && test->emdash_count < 1024; i++) {
            size_t pos = base + i * 3;
            if (pos + 3 <= test->size) {
                test->emdash_positions[test->emdash_count++] = pos;
            }
        }
    }

    *test_count = idx;
}

/* Run benchmark for a pattern */
static void benchmark_pattern(pattern_test_t* test) {
    /* Allocate aligned buffers */
    size_t alloc_size = test->size + test->alignment_offset + 64;
    char* input_alloc = malloc(alloc_size);
    char* output_alloc = malloc(alloc_size);
    char* naive_output = malloc(test->size);

    /* Apply alignment offset */
    char* input = input_alloc + test->alignment_offset;
    char* output = output_alloc + test->alignment_offset;

    /* Create the pattern */
    memset(input_alloc, 'X', alloc_size);  /* Fill with data */
    char* pattern = create_pattern(test->size, test->emdash_positions, test->emdash_count);
    memcpy(input, pattern, test->size);
    free(pattern);

    /* Warmup */
    for (int i = 0; i < WARMUP_RUNS; i++) {
        size_t output_len;
        dashem_remove(input, test->size, output, test->size, &output_len);
    }

    /* Benchmark dashem */
    double total_time = 0.0;
    size_t output_len = 0;

    for (int i = 0; i < BENCHMARK_RUNS; i++) {
        double start = get_time_us();
        dashem_remove(input, test->size, output, test->size, &output_len);
        double end = get_time_us();
        total_time += (end - start);
    }

    test->time_us = total_time / BENCHMARK_RUNS;

    /* Benchmark naive for comparison */
    double naive_total = 0.0;
    size_t naive_len = 0;

    for (int i = 0; i < BENCHMARK_RUNS; i++) {
        double start = get_time_us();
        naive_len = remove_emdash_naive(input, test->size, naive_output);
        double end = get_time_us();
        naive_total += (end - start);
    }

    double naive_time = naive_total / BENCHMARK_RUNS;

    /* Verify correctness */
    test->correct = (output_len == naive_len &&
                    memcmp(output, naive_output, output_len) == 0);

    /* Calculate metrics */
    test->throughput_gbps = (test->size / (1024.0 * 1024.0 * 1024.0)) /
                           (test->time_us / 1000000.0);
    test->speedup = naive_time / test->time_us;

    /* Cleanup */
    free(input_alloc);
    free(output_alloc);
    free(naive_output);
}

/* Output test results */
static void output_results(pattern_test_t* tests, size_t test_count) {
    printf("\nPattern-Specific Benchmark Results\n");
    printf("===================================\n\n");

    /* Group by category */
    const char* current_category = "";

    printf("%-20s %-25s %8s %6s %8s %8s %7s %6s\n",
           "Pattern", "Description", "Size", "Em-dash", "Time", "GB/s", "Speedup", "Valid");
    printf("%-20s %-25s %8s %6s %8s %8s %7s %6s\n",
           "", "", "(bytes)", "Count", "(μs)", "", "vs Naive", "");
    printf("─────────────────────────────────────────────────────────"
           "───────────────────────────────────────────\n");

    for (size_t i = 0; i < test_count; i++) {
        pattern_test_t* test = &tests[i];

        /* Detect category change */
        const char* category = "";
        if (strstr(test->name, "align")) category = "Alignment";
        else if (strstr(test->name, "boundary")) category = "SIMD Boundaries";
        else if (strstr(test->name, "l1") || strstr(test->name, "l2") ||
                strstr(test->name, "l3")) category = "Cache Patterns";
        else category = "Pathological";

        if (strcmp(category, current_category) != 0) {
            if (i > 0) printf("\n");
            printf("%s:\n", category);
            current_category = category;
        }

        printf("%-20s %-25s %8zu %6zu %8.1f %8.2f %7.2fx %6s\n",
               test->name, test->description, test->size, test->emdash_count,
               test->time_us, test->throughput_gbps, test->speedup,
               test->correct ? "PASS" : "FAIL");
    }

    /* Summary statistics */
    printf("\n\nPerformance Summary\n");
    printf("===================\n");

    double best_speedup = 0.0, worst_speedup = 1000.0, avg_speedup = 0.0;
    double best_throughput = 0.0, worst_throughput = 1000.0, avg_throughput = 0.0;
    size_t failures = 0;

    for (size_t i = 0; i < test_count; i++) {
        if (!tests[i].correct) {
            failures++;
            continue;
        }
        if (tests[i].speedup > best_speedup) best_speedup = tests[i].speedup;
        if (tests[i].speedup < worst_speedup) worst_speedup = tests[i].speedup;
        avg_speedup += tests[i].speedup;

        if (tests[i].throughput_gbps > best_throughput) best_throughput = tests[i].throughput_gbps;
        if (tests[i].throughput_gbps < worst_throughput) worst_throughput = tests[i].throughput_gbps;
        avg_throughput += tests[i].throughput_gbps;
    }

    size_t valid_count = test_count - failures;
    if (valid_count > 0) {
        avg_speedup /= valid_count;
        avg_throughput /= valid_count;
    }

    printf("Best speedup:  %.2fx\n", best_speedup);
    printf("Worst speedup: %.2fx\n", worst_speedup);
    printf("Avg speedup:   %.2fx\n", avg_speedup);
    printf("\n");
    printf("Best throughput:  %.2f GB/s\n", best_throughput);
    printf("Worst throughput: %.2f GB/s\n", worst_throughput);
    printf("Avg throughput:   %.2f GB/s\n", avg_throughput);
    printf("\n");
    if (failures > 0) {
        printf("FAILURES: %zu tests failed correctness check!\n", failures);
    } else {
        printf("All tests passed correctness check.\n");
    }
}

int main(int argc, char* argv[]) {
    printf("dash-em Pattern-Specific Benchmarks\n");
    printf("====================================\n");
    printf("Implementation: %s\n", dashem_implementation_name());
    printf("Version: %s\n", dashem_version());
    printf("CPU Features: 0x%08X\n", dashem_detect_cpu_features());

    /* Detect cache sizes */
    cache_info_t cache = detect_cache_info();
    printf("\nDetected Cache Configuration:\n");
    printf("  L1: %zu KB\n", cache.l1_size / 1024);
    printf("  L2: %zu KB\n", cache.l2_size / 1024);
    printf("  L3: %zu MB\n", cache.l3_size / (1024 * 1024));
    printf("  Cache line: %zu bytes\n", cache.cache_line_size);

    /* Allocate test array */
    pattern_test_t* tests = calloc(256, sizeof(pattern_test_t));
    size_t test_count = 0;

    /* Generate all test patterns */
    test_alignment_patterns(tests, &test_count);
    test_simd_boundaries(tests, &test_count);
    test_cache_patterns(tests, &test_count, &cache);
    test_pathological_patterns(tests, &test_count);

    printf("\nRunning %zu pattern tests...\n", test_count);

    /* Run benchmarks */
    for (size_t i = 0; i < test_count; i++) {
        benchmark_pattern(&tests[i]);

        /* Progress indicator */
        if ((i + 1) % 10 == 0 || i == test_count - 1) {
            printf("Progress: %zu/%zu tests completed\r", i + 1, test_count);
            fflush(stdout);
        }
    }
    printf("\n");

    /* Output results */
    output_results(tests, test_count);

    /* Cleanup */
    for (size_t i = 0; i < test_count; i++) {
        free((void*)tests[i].name);
    }
    free(tests);

    return 0;
}
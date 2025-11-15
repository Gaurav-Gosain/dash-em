/**
 * @file comprehensive_tests.c
 * @brief Comprehensive correctness test suite for dash-em
 *
 * Tests cover edge cases, boundary conditions, and pathological inputs
 * to ensure correctness before performance claims.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/dashem.h"

#define EMDASH_UTF8 "\xe2\x80\x94"  /* U+2014 in UTF-8 */
#define TEST_BUF_SIZE (10 * 1024 * 1024)  /* 10MB test buffer */

typedef struct {
    const char* name;
    const char* input;
    size_t input_len;
    const char* expected_output;
    size_t expected_len;
    int should_pass;
} TestCase;

static char test_output_buf[TEST_BUF_SIZE];
static char expected_buf[TEST_BUF_SIZE];

/**
 * Helper: Compare outputs exactly
 */
static int verify_output(const char* actual, size_t actual_len,
                        const char* expected, size_t expected_len) {
    if (actual_len != expected_len) {
        fprintf(stderr, "Length mismatch: expected %zu, got %zu\n", expected_len, actual_len);
        return 0;
    }

    if (memcmp(actual, expected, actual_len) != 0) {
        fprintf(stderr, "Content mismatch at byte:\n");
        for (size_t i = 0; i < actual_len; i++) {
            if (actual[i] != expected[i]) {
                fprintf(stderr, "  Position %zu: expected 0x%02x, got 0x%02x\n",
                        i, (unsigned char)expected[i], (unsigned char)actual[i]);
                break;
            }
        }
        return 0;
    }
    return 1;
}

/**
 * Test 1: Empty string
 */
static int test_empty_string(void) {
    size_t out_len = 0;
    int result = dashem_remove("", 0, test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "Empty string test failed: expected 0, got %d\n", result);
        return 0;
    }
    if (out_len != 0) {
        fprintf(stderr, "Empty string test failed: expected output len 0, got %zu\n", out_len);
        return 0;
    }
    return 1;
}

/**
 * Test 2: No em-dashes (should pass through unchanged)
 */
static int test_no_emdashes(void) {
    const char* input = "Hello world this is a test";
    size_t input_len = strlen(input);
    size_t out_len = 0;

    int result = dashem_remove(input, input_len, test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "No em-dashes test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, input, input_len);
}

/**
 * Test 3: Single em-dash at start
 */
static int test_emdash_at_start(void) {
    const char* input = EMDASH_UTF8 "Hello";
    const char* expected = "Hello";
    size_t out_len = 0;

    int result = dashem_remove(input, strlen(input), test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "Em-dash at start test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, strlen(expected));
}

/**
 * Test 4: Single em-dash at end
 */
static int test_emdash_at_end(void) {
    const char* input = "Hello" EMDASH_UTF8;
    const char* expected = "Hello";
    size_t out_len = 0;

    int result = dashem_remove(input, strlen(input), test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "Em-dash at end test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, strlen(expected));
}

/**
 * Test 5: Multiple consecutive em-dashes
 */
static int test_consecutive_emdashes(void) {
    char input[100];
    snprintf(input, sizeof(input), "A" EMDASH_UTF8 EMDASH_UTF8 EMDASH_UTF8 "B");
    const char* expected = "AB";
    size_t out_len = 0;

    int result = dashem_remove(input, strlen(input), test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "Consecutive em-dashes test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, strlen(expected));
}

/**
 * Test 6: Boundary test - exactly 32 bytes with em-dash at end (SSE boundary)
 */
static int test_32byte_boundary(void) {
    char input[64];
    memset(input, 'A', 29);
    memcpy(input + 29, EMDASH_UTF8, 3);
    size_t input_len = 32;

    char expected[64];
    memset(expected, 'A', 29);
    size_t expected_len = 29;

    size_t out_len = 0;
    int result = dashem_remove(input, input_len, test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "32-byte boundary test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, expected_len);
}

/**
 * Test 7: Boundary test - exactly 64 bytes with em-dash at end (AVX2 boundary)
 */
static int test_64byte_boundary(void) {
    char input[128];
    memset(input, 'A', 61);
    memcpy(input + 61, EMDASH_UTF8, 3);
    size_t input_len = 64;

    char expected[128];
    memset(expected, 'A', 61);
    size_t expected_len = 61;

    size_t out_len = 0;
    int result = dashem_remove(input, input_len, test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "64-byte boundary test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, expected_len);
}

/**
 * Test 8: The original failing case - 1000 em-dashes in "Lorem ipsum—" pattern
 */
static int test_lorem_ipsum_1000(void) {
    char* input = (char*)malloc(14000 + 1);
    char* expected = (char*)malloc(11000 + 1);

    size_t input_pos = 0, expected_pos = 0;
    for (int i = 0; i < 1000; i++) {
        input_pos += sprintf(input + input_pos, "Lorem ipsum—");
        expected_pos += sprintf(expected + expected_pos, "Lorem ipsum");
    }

    size_t out_len = 0;
    int result = dashem_remove(input, input_pos, test_output_buf, TEST_BUF_SIZE, &out_len);

    free(input);
    free(expected);

    if (result != 0) {
        fprintf(stderr, "Lorem ipsum 1000 test failed: expected 0, got %d\n", result);
        return 0;
    }

    return verify_output(test_output_buf, out_len, expected, expected_pos);
}

/**
 * Test 9: Em-dash at various alignment positions (unaligned access test)
 */
static int test_unaligned_positions(void) {
    for (int offset = 0; offset < 64; offset++) {
        char input[256];
        memset(input, 'X', sizeof(input));
        memcpy(input + offset, EMDASH_UTF8, 3);

        char expected[256];
        memset(expected, 'X', offset);
        expected[offset] = 'Y';  /* Verify we can find dashes at any position */

        /* For this test, just verify it doesn't crash and produces shorter output */
        size_t out_len = 0;
        int result = dashem_remove(input, offset + 10, test_output_buf, TEST_BUF_SIZE, &out_len);

        if (result != 0) {
            fprintf(stderr, "Unaligned position test failed at offset %d\n", offset);
            return 0;
        }

        if (out_len != offset + 10 - 3) {
            /* Should have exactly offset+7 bytes: input length minus 3-byte em-dash */
            fprintf(stderr, "Unaligned test: incorrect output length at offset %d (expected %d, got %zu)\n",
                    offset, offset + 7, out_len);
            return 0;
        }
    }
    return 1;
}

/**
 * Test 10: Very large input (2MB with mixed content)
 */
static int test_large_input(void) {
    size_t input_size = 2 * 1024 * 1024;
    char* input = (char*)malloc(input_size);

    /* Fill with pattern: "Hello World—" repeated */
    size_t pos = 0;
    int dash_count = 0;
    while (pos + 13 <= input_size) {
        pos += sprintf(input + pos, "Hello World—");
        dash_count++;
        if (pos >= input_size) break;
    }

    size_t out_len = 0;
    int result = dashem_remove(input, pos, test_output_buf, TEST_BUF_SIZE, &out_len);

    /* Expected: each "Hello World—" (13 bytes) becomes "Hello World" (11 bytes) */
    size_t expected_len = dash_count * 11;

    free(input);

    if (result != 0) {
        fprintf(stderr, "Large input test failed: expected 0, got %d\n", result);
        return 0;
    }

    if (out_len != expected_len) {
        fprintf(stderr, "Large input test failed: expected %zu bytes, got %zu\n", expected_len, out_len);
        return 0;
    }

    return 1;
}

/**
 * Test 11: Output buffer too small
 */
static int test_buffer_overflow_protection(void) {
    const char* input = "Hello—World";
    size_t out_len = 0;
    char small_buf[3];

    int result = dashem_remove(input, strlen(input), small_buf, sizeof(small_buf), &out_len);

    /* Should return error when output capacity is insufficient */
    if (result == 0) {
        fprintf(stderr, "Buffer overflow protection test failed: should have returned error\n");
        return 0;
    }

    return 1;
}

/**
 * Test 12: Only em-dashes (no other content)
 */
static int test_only_emdashes(void) {
    char input[100];
    size_t input_len = 0;
    for (int i = 0; i < 10; i++) {
        memcpy(input + input_len, EMDASH_UTF8, 3);
        input_len += 3;
    }

    size_t out_len = 0;
    int result = dashem_remove(input, input_len, test_output_buf, TEST_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "Only em-dashes test failed: expected 0, got %d\n", result);
        return 0;
    }

    if (out_len != 0) {
        fprintf(stderr, "Only em-dashes test failed: expected 0 output, got %zu\n", out_len);
        return 0;
    }

    return 1;
}

/**
 * Run all tests
 */
int main(void) {
    struct {
        const char* name;
        int (*test_fn)(void);
    } tests[] = {
        {"Empty string", test_empty_string},
        {"No em-dashes", test_no_emdashes},
        {"Em-dash at start", test_emdash_at_start},
        {"Em-dash at end", test_emdash_at_end},
        {"Consecutive em-dashes", test_consecutive_emdashes},
        {"32-byte boundary", test_32byte_boundary},
        {"64-byte boundary", test_64byte_boundary},
        {"Lorem ipsum 1000x (boundary bug)", test_lorem_ipsum_1000},
        {"Unaligned positions", test_unaligned_positions},
        {"Large 2MB input", test_large_input},
        {"Buffer overflow protection", test_buffer_overflow_protection},
        {"Only em-dashes", test_only_emdashes},
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;

    printf("=== Comprehensive Correctness Test Suite ===\n\n");

    for (int i = 0; i < num_tests; i++) {
        printf("[%d/%d] %s... ", i + 1, num_tests, tests[i].name);
        fflush(stdout);

        if (tests[i].test_fn()) {
            printf("✓ PASS\n");
            passed++;
        } else {
            printf("✗ FAIL\n");
            failed++;
        }
    }

    printf("\n=== Results ===\n");
    printf("Passed: %d/%d\n", passed, num_tests);
    printf("Failed: %d/%d\n", failed, num_tests);

    return (failed == 0) ? 0 : 1;
}

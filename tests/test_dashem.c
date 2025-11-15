/**
 * @file test_dashem.c
 * @brief Unit tests for the em-dash removal library
 */

#include "dashem.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Suppress unused variable warnings in test code - assertions use the variables */
#pragma GCC diagnostic ignored "-Wunused-variable"

#define TEST_COUNT 0
#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name) printf("✗ %s\n", name)

int main(void) {
    printf("Running dash-em tests...\n");
    printf("Using implementation: %s\n", dashem_implementation_name());
    printf("Version: %s\n\n", dashem_version());

    /* Test 1: Empty string */
    {
        const char *input = "";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, 0, output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == 0);
        TEST_PASS("Empty string");
    }

    /* Test 2: String with no em-dashes */
    {
        const char *input = "Hello, world!";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == strlen(input));
        assert(memcmp(output, input, output_len) == 0);
        TEST_PASS("String without em-dashes");
    }

    /* Test 3: Single em-dash */
    {
        const char *input = "Hello—world";
        const char *expected = "Helloworld";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == strlen(expected));
        assert(memcmp(output, expected, output_len) == 0);
        TEST_PASS("Single em-dash removal");
    }

    /* Test 4: Multiple em-dashes */
    {
        const char *input = "First—second—third—fourth";
        const char *expected = "Firstsecondthirdfourth";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == strlen(expected));
        assert(memcmp(output, expected, output_len) == 0);
        TEST_PASS("Multiple em-dashes removal");
    }

    /* Test 5: Em-dashes at start and end */
    {
        const char *input = "—hello—";
        const char *expected = "hello";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == strlen(expected));
        assert(memcmp(output, expected, output_len) == 0);
        TEST_PASS("Em-dashes at boundaries");
    }

    /* Test 6: Output buffer too small */
    {
        const char *input = "Hello, world!";
        char output[5];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 5, &output_len);
        assert(result == -1);
        TEST_PASS("Output buffer overflow detection");
    }

    /* Test 7: NULL pointer check */
    {
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(NULL, 0, output, 1024, &output_len);
        assert(result == -2);
        TEST_PASS("NULL input validation");
    }

    /* Test 8: Large input with mixed content */
    {
        const char *input = "Lorem ipsum—dolor sit amet—consectetur adipiscing—elit";
        const char *expected = "Lorem ipsumdolor sit ametconsectetur adipiscingelit";
        char output[1024];
        size_t output_len = 0;
        int result = dashem_remove(input, strlen(input), output, 1024, &output_len);
        assert(result == 0);
        assert(output_len == strlen(expected));
        assert(memcmp(output, expected, output_len) == 0);
        TEST_PASS("Large input with mixed content");
    }

    /* Test 9: CPU feature detection */
    {
        uint32_t features = dashem_detect_cpu_features();
        assert(features != 0);
        TEST_PASS("CPU feature detection");
    }

    /* Test 10: Output size calculation */
    {
        size_t expected_size = dashem_output_size(1000);
        assert(expected_size == 1000);
        TEST_PASS("Output size calculation");
    }

    printf("\nAll tests passed! ✓\n");
    return 0;
}

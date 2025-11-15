/**
 * Test for dense em-dash patterns (critical bug fix verification)
 * Tests that multiple em-dashes within a single SIMD chunk are handled correctly
 */

#include "src/dashem.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_dense_emdashes() {
    printf("Testing dense em-dash patterns...\n");
    
    // Test 1: Multiple em-dashes within same 32-byte chunk
    const char *input1 = "a—b—c—d—e—";  // 5 em-dashes
    char output1[512];
    size_t output_len1;
    
    int result = dashem_remove(input1, strlen(input1), output1, sizeof(output1), &output_len1);
    assert(result == 0);
    assert(strcmp(output1, "abcde") == 0);
    printf("✓ Test 1: Multiple em-dashes in chunk\n");
    
    // Test 2: Em-dashes at chunk boundaries
    const char *input2 = "xxxxxxxxxxxxxxxx—yyyyyyyyyyyyyyyy—zzzzzzzzzzzzzzzz";
    char output2[512];
    size_t output_len2;
    
    result = dashem_remove(input2, strlen(input2), output2, sizeof(output2), &output_len2);
    assert(result == 0);
    printf("✓ Test 2: Em-dashes at chunk boundaries\n");
    
    // Test 3: Dense em-dashes (em-dash every other byte)
    const char *input3 = "a—b—c—d—e—f—g—h—i—j—k—";
    char output3[512];
    size_t output_len3;
    
    result = dashem_remove(input3, strlen(input3), output3, sizeof(output3), &output_len3);
    assert(result == 0);
    assert(strcmp(output3, "abcdefghijk") == 0);
    printf("✓ Test 3: Dense alternating em-dashes\n");
    
    // Test 4: All em-dashes (worst case)
    const char *input4 = "—————————";  // 3 em-dashes
    char output4[512];
    size_t output_len4;
    
    result = dashem_remove(input4, strlen(input4), output4, sizeof(output4), &output_len4);
    assert(result == 0);
    assert(output_len4 == 0);  // Should be empty
    printf("✓ Test 4: All em-dashes\n");
    
    // Test 5: Em-dash at very end of string
    const char *input5 = "hello—";
    char output5[512];
    size_t output_len5;
    
    result = dashem_remove(input5, strlen(input5), output5, sizeof(output5), &output_len5);
    assert(result == 0);
    assert(strcmp(output5, "hello") == 0);
    printf("✓ Test 5: Em-dash at end\n");
    
    // Test 6: 64+ bytes with multiple em-dashes
    char input6[256] = "";
    strcat(input6, "test—one—two—three—four—five—six—seven—eight—nine—ten—");
    strcat(input6, "a—b—c—d—e—f—g—h—i—j—k—l—m—n—o—p—q—r—s—t—u—v—w—x—y—z—");
    
    char output6[512];
    size_t output_len6;
    
    result = dashem_remove(input6, strlen(input6), output6, sizeof(output6), &output_len6);
    assert(result == 0);
    printf("✓ Test 6: Large string with many em-dashes\n");
    
    printf("\nAll dense em-dash tests passed!\n");
}

int main() {
    test_dense_emdashes();
    return 0;
}

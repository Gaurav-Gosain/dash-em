#!/usr/bin/env node

/**
 * @file test.js
 * @brief Basic tests for dash-em Node.js binding
 */

try {
  const dashem = require('./index.js');

  console.log('Testing dash-em Node.js binding...\n');

  // Test 1: Version
  console.log('Test 1: Version');
  const version = dashem.version();
  console.log(`✓ Version: ${version}\n`);

  // Test 2: Implementation
  console.log('Test 2: Implementation');
  const impl = dashem.implementationName();
  console.log(`✓ Implementation: ${impl}\n`);

  // Test 3: Simple removal
  console.log('Test 3: Remove single em-dash');
  const result1 = dashem.remove('Hello—world');
  console.log(`Input:  "Hello—world"`);
  console.log(`Output: "${result1}"`);
  console.assert(result1 === 'Helloworld', 'Should remove em-dash');
  console.log('✓ Pass\n');

  // Test 4: Multiple removals
  console.log('Test 4: Remove multiple em-dashes');
  const result2 = dashem.remove('First—second—third—fourth');
  console.log(`Input:  "First—second—third—fourth"`);
  console.log(`Output: "${result2}"`);
  console.assert(result2 === 'Firstsecondthirdfourth', 'Should remove all em-dashes');
  console.log('✓ Pass\n');

  // Test 5: No em-dashes
  console.log('Test 5: String without em-dashes');
  const result3 = dashem.remove('Hello, world!');
  console.log(`Input:  "Hello, world!"`);
  console.log(`Output: "${result3}"`);
  console.assert(result3 === 'Hello, world!', 'Should not modify string without em-dashes');
  console.log('✓ Pass\n');

  // Test 6: Empty string
  console.log('Test 6: Empty string');
  const result4 = dashem.remove('');
  console.log(`Input:  ""`);
  console.log(`Output: "${result4}"`);
  console.assert(result4 === '', 'Should handle empty string');
  console.log('✓ Pass\n');

  console.log('================================');
  console.log('✓ All tests passed!\n');
  process.exit(0);
} catch (error) {
  console.error('✗ Error:', error.message);
  console.error(error.stack);
  process.exit(1);
}

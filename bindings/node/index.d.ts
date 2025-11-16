/**
 * Remove em-dashes from a string
 *
 * @param input - Input string
 * @returns String with em-dashes removed
 */
export function remove(input: string): string;

/**
 * Remove em-dashes from a Buffer (zero-copy, high-performance)
 *
 * @param buffer - Input Buffer
 * @returns New Buffer with em-dashes removed
 */
export function removeBuffer(buffer: Buffer): Buffer;

/**
 * Remove em-dashes from a Buffer in-place (ultra-fast, modifies input!)
 *
 * WARNING: This modifies the input Buffer. The returned length indicates
 * how many bytes of the buffer are valid after removal.
 *
 * @param buffer - Input Buffer (will be modified)
 * @returns New length of valid data in buffer
 */
export function removeBufferInPlace(buffer: Buffer): number;

/**
 * Convenience function: Remove em-dashes from string using fast Buffer API
 *
 * This is 10-26x faster than remove() by using the Buffer API internally.
 * Recommended for performance-critical string processing.
 *
 * @param input - Input string
 * @returns String with em-dashes removed
 */
export function removeFast(input: string): string;

/**
 * Convenience function: Process multiple strings efficiently
 *
 * Processes an array of strings using the Buffer API for optimal performance.
 *
 * @param inputs - Array of input strings
 * @returns Array of strings with em-dashes removed
 */
export function removeMany(inputs: string[]): string[];

/**
 * Get library version
 *
 * @returns Version string
 */
export function version(): string;

/**
 * Get implementation name
 *
 * @returns Implementation name (e.g., "AVX2", "SSE4.2")
 */
export function implementationName(): string;

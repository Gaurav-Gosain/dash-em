/**
 * Remove em-dashes from a string
 *
 * @param input - Input string
 * @returns String with em-dashes removed
 */
export function remove(input: string): string;

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

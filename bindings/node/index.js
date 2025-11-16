/**
 * @file index.js
 * @brief Node.js wrapper for dash-em
 */

const addon = require('./build/Release/dashem.node');

/**
 * Remove em-dashes from a string
 *
 * @param {string} input - Input string
 * @returns {string} String with em-dashes removed
 */
function remove(input) {
    if (typeof input !== 'string') {
        throw new TypeError('Input must be a string');
    }
    return addon.remove(input);
}

/**
 * Remove em-dashes from a Buffer (zero-copy, high-performance)
 *
 * @param {Buffer} buffer - Input Buffer
 * @returns {Buffer} New Buffer with em-dashes removed
 */
function removeBuffer(buffer) {
    if (!Buffer.isBuffer(buffer)) {
        throw new TypeError('Input must be a Buffer');
    }
    return addon.removeBuffer(buffer);
}

/**
 * Remove em-dashes from a Buffer in-place (ultra-fast, modifies input!)
 *
 * WARNING: This modifies the input Buffer. The returned length indicates
 * how many bytes of the buffer are valid after removal.
 *
 * @param {Buffer} buffer - Input Buffer (will be modified)
 * @returns {number} New length of valid data in buffer
 */
function removeBufferInPlace(buffer) {
    if (!Buffer.isBuffer(buffer)) {
        throw new TypeError('Input must be a Buffer');
    }
    return addon.removeBufferInPlace(buffer);
}

/**
 * Convenience function: Remove em-dashes from string using fast Buffer API
 *
 * This is 10-26x faster than remove() by using the Buffer API internally.
 * Recommended for performance-critical string processing.
 *
 * @param {string} input - Input string
 * @returns {string} String with em-dashes removed
 */
function removeFast(input) {
    if (typeof input !== 'string') {
        throw new TypeError('Input must be a string');
    }
    const buffer = Buffer.from(input, 'utf-8');
    const result = addon.removeBuffer(buffer);
    return result.toString('utf-8');
}

/**
 * Convenience function: Process multiple strings efficiently
 *
 * Processes an array of strings using the Buffer API for optimal performance.
 *
 * @param {string[]} inputs - Array of input strings
 * @returns {string[]} Array of strings with em-dashes removed
 */
function removeMany(inputs) {
    if (!Array.isArray(inputs)) {
        throw new TypeError('Input must be an array');
    }
    return inputs.map(input => {
        if (typeof input !== 'string') {
            throw new TypeError('All inputs must be strings');
        }
        const buffer = Buffer.from(input, 'utf-8');
        const result = addon.removeBuffer(buffer);
        return result.toString('utf-8');
    });
}

/**
 * Get library version
 *
 * @returns {string} Version string
 */
function version() {
    return addon.version();
}

/**
 * Get implementation name
 *
 * @returns {string} Implementation name (e.g., "AVX2", "SSE4.2")
 */
function implementationName() {
    return addon.implementationName();
}

module.exports = {
    remove,
    removeBuffer,
    removeBufferInPlace,
    removeFast,
    removeMany,
    version,
    implementationName,
};

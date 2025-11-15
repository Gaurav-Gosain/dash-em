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
    version,
    implementationName,
};

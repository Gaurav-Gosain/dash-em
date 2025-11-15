<?php
/**
 * Enterprise-Grade Em-Dash Removal Library for PHP
 *
 * High-performance, SIMD-accelerated string processing for removing
 * em-dashes (U+2014) from UTF-8 encoded text.
 */

if (!extension_loaded('dashem')) {
    throw new Exception('The dashem extension is not loaded. Please install it first.');
}

/**
 * Remove em-dashes from a string
 *
 * @param string $input Input string
 * @return string String with em-dashes removed
 */
function dashem_remove($input) {
    if (!is_string($input)) {
        throw new TypeError('Input must be a string');
    }
    return dashem_remove_internal($input);
}

/**
 * Get library version
 *
 * @return string Version string
 */
function dashem_version() {
    return dashem_version_internal();
}

/**
 * Get implementation name
 *
 * @return string Implementation name
 */
function dashem_implementation_name() {
    return dashem_implementation_name_internal();
}
?>

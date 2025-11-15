/**
 * @file index.js
 * @brief WebAssembly wrapper for dash-em
 *
 * This module provides a high-level JavaScript interface to the
 * WebAssembly implementation of the em-dash removal library.
 */

let wasmModule = null;
let memoryBuffer = null;

/**
 * Initialize the WebAssembly module
 *
 * @param {ArrayBuffer|WebAssembly.Instance} wasmOrInstance - WASM binary or instance
 * @returns {Promise<void>}
 */
async function init(wasmOrInstance) {
    if (wasmOrInstance instanceof WebAssembly.Instance) {
        wasmModule = wasmOrInstance;
    } else if (wasmOrInstance instanceof ArrayBuffer) {
        const wasmModule_ = await WebAssembly.instantiate(wasmOrInstance);
        wasmModule = wasmModule_.instance;
    } else {
        throw new TypeError("Expected ArrayBuffer or WebAssembly.Instance");
    }

    memoryBuffer = new Uint8Array(wasmModule.exports.memory.buffer);
}

/**
 * Remove em-dashes from a UTF-8 string
 *
 * @param {string} input - Input string
 * @returns {string} String with em-dashes removed
 */
function remove(input) {
    if (!wasmModule) {
        throw new Error("WASM module not initialized. Call init() first.");
    }

    // Encode input string to UTF-8
    const encoder = new TextEncoder();
    const encoded = encoder.encode(input);

    // Allocate memory in WASM module
    const inputPtr = wasmModule.exports.malloc(encoded.length);
    const outputPtr = wasmModule.exports.malloc(encoded.length);

    // Copy input to WASM memory
    const wasmMem = new Uint8Array(wasmModule.exports.memory.buffer);
    wasmMem.set(encoded, inputPtr);

    // Call removal function
    const outputLenPtr = wasmModule.exports.malloc(4);
    const result = wasmModule.exports.dashem_remove(
        inputPtr,
        encoded.length,
        outputPtr,
        encoded.length,
        outputLenPtr
    );

    if (result !== 0) {
        wasmModule.exports.free(inputPtr);
        wasmModule.exports.free(outputPtr);
        wasmModule.exports.free(outputLenPtr);
        throw new Error(`dashem_remove failed with code ${result}`);
    }

    // Read output length
    const outputLenView = new Uint32Array(wasmModule.exports.memory.buffer);
    const outputLen = outputLenView[outputLenPtr / 4];

    // Read output from WASM memory
    const output = new Uint8Array(wasmModule.exports.memory.buffer, outputPtr, outputLen);

    // Decode output
    const decoder = new TextDecoder();
    const result_str = decoder.decode(new Uint8Array(output));

    // Free allocated memory
    wasmModule.exports.free(inputPtr);
    wasmModule.exports.free(outputPtr);
    wasmModule.exports.free(outputLenPtr);

    return result_str;
}

/**
 * Get library version
 *
 * @returns {string} Version string
 */
function version() {
    if (!wasmModule) {
        throw new Error("WASM module not initialized. Call init() first.");
    }

    const versionPtr = wasmModule.exports.dashem_version();
    const wasmMem = new Uint8Array(wasmModule.exports.memory.buffer);

    let i = 0;
    while (wasmMem[versionPtr + i] !== 0) {
        i++;
    }

    const decoder = new TextDecoder();
    return decoder.decode(new Uint8Array(wasmModule.exports.memory.buffer, versionPtr, i));
}

export { init, remove, version };

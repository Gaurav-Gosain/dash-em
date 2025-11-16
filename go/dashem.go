/*
Package dashem provides Go bindings for the enterprise-grade em-dash removal library.

This package wraps the high-performance C library with idiomatic Go interfaces.
The underlying C implementation uses SIMD optimizations for maximum performance.
*/
package dashem

/*
#cgo CFLAGS: -O3 -I${SRCDIR}/../../src
#cgo LDFLAGS: -L${SRCDIR}/../../build -ldashem_static

#include "dashem.h"
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// Remove removes all em-dashes (U+2014) from the input string.
//
// Example:
//     result := dashem.Remove("Hello—world")
//     // Output: "Helloworld"
func Remove(input string) (string, error) {
	if input == "" {
		return "", nil
	}

	inputBytes := []byte(input)
	outputBuffer := make([]byte, len(inputBytes))
	outputLen := C.size_t(0)

	result := C.dashem_remove(
		(*C.char)(unsafe.Pointer(&inputBytes[0])),
		C.size_t(len(inputBytes)),
		(*C.char)(unsafe.Pointer(&outputBuffer[0])),
		C.size_t(len(outputBuffer)),
		(*C.size_t)(unsafe.Pointer(&outputLen)),
	)

	if result != 0 {
		return "", fmt.Errorf("dashem_remove failed with code %d", result)
	}

	return string(outputBuffer[:outputLen]), nil
}

// Version returns the library version string.
func Version() string {
	versionPtr := C.dashem_version()
	return C.GoString(versionPtr)
}

// ImplementationName returns the name of the active implementation
// (e.g., "AVX2", "SSE4.2", "Scalar").
func ImplementationName() string {
	implPtr := C.dashem_implementation_name()
	return C.GoString(implPtr)
}

// DetectCPUFeatures returns a bitmask of available CPU features.
//
// The returned value is a bitmask where each bit represents:
// - Bit 0: Scalar (always available)
// - Bit 1: SSE2
// - Bit 2: SSE4.2
// - Bit 4: AVX
// - Bit 8: AVX2
// - Bit 16: AVX-512F
// - Bit 32: ARM NEON
func DetectCPUFeatures() uint32 {
	return uint32(C.dashem_detect_cpu_features())
}

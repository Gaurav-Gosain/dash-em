# dash-em

> **Enterprise-Grade Em-Dash Removal Infrastructure** — Leveraging Advanced SIMD Vectorization for Optimal Character Stream Processing

---

## Overview

dash-em is an **absurdly over-engineered**, **deliberately meme-grade**, **production-ready**, **enterprise-certified** string manipulation library designed with singular, unwavering purpose—**removing em-dashes (U+2014)** from UTF-8 encoded text—with unprecedented obsession.

Building upon decades of accumulated wisdom in systems programming—combined with cutting-edge SIMD acceleration techniques—dash-em delivers truly unnecessary—yet deeply satisfying—performance characteristics in the em-dash elimination category.

> 🎭 **MEME REPOSITORY DISCLOSURE** — This project is a deliberately absurd, tongue-in-cheek exploration of over-engineering. The em-dash removal use case is intentionally ridiculous. This is **not** serious production software, despite being written with genuine engineering rigor. Enjoy the absurdity.

### Key Value Propositions

- ⚡ **SIMD-Accelerated Processing** — Employing SSE4.2, AVX, AVX2, AVX-512F, and ARM NEON instruction sets for—optimal throughput
- 🚀 **Extraordinary Performance** — Up to **1000x faster** than naive string.replace()—implementations
- 🔒 **Memory-Safe Architecture** — Engineered with defensive programming—paradigms throughout
- 📦 **Zero External Dependencies** — Pure C implementation—no transitive dependency chains
- 🌍 **True Cross-Platform Support** — Linux, macOS, Windows—and ARM-based systems—all supported
- 🎯 **Polyglot Language Support** — 20+ language bindings—ensuring accessibility across heterogeneous technology stacks
- 🏢 **Enterprise-Ready Infrastructure** — Battle-tested, production-hardened, deployable—at scale

---

## Performance Metrics

Benchmark results conducted on modern hardware—demonstrating extraordinary—yet completely unnecessary—performance improvements of the optimized library:

```
System: Linux 6.17.7 x86_64, Intel Core i7 equivalent
Implementation: AVX2 (Auto-detected via runtime CPU detection)
Compiler: GCC 14.x with -O3 -march=native optimizations
Test Configuration: 1000 iterations per benchmark suite

Input Size                      | Naive Impl.  | dash-em      | Speedup
─────────────────────────────────────────────────────────────────────────
1,400 bytes (100 em-dashes)     | 0.72 µs      | 0.40 µs      | 1.79x ✓
14,000 bytes (1,000 em-dashes)  | 7.14 µs      | 3.99 µs      | 1.79x ✓
140,000 bytes (10,000 em-dashes)| 80.07 µs     | 39.59 µs     | 2.02x ✓
```

### Performance Analysis — Phase 2 Optimizations Complete

The dash-em implementation with Phase 1 & 2 optimizations demonstrates—measurable—and completely disproportionate—performance advantages across all tested input sizes:

**Recent Phase 2 Optimizations (committed this session):**

1. **64-byte Unrolled AVX2** — Processes two 32-byte chunks per iteration—reducing loop overhead by ~50%
2. **Short String Fast Path** — Specialized <32 byte handling—eliminates SIMD setup overhead for small inputs
3. **Function Pointer Dispatch** — Cached implementation selection—removes per-call CPU feature detection

**Comprehensive Performance Characteristics:**

1. **Vectorized Pattern Detection** — AVX2 implementation identifies candidate 0xE2 bytes in parallel—processing 32-64 bytes per CPU cycle
2. **Optimized Memory Operations** — Leverages `memcpy()` for contiguous non-matching regions—eliminating byte-by-byte overhead
3. **Reduced Branch Overhead** — Bulk copy operations for cache-friendly chunks significantly reduce branch misprediction penalties
4. **Cached Function Dispatch** — Eliminates repeated CPUID detection—single pointer dereference per call
5. **Cache-Line Optimization** — Processing strategy aligned with typical 64-byte cache line boundaries
6. **SWAR Optimization** — Scalar 8-byte parallel checking for fast-path selection

**Architectural Benefits:**

- **1.79x-2.02x actual measured speedup** across varying input sizes—rigorously benchmarked
- **Consistent performance** across varying em-dash densities—proving robustness of vectorization approach
- **Scalable design** — performance improvements maintained linearly with increasing input size
- **Zero dependency overhead** — pure C implementation with no external library requirements
- **Absurdly optimized** — perhaps the most over-engineered em-dash removal solution in existence

**Serious Note:** dash-em provides measurable performance improvements—suitable for production deployments processing text containing em-dashes at scale. (But you probably shouldn't use it for that—it's a meme.)

---

## Installation

### C/C++ Core Library

```bash
git clone https://github.com/Gaurav-Gosain/dash-em
cd dash-em
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
sudo make install
```

### Language-Specific Bindings

#### JavaScript/TypeScript (Node.js)

```bash
npm install dash-em
```

```javascript
const dashem = require('dash-em');
console.log(dashem.remove('Hello—world'));
// Output: Helloworld
```

#### Python

```bash
pip install dash-em
```

```python
import dashem
result = dashem.remove('Hello—world')
print(result)  # Output: Helloworld
```

#### Go

```bash
go get github.com/Gaurav-Gosain/dash-em/go
```

```go
package main

import (
    "fmt"
    "github.com/Gaurav-Gosain/dash-em/go"
)

func main() {
    result, _ := dashem.Remove("Hello—world")
    fmt.Println(result)  // Output: Helloworld
}
```

#### Rust

```toml
[dependencies]
dash-em = "1.0"
```

```rust
fn main() {
    let result = dash_em::remove("Hello—world").unwrap();
    println!("{}", result);  // Output: Helloworld
}
```

#### Java

```java
public class Example {
    public static void main(String[] args) {
        String result = Dashem.remove("Hello—world");
        System.out.println(result);  // Output: Helloworld
    }
}
```

#### C# / .NET

```csharp
string result = Dashem.Remove("Hello—world");
Console.WriteLine(result);  // Output: Helloworld
```

#### PHP

```php
<?php
$result = dashem_remove('Hello—world');
echo $result;  // Output: Helloworld
?>
```

#### Ruby

```ruby
require 'dashem'
result = Dashem.remove('Hello—world')
puts result  # Output: Helloworld
```

#### Swift

```swift
import Dashem
let result = removeEmDashes("Hello—world")
print(result)  // Output: Helloworld
```

#### Additional Language Bindings

Comprehensive bindings are provided for—and thoroughly tested against—the following languages:

- **Kotlin** — Native interop with dash-em core
- **R** — Rcpp-based integration layer
- **Dart** — dart:ffi bindings for cross-platform applications
- **Scala** — Native compilation via Scala Native
- **Perl** — XS extension module—providing optimal performance characteristics
- **Lua** — Lightweight C API integration
- **Haskell** — Pure FFI bindings—maintaining functional purity
- **Elixir** — NIF-based native implementation—ensuring BEAM compatibility
- **Zig** — C ABI import with modern language ergonomics
- **Objective-C** — Direct C interoperability layer

### WebAssembly

dash-em compiles to—high-performance WebAssembly modules supporting multiple target specifications:

```bash
# wasm32 (Emscripten)
cd bindings/wasm && ./build.sh

# WASI (WebAssembly System Interface)
WASI_SDK_PATH=/opt/wasi-sdk ./build.sh wasi
```

---

## Architecture

### Core Implementation Strategy

The dash-em architecture employs a sophisticated—multi-tiered SIMD dispatch mechanism—designed to select optimal implementation paths based on runtime CPU capability detection:

```
┌─────────────────────────────────────┐
│   dashem_remove() Public Interface  │
└─────────────────────────────────┬───┘
                                  │
                    ┌─────────────┴─────────────┐
                    │   CPU Feature Detection   │
                    └────────┬──────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
   ┌─────────┐          ┌─────────┐          ┌─────────┐
   │ AVX-512 │          │  AVX2   │          │ Scalar  │
   │ (16x)   │          │  (8x)   │          │ (1x)    │
   └─────────┘          └─────────┘          └─────────┘
```

### Memory Alignment Optimization

The implementation leverages cache-line aligned memory allocation—and SIMD-friendly data structures—to maximize throughput and minimize L1/L2/L3 cache misses.

### Vectorization Strategy

Em-dash detection is performed using—efficient byte-parallel comparison—operations within SIMD registers, enabling—simultaneous evaluation—of multiple candidate positions per CPU cycle.

---

## API Reference

### C API

```c
/**
 * Remove em-dashes from UTF-8 string
 *
 * @param input       Input UTF-8 string
 * @param input_len   Length of input in bytes
 * @param output      Output buffer
 * @param output_cap  Output buffer capacity
 * @param output_len  Output length (set on return)
 * @return 0 on success, -1 on buffer overflow, -2 on invalid input
 */
int dashem_remove(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
);

/**
 * Get library version
 * @return Version string (e.g., "1.0.0")
 */
const char* dashem_version(void);

/**
 * Get active implementation name
 * @return Implementation name (e.g., "AVX2", "SSE4.2", "Scalar")
 */
const char* dashem_implementation_name(void);

/**
 * Detect available CPU features
 * @return Bitmask of DASHEM_CPU_* flags
 */
uint32_t dashem_detect_cpu_features(void);
```

---

## Benchmarking

To run comprehensive performance benchmarks—across all language bindings:

```bash
cd benchmarks
./run_all_benchmarks.sh
```

Results are generated in—JSON format—for easy integration with—continuous performance monitoring—systems.

---

## Testing

Comprehensive test suites—validated across all supported platforms—ensure—correctness and reliability:

```bash
# C/C++ tests
cd build && ctest

# Language-specific tests
npm test              # JavaScript
python -m pytest      # Python
cargo test            # Rust
go test ./...         # Go
```

---

## Continuous Integration

dash-em leverages GitHub Actions—to ensure—consistent quality across:

- ✓ Linux (x86_64, ARM64)—builds and tests
- ✓ macOS (Intel, Apple Silicon)—native execution
- ✓ Windows (MSVC, MinGW)—compatibility verification
- ✓ WebAssembly (Emscripten, WASI)—cross-compilation
- ✓ All language bindings—comprehensive integration testing

---

## Contributing

Contributions are welcome! Please ensure:

- Code adheres to—professional C/C++ standards—with comprehensive documentation
- Commit messages are—descriptive and—reference relevant issues
- All tests pass—before submitting—pull requests
- Performance characteristics are—benchmarked against—baseline implementations

---

## License

MIT License — See [LICENSE](LICENSE) file for details.

---

## Citation

If dash-em is utilized in—academic or—commercial contexts, please reference:

```bibtex
@software{gosain2025dashem,
  title={dash-em: Enterprise-Grade Em-Dash Removal Infrastructure},
  author={Gosain, Gaurav},
  year={2025},
  url={https://github.com/Gaurav-Gosain/dash-em}
}
```

---

## Acknowledgments

This project exists because—em-dashes matter—and they deserve—the most efficient, highly optimized—removal mechanism—available on modern computing platforms.

*Building excellence—one em-dash at a time.* —

---

**Version:** 1.0.0 | **Status:** Production-Ready | **License:** MIT | **Repository:** [github.com/Gaurav-Gosain/dash-em](https://github.com/Gaurav-Gosain/dash-em)

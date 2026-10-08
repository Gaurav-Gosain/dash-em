# dash-em

> **Enterprise-Grade Em-Dash Removal Infrastructure** — SIMD-accelerated, runtime-dispatched, fuzz-tested, and still not sure why it exists

---

## Overview

dash-em removes one character from text. That character is the em-dash (U+2014, `E2 80 94` in UTF-8). It removes it very, very fast.

It does nothing else. It has no plans to do anything else. It has a roadmap anyway.

> **MEME REPOSITORY DISCLOSURE** — This project is a joke about over-engineering. The engineering is not a joke. The kernels are real, the fuzz tests are real, and the bugs we found in them were extremely real.

### Key Value Propositions

- **SIMD-Accelerated Processing** — AVX-512 VBMI2, AVX2, SSE4.2, ARM NEON, and a scalar fallback for the brave
- **Runtime Dispatch** — Build once with generic flags. The library asks the CPU what it can do and picks a kernel when it runs
- **Zero External Dependencies** — One C file. One header. One purpose. Zero chill
- **Polyglot Support** — Bindings for Python, Node.js, Go, Rust, Java, C#, PHP, Ruby and Swift. Ten more directories exist and contain nothing, which we consider a minimalist design statement
- **Enterprise-Ready** — We have a citation block, three version numbers and a bot. That is the definition of enterprise

---

## The Meta Section

Some facts about this repository. All of them are true. We checked.

- **This README contains 26 em-dashes.** dash-em removes all of them in about 330 nanoseconds on an Intel i7-10700. We keep them in on purpose. A README without em-dashes would be a product demo, and a product demo is marketing.
- **The bot has more commits than the humans.** The nightly benchmark job commits fresh numbers to this README. It has made more than 200 commits. The humans have made about 100. The bot does not take holidays.
- **The README had a memory leak.** Each nightly run added one blank line under the `## Performance` heading. After 216 runs, the section began with 216 empty lines. That is one blank line per bot commit. The leak is fixed. The bot has not been told.
- **macOS CI was red for seven months.** A NEON "optimization" removed only the first byte of some em-dashes. It left the other two (`80 94`) in the text, where they sat like the ghost of punctuation. Nobody noticed, because the CI badge was not in the README. It is fixed.
- **The AVX-512 VBMI2 kernel was labeled "REVOLUTIONARY" in a comment.** It returned the wrong output in 49% of fuzz cases on the exact CPUs it was written for. It also read past the end of your buffer. It has been replaced by a kernel with a less exciting comment and a 0% failure rate.
- **The latest speedups were written by an AI model.** AI models are the leading global supplier of em-dashes. We see no conflict of interest.
- **dash-em has three version numbers.** The CMake project says 1.0.1. The CMake package config says 1.0.0. The header and the bindings say 1.1.2. Pick the one that matches your risk tolerance.

---

## How It Works

### Runtime dispatch

The library checks the CPU once, on the first call, and caches the result. Each x86 kernel is compiled for its own instruction set, so a Python wheel or a Rust crate built for generic x86-64 still gets AVX2 or AVX-512 on a machine that has them. Before this change, those builds ran the scalar loop. They were very portable and very slow.

| Kernel | Bytes per step | Notes |
|--------|----------------|-------|
| AVX-512 VBMI2 | 64 | Packs the kept bytes with `VPCOMPRESSB`. Ice Lake and newer Intel, Zen 4 and newer AMD |
| AVX2 | 64 | Packs the kept bytes with a `pshufb` lookup table. Most x86 CPUs since 2013 |
| SSE4.2 | 32 | Same algorithm, half the width, for CPUs that remember Windows 7 |
| NEON | 16 to 64 | ARM servers and Apple Silicon |
| Scalar | 8 | SWAR bit tricks. Works on a toaster, if the toaster has a C compiler |

### The algorithm

Every x86 kernel does the same five steps for each block:

1. Compare the block, the block shifted by one byte, and the block shifted by two bytes against `E2`, `80` and `94`.
2. AND the three results. Each set bit marks the start of an em-dash.
3. Expand each start bit to cover all three bytes. Carry up to two bytes into the next block, for em-dashes that start at the end of a block.
4. Pack the kept bytes together with one shuffle (or one `VPCOMPRESSB`).
5. Store them. Move on. Never look back. Never look for an em-dash that is not there.

Blocks without an `E2` byte take a fast path that is one compare and one store. The work per block does not depend on how many em-dashes it contains, so dense text no longer falls off a cliff.

### Speedups from the rewrite

Measured on an Intel i7-10700 (AVX2), best of several runs:

| Case | Before | After |
|------|--------|-------|
| Dense (25% em-dashes) | 4.5 GB/s | 7.0 GB/s |
| Alternating | 4.4 GB/s | 7.1 GB/s |
| Sparse, in place | 1.4 GB/s | 16.5 GB/s |
| Dense, in place | 2.5 GB/s | 7.1 GB/s |
| 64-byte string | 20.6 ns | 6.6 ns |
| Python wheel, any pattern | scalar | AVX2 or better |

Text with no em-dashes was already limited by memory bandwidth. It stays at about 26 GB/s. You cannot remove em-dashes faster than you can read the text. We tried.

### How we know it works

Each kernel runs against a reference implementation for every input length up to 4,160 bytes. The tests cover out-of-place and in-place calls, random alignments, and exact-size heap buffers under AddressSanitizer and UndefinedBehaviorSanitizer. The AVX-512 kernel runs under the Intel Software Development Emulator, because the development machine does not have AVX-512 and refused to grow it. The NEON kernel runs on x86 through a plain-C model of the eight NEON intrinsics it uses.

To check the checker, we broke the carry logic on purpose. The fuzz test reported 6,917 failures. Then we fixed it again.

---

## Performance

### Core Library Performance

Multi-architecture SIMD performance (statistical benchmarks):

| Pattern | ubuntu-22.04-gcc | windows-2022-msvc | macos-14-aarch64 | ubuntu-22.04-clang |
|---------|----------|----------|----------|----------|
| sparse | 24.00 GB/s (16.41x) | 54.87 GB/s (29.80x) | 26.76 GB/s (22.88x) | 40.54 GB/s (30.01x) |
| moderate | 9.93 GB/s (7.33x) | 51.56 GB/s (33.28x) | 6.03 GB/s (5.27x) | 24.93 GB/s (17.76x) |
| dense | 6.10 GB/s (2.53x) | 38.95 GB/s (11.20x) | 1.64 GB/s (1.09x) | 23.28 GB/s (10.15x) |
| alternating | 5.83 GB/s (2.42x) | 37.33 GB/s (9.00x) | 1.46 GB/s (1.09x) | 23.09 GB/s (10.03x) |
| boundary | 18.87 GB/s (13.45x) | 67.82 GB/s (39.33x) | 7.72 GB/s (7.67x) | 37.66 GB/s (32.90x) |
| no | 28.25 GB/s (19.11x) | 53.82 GB/s (26.61x) | 46.49 GB/s (40.24x) | 39.62 GB/s (29.13x) |

### Language Bindings Performance

Comparing dash-em bindings against native byte-level implementations:

| Language | Test Pattern | Native (μs) | dash-em (μs) | Speedup |
|----------|--------------|-------------|--------------|---------|
| javascript | alternating | 54.5 | 8.38 | 6.50x |
| javascript | dense | 108.0 | 17.4 | 6.22x |
| javascript | moderate | 280.5 | 11.8 | 23.80x |
| javascript | no | 2757.4 | 64.5 | 42.77x |
| javascript | sparse | 2572.1 | 83.3 | 30.88x |
| python | alternating | 3716.6 | 7.13 | 521.54x |
| python | dense | 7867.5 | 12.4 | 634.25x |
| python | moderate | 20012.8 | 19.2 | 1041.96x |
| python | no | 206024.0 | 184.1 | 1119.09x |
| python | sparse | 206056.3 | 331.6 | 621.31x |
<!-- Performance table last updated: 2026-10-08T08:09:11.628479 -->


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

**String API** (easy to use):
```javascript
const dashem = require('dash-em');
const result = dashem.remove('Hello—world');
console.log(result);  // Output: Helloworld
```

**Buffer API** (high-performance, zero-copy):
```javascript
const dashem = require('dash-em');

// Process Buffer directly (10-26x faster than string API)
const input = Buffer.from('Hello—world', 'utf-8');
const output = dashem.removeBuffer(input);
console.log(output.toString('utf-8'));  // Output: Helloworld

// In-place modification (fastest, modifies input)
const buffer = Buffer.from('Hello—world', 'utf-8');
const newLength = dashem.removeBufferInPlace(buffer);
console.log(buffer.slice(0, newLength).toString('utf-8'));  // Output: Helloworld
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

    dashem "github.com/Gaurav-Gosain/dash-em/go"
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

Directories exist for Kotlin, R, Dart, Scala, Perl, Lua, Haskell, Elixir, Zig and Objective-C. They are empty. They have never failed a test. They have a 100% pass rate and a 0% line count, which is the best ratio in the repository.

Pull requests that put code in them are welcome. Pull requests that remove them will be considered an attack on our language support numbers.

### WebAssembly

dash-em compiles to WebAssembly, where it uses the scalar kernel and contemplates its life choices:

```bash
# wasm32 (Emscripten)
cd bindings/wasm && ./build.sh

# WASI (WebAssembly System Interface)
WASI_SDK_PATH=/opt/wasi-sdk ./build.sh wasi
```

---

## Architecture

```mermaid
graph TD
    A["dashem_remove()"] --> B{"First call?"}
    B -->|Yes| C["Ask the CPU (CPUID + XGETBV)<br/>and cache the answer"]
    B -->|No| D{"Cached kernel"}
    C --> D
    D -->|AVX-512 VBMI2| E["64-byte blocks<br/>VPCOMPRESSB"]
    D -->|AVX2| F["64-byte blocks<br/>pshufb lookup table"]
    D -->|SSE4.2| G["32-byte blocks<br/>pshufb lookup table"]
    D -->|NEON| H["16 to 64-byte blocks"]
    D -->|none of the above| I["Scalar SWAR<br/>8 bytes at a time"]
    E --> J["Text, now with fewer em-dashes"]
    F --> J
    G --> J
    H --> J
    I --> J
```

Very short inputs use the scalar code (or masked loads on AVX-512). The last partial block of a long input is processed as one more full block that ends at the end of the input, so the kernels never read past your buffer. We used to read past your buffer. We apologize to your buffer.

---

## API Reference

### C API

```c
/**
 * Remove em-dashes from UTF-8 string
 *
 * @param input       Input UTF-8 string
 * @param input_len   Length of input in bytes
 * @param output      Output buffer (may equal input for in-place use)
 * @param output_cap  Output buffer capacity (at least input_len)
 * @param output_len  Output length (set on return)
 * @return 0 on success, -1 on buffer overflow, -2 on invalid input
 *
 * Bytes of the output buffer after *output_len, up to input_len,
 * may be overwritten with scratch data.
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
 * @return Version string (one of our several version numbers)
 */
const char* dashem_version(void);

/**
 * Get active implementation name
 * @return "AVX-512 VBMI2 (VPCOMPRESSB)", "AVX2", "SSE4.2", "NEON" or "Scalar"
 */
const char* dashem_implementation_name(void);

/**
 * Detect available CPU features
 * @return Bitmask of DASHEM_CPU_* flags
 */
uint32_t dashem_detect_cpu_features(void);
```

### JavaScript/Node.js API

```javascript
/**
 * Remove em-dashes from a string
 * @param {string} input - Input string
 * @returns {string} String with em-dashes removed
 */
dashem.remove(input);

/**
 * Remove em-dashes from a Buffer (zero-copy, high-performance)
 * @param {Buffer} buffer - Input Buffer
 * @returns {Buffer} New Buffer with em-dashes removed
 */
dashem.removeBuffer(buffer);

/**
 * Remove em-dashes from a Buffer in-place (ultra-fast, modifies input!)
 * WARNING: This modifies the input Buffer
 * @param {Buffer} buffer - Input Buffer (will be modified)
 * @returns {number} New length of valid data in buffer
 */
dashem.removeBufferInPlace(buffer);

/**
 * Get library version
 * @returns {string} Version string
 */
dashem.version();

/**
 * Get implementation name
 * @returns {string} Implementation name (e.g., "AVX2", "SSE4.2")
 */
dashem.implementationName();
```

**Performance Notes:**
- `remove()`: Easy to use but includes UTF-8 conversion overhead
- `removeBuffer()`: 10-26x faster than `remove()`, zero-copy operation
- `removeBufferInPlace()`: Fastest option, modifies input buffer directly

---

## Running Benchmarks

A bot runs the benchmarks every night and commits the results to the Performance section above. Please do not edit that section by hand. The bot will overwrite your changes, and it has more commit access than you think.

To run benchmarks locally:

```bash
cd build && ./bench_statistical
```

---

## Testing

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

GitHub Actions builds and tests the library and the bindings on Linux, macOS (Apple Silicon) and Windows (MSVC). Benchmarks run on x86-64 with GCC, Clang and MSVC, and on ARM64.

The CI is green when the em-dashes are gone. When the CI is red, an em-dash has escaped. Treat it as a security incident.

---

## Contributing

Contributions are welcome. Please make sure that:

- All tests pass
- Performance claims come with a benchmark
- Comments that say "REVOLUTIONARY" come with a fuzz test

---

## License

MIT License — See [LICENSE](LICENSE) file for details.

---

## Citation

If dash-em is used in academic or commercial work, please cite it. Then please tell us why.

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

**Version:** 1.0.1 (or 1.1.2, or 1.0.0) | **Status:** Production-Ready | **License:** MIT | **Repository:** [github.com/Gaurav-Gosain/dash-em](https://github.com/Gaurav-Gosain/dash-em)

# Changelog

All notable changes to the dash-em project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.2] - 2026-03-09

### Fixed
- Fix benchmark README update script regex; fix PIPESTATUS race in release workflow

## [1.1.1] - 2026-03-09

### Fixed
- Version bump for clean release — no code changes from 1.1.0

## [1.1.0] - 2026-03-09

### Performance — The One Where We Made It Faster

The em-dash removal engine — already absurdly over-engineered — has been turbocharged with a ground-up rewrite of the AVX2 hot path. Every pattern — sparse, dense, pathological — is now faster. Yes, even the "100% em-dashes" case — which was previously *slower* than naive — now beats it.

#### Key Results (i7-10700)

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Average throughput | 7.9 GB/s | 13.1 GB/s | **+65%** |
| Best throughput | 14.9 GB/s | 36.1 GB/s | **+143%** |
| Worst speedup vs naive | 0.80x | 1.15x | **Never slower than naive** |
| Dense em-dash patterns | 2.7 GB/s | 5.3 GB/s | **+97%** |

#### What Changed

- **Early-out on no 0xE2 bytes** — The fast path now checks for the first byte of em-dash (0xE2) before doing expensive overlapped loads. If none found — which is the common case — we store 32 bytes with a single SIMD instruction and move on. This alone delivers a ~74% speedup on the no-em-dash path.
- **Mask-expansion dense path** — For chunks with 3+ em-dash matches, we now expand the match bitmask to cover all 3 bytes of each em-dash, invert it to get a "keep" mask, and extract kept bytes via CTZ bit iteration. This replaces the old byte-by-byte fallback — which was doing 32 iterations per chunk — with ~8 iterations. Dense patterns improved by 63–99%.
- **Removed density heuristics** — The old code had popcount-based density detection and special-case branches for "exactly 8 0xE2 bytes" — adding overhead to *every* chunk even when there were no em-dashes at all. Gone.
- **SSE4.2 early-out** — Same 0xE2 pre-check optimization applied to the SSE4.2 path — avoiding 2 redundant loads on the common path.
- **Scalar SWAR skip-to-first** — When the SWAR check finds an 0xE2 byte in an 8-byte chunk, we now jump directly to its position via `ctzll` instead of processing one byte at a time.
- **In-situ SWAR fast-skip** — The in-place removal path now uses SWAR to scan 8 bytes at a time while read/write positions are still synchronized — skipping over safe regions without touching memory.
- **Portable `dashem_ctzll`** — Added MSVC-compatible 64-bit count-trailing-zeros using `_BitScanForward64` — because even em-dash removal deserves cross-platform excellence.

### Fixed
- MSVC build failure from `__builtin_ctzll` usage in scalar/in-situ paths
- Synced all vendored source copies across Go, Rust, and Python bindings

## [1.0.0] - 2025-11-15

### Added
- Initial release of dash-em: Enterprise-Grade Em-Dash Removal Library
- SIMD-accelerated C library with automatic CPU feature detection:
  - AVX2 implementation (32-byte vectorized processing)
  - SSE4.2 implementation (16-byte vectorized processing)
  - NEON implementation for ARM64
  - Scalar fallback for maximum compatibility
- Language bindings for 20+ languages:
  - **Node.js** (18.x, 20.x, 24.x) with N-API native addon
  - **Python** (3.10+) with modern C extension and pyproject.toml
  - **Rust** (2021 edition) with safe FFI wrapper
  - **Go** (1.24+) with cgo bindings
  - **Java** (17+) with JNI bindings
  - **WebAssembly** (wasm32, wasm64, wasi targets)
- Comprehensive test suites for all bindings
- Nix shells for reproducible development environments
- GitHub Actions CI/CD with multi-platform, multi-version testing:
  - C/C++ on Linux, macOS, Windows
  - Node.js on 3 platforms × 3 versions
  - Python on 3 platforms × 4 versions
  - Rust on 3 platforms
  - Go on 3 platforms × 3 versions
  - Java on 3 platforms × 2 versions
  - Security scanning with Trivy
- Trusted publishing to npm, PyPI, and crates.io
- Comprehensive documentation and benchmarks

### Performance
- **1.78x-1.87x** speedup over naive UTF-8 string processing
- Benchmarks on 100-10,000 em-dashes validate performance claims
- SIMD vectorization reduces branch misprediction overhead
- Portable baseline optimizations (x86-64, ARM64) ensure binary compatibility

### Documentation
- Professional README with architecture overview
- Extensive em-dash usage throughout (meme value: maximum)
- Installation instructions for all supported languages
- Development guide with Nix setup
- PUBLISHING.md for release automation

### Build System
- CMake-based C library build (Linux, macOS, Windows)
- Language-specific build configurations:
  - Node.js: node-gyp v12.0.0
  - Python: modern setuptools with pyproject.toml (PEP 517/518)
  - Rust: cargo with portable SIMD compilation
  - Go: standard go modules
  - Java: javac with platform compatibility

### Code Quality
- No compiler warnings on all platforms
- Safe pointer handling with bounds checking
- UTF-8 validation and error handling
- Unit tests for edge cases (empty strings, no em-dashes, etc.)
- Integration tests across all language bindings

## Future Plans
- Additional language bindings (PHP, Ruby, Swift, Kotlin, etc.)
- Performance improvements through fine-tuning SIMD parameters
- Extended Unicode support (en-dash, etc.)
- WebAssembly optimization for browser environments
- Async/parallel processing variants

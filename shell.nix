{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  description = "dash-em development environment";

  buildInputs = with pkgs; [
    # C/C++ build tools
    cmake
    gcc
    gnumake
    pkg-config

    # Node.js 24.x for bindings
    nodejs_24
    node2nix

    # Python 3.13 for bindings
    python313
    python313Packages.build
    python313Packages.setuptools
    python313Packages.wheel

    # Rust for bindings
    rustc
    cargo
    rustfmt
    clippy

    # Go 1.25 for bindings
    go_1_25

    # Java 21 for bindings
    jdk21

    # Benchmarking and performance tools
    hyperfine

    # Git
    git

    # General utilities
    curl
    wget
    which
  ];

  shellHook = ''
    echo "✓ dash-em development environment loaded"
    echo ""
    echo "Available tools:"
    echo "  C/C++:  gcc, cmake, make"
    echo "  Node.js: node $(node --version), npm"
    echo "  Python: python $(python3 --version), pip"
    echo "  Rust:   rustc $(rustc --version), cargo"
    echo "  Go:     go $(go version | cut -d' ' -f3)"
    echo "  Java:   javac $(javac -version 2>&1 | head -1)"
    echo ""
    echo "Recommended workflow:"
    echo "  1. nix-shell (loads this environment)"
    echo "  2. mkdir build && cd build"
    echo "  3. cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  4. make && ctest"
    echo "  5. Test individual bindings with language-specific shells"
  '';
}

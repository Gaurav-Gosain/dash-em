{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  description = "dash-em Rust binding development environment";

  buildInputs = with pkgs; [
    # C/C++ build tools (needed for linking to C library)
    cmake
    gcc
    gnumake
    pkg-config

    # Rust stable
    rustc
    cargo
    rustfmt
    clippy
  ];

  shellHook = ''
    echo "✓ Rust binding environment loaded"
    echo ""
    echo "$(rustc --version)"
    echo "$(cargo --version)"
    echo ""
    echo "Build commands:"
    echo "  cargo build --release       # Build the library"
    echo "  cargo test --release        # Run tests"
    echo "  cargo run --example simple  # Run the simple example"
    echo "  cargo publish               # Publish to crates.io"
    echo "  cargo doc --open            # Generate and view docs"
  '';
}

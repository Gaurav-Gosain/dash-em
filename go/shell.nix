{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  description = "dash-em Go binding development environment";

  buildInputs = with pkgs; [
    # C/C++ build tools (needed for cgo linking)
    cmake
    gcc
    gnumake
    pkg-config

    # Go 1.25
    go_1_25
  ];

  shellHook = ''
    echo "✓ Go binding environment loaded"
    echo ""
    echo "$(go version)"
    echo ""
    echo "Build commands:"
    echo "  go mod tidy        # Update dependencies"
    echo "  go build           # Build the library"
    echo "  go test ./...      # Run tests"
    echo "  go test -bench .   # Run benchmarks"
    echo "  go vet ./...       # Run static analysis"
  '';
}

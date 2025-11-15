{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  description = "dash-em Node.js binding development environment";

  buildInputs = with pkgs; [
    # C/C++ build tools (needed for node-gyp)
    cmake
    gcc
    gnumake
    pkg-config
    python3

    # Node.js 24.x LTS
    nodejs_24
    node2nix
  ];

  shellHook = ''
    echo "✓ Node.js binding environment loaded"
    echo ""
    echo "node: $(node --version)"
    echo "npm:  $(npm --version)"
    echo ""
    echo "Build commands:"
    echo "  npm install        # Install dependencies"
    echo "  npm run build      # Build the native addon"
    echo "  npm test           # Run tests"
    echo "  npm run clean      # Clean build artifacts"
  '';
}

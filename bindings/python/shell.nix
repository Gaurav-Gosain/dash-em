{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  description = "dash-em Python binding development environment";

  buildInputs = with pkgs; [
    # C/C++ build tools (needed for compiling C extensions)
    cmake
    gcc
    gnumake
    pkg-config

    # Python 3.13 with build tools
    python313
    python313Packages.build
    python313Packages.setuptools
    python313Packages.wheel
    python313Packages.pip
    python313Packages.twine
  ];

  shellHook = ''
    echo "✓ Python binding environment loaded"
    echo ""
    echo "python: $(python3 --version)"
    echo "pip:    $(pip --version)"
    echo ""
    echo "Build commands:"
    echo "  python -m build    # Build wheel and sdist"
    echo "  pip install -e .   # Install in development mode"
    echo "  python test_dashem.py  # Run tests"
    echo "  twine upload dist/ # Upload to PyPI (requires credentials)"
  '';
}

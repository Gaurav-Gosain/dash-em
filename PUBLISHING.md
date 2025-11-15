# Publishing & Deployment Guide

This document describes how to publish dash-em to package registries (npm, PyPI, crates.io) and manage releases.

## Overview

dash-em uses **Trusted Publishing** (OIDC) for all package managers, eliminating the need for long-lived API tokens. This approach is more secure and recommended by all major Python, JavaScript, and Rust package registries.

## Version Numbering

Follow [Semantic Versioning](https://semver.org/):
- **MAJOR.MINOR.PATCH** (e.g., 1.0.0)
- MAJOR: Breaking changes
- MINOR: New features (backward compatible)
- PATCH: Bug fixes only

## Pre-Release Checklist

Before publishing any version:

- [ ] Update version numbers in all packages
- [ ] Update CHANGELOG.md with new features/fixes
- [ ] Run full test suite: `npm test`, `pytest`, `cargo test`, `go test`
- [ ] Build all bindings successfully
- [ ] Run benchmarks and verify performance hasn't regressed
- [ ] Verify all CI/CD jobs pass on GitHub Actions
- [ ] Create a git commit with version bump
- [ ] Tag the commit with semantic version (v1.0.0)

## Update Version Numbers

Version must be synchronized across all packages:

### 1. Core C Library (CMakeLists.txt)
```bash
# Update in bindings/*/setup.py, Cargo.toml, package.json, etc.
VERSION="1.0.1"
```

### 2. Node.js Package (bindings/node/package.json)
```json
{
  "version": "1.0.1"
}
```

### 3. Python Package (bindings/python/pyproject.toml)
```toml
[project]
version = "1.0.1"
```

### 4. Rust Package (bindings/rust/Cargo.toml)
```toml
[package]
version = "1.0.1"
```

### 5. Go Package (via git tag only)
No version file needed—version is determined by git tags.

## Publishing Process

### Step 1: Create Git Commit and Tag

```bash
# Ensure you're on main branch and everything is committed
git status

# Create version bump commit
git add -A
git commit -m "chore: Bump version to 1.0.1

Release notes:
- Add feature X
- Fix bug Y
- Improve performance by Z%"

# Create and push git tag (triggers GitHub Actions)
git tag v1.0.1
git push origin main
git push origin v1.0.1
```

The git tag **MUST** be in format `v<VERSION>` (e.g., `v1.0.1`).

### Step 2: GitHub Actions Automatic Publishing

Once the tag is pushed, GitHub Actions workflows automatically:

1. **Create GitHub Release** with auto-generated release notes
2. **Publish to npm** (bindings/node → npmjs.com)
3. **Publish to PyPI** (bindings/python → pypi.org)
4. **Publish to crates.io** (bindings/rust → crates.io)
5. **Make Go module available** (via git tag)

No manual action needed! GitHub Actions handles everything.

## Setup Guide (One-Time Configuration)

### npm (Node.js)

#### Initial Setup
1. Create package on [npmjs.com](https://npmjs.com)
2. Ensure your GitHub account is linked to npm account
3. No configuration needed in GitHub Actions (automatic OIDC)

#### First Manual Publish (optional)
```bash
cd bindings/node
npm publish
```

### PyPI (Python)

#### Initial Setup
1. Create package on [pypi.org](https://pypi.org) (or it will auto-create on first push)
2. Go to **Account Settings → Publishing**
3. Add trusted publisher:
   - Repository: `Gaurav-Gosain/dash-em`
   - Workflow filename: `release.yml`
   - Environment: (leave blank)

#### First Manual Publish (optional)
```bash
cd bindings/python
python -m build
pip install twine
twine upload dist/*
```

### crates.io (Rust)

#### Initial Setup
1. Create account on [crates.io](https://crates.io)
2. First publish **must be manual** (to register package):
   ```bash
   cd bindings/rust
   # Get token from https://crates.io/settings/tokens
   cargo publish --token <YOUR_CRATES_IO_TOKEN>
   ```
3. After first publish, link GitHub on [crates.io](https://crates.io/settings/tokens)
4. Enable trusted publishing for the repository

#### Subsequent Publishes
Automatic via GitHub Actions (OIDC).

### Go (Module)

No setup needed! Go modules are automatically available via git tags:

```bash
go get github.com/Gaurav-Gosain/dash-em/go@v1.0.1
```

## Workflow Files

The automated publishing is configured in:
- `.github/workflows/release.yml` - Main release workflow
- `.github/workflows/build.yml` - Testing and validation

## Manual Publishing (Emergency Only)

If GitHub Actions fails, you can publish manually:

### npm
```bash
cd bindings/node
npm publish --provenance
```

### PyPI
```bash
cd bindings/python
python -m build
pip install twine
twine upload dist/*
```

### crates.io
```bash
cd bindings/rust
cargo publish
```

## Verification

After publishing, verify packages are available:

### npm
```bash
npm view dash-em@1.0.1
npm install dash-em@1.0.1
```

### PyPI
```bash
pip search dash-em  # or visit pypi.org
pip install dash-em==1.0.1
```

### crates.io
```bash
cargo search dash-em
# Or add to Cargo.toml: dash-em = "1.0.1"
```

### Go
```bash
go get github.com/Gaurav-Gosain/dash-em/go@v1.0.1
```

## Troubleshooting

### GitHub Actions Failed
1. Check workflow logs at `https://github.com/Gaurav-Gosain/dash-em/actions`
2. Most common issues:
   - Tag format incorrect (must be `v<VERSION>`)
   - Version numbers not synchronized
   - Missing trusted publisher configuration
   - Test failures preventing build

### Package Not Appearing

**npm**: Check `npm info dash-em`
**PyPI**: Visit `pypi.org/project/dash-em/`
**crates.io**: Check crates search

Allow 5-10 minutes for package registries to index.

## Rollback Procedure

If a bad version is published:

1. **Unpublish from registries** (if possible):
   - npm: `npm unpublish dash-em@1.0.1` (within 72 hours)
   - PyPI: Yank version on pypi.org settings
   - crates.io: Yank version on crates.io settings

2. **Delete git tag**:
   ```bash
   git tag -d v1.0.1
   git push origin --delete v1.0.1
   ```

3. **Fix the issue and re-release**:
   ```bash
   git tag v1.0.1-rc.1  # or bump to v1.0.2
   git push origin v1.0.1-rc.1
   ```

## Release Cadence

Recommended practices:
- **Critical security fixes**: PATCH release (1.0.1) immediately
- **Bug fixes**: Bundle into next scheduled release or PATCH
- **Features**: MINOR release (1.1.0) when ready
- **Major breaking changes**: MAJOR release (2.0.0) with migration guide

Current release: **1.0.0** (Initial Release)

## Questions?

For detailed information:
- GitHub Actions docs: https://docs.github.com/en/actions
- OIDC trusted publishing: https://docs.github.com/en/actions/deployment/security-hardening-your-deployments
- npm: https://docs.npmjs.com/
- PyPI: https://packaging.python.org/
- crates.io: https://doc.rust-lang.org/cargo/
- Go modules: https://go.dev/ref/mod

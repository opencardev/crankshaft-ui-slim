# crankshaft-ui-slim Documentation

## Overview

`crankshaft-ui-slim` is the slim UI frontend for Crankshaft, implemented with Qt 6 and QML.
It provides the runtime projection view, connection UX, settings UI, and integration with core services.

## Repository Layout

- `src/`: C++ application logic, QML UI, tests, translations, and packaging files
- `deps/`: OS package manifests consumed by `build.sh --install-deps`
- `.github/workflows/`: CI pipeline definitions
- `build.sh`: Main build/test/package entrypoint used locally and in CI

## Build and Test

### Common Commands

Install host dependencies from distro-aware manifests:

```bash
./build.sh --install-deps
```

Build (default `Release`):

```bash
./build.sh
```

Clean build:

```bash
./build.sh --clean
```

Build with tests disabled:

```bash
BUILD_TESTS=OFF ./build.sh --clean
```

Build and package artifacts (`.deb`, `.tgz`):

```bash
BUILD_PACKAGE=ON ./build.sh --clean
```

Run code quality and format checks:

```bash
CODE_QUALITY=ON FORMAT_CHECK=ON BUILD_TESTS=OFF ./build.sh --clean
```

Coverage build:

```bash
ENABLE_COVERAGE=ON BUILD_TESTS=ON ./build.sh --clean
```

Build and generate SBOMs from source + package artifacts:

```bash
BUILD_PACKAGE=ON BUILD_SBOM=ON ./build.sh --clean
```

Generate SBOMs only from an existing build directory:

```bash
BUILD_DIR=build-release BUILD_SBOM=ON ./build.sh --sbom-only
```

## Versioning

CI resolves version values from git tags, sanitizes them to SemVer, and passes them to `build.sh`.
`build.sh` forwards that version to CMake (`CRANKSHAFT_VERSION`), which is used by CPack for package metadata.

## CI Summary

The CI pipeline includes:

- Unified gate job for quality + coverage + quick packaging on `ubuntu-24.04`
- Full multiarch build + test matrix for `push`/`workflow_dispatch`
- Coverage summary published directly in the workflow run page
- Build artifacts (`.deb`, `.tgz`) and per-build SBOM artifacts
- Release metadata generation (changelog + release notes + downloaded SBOM artifacts)

## Additional Project Policies

- [Contributing Guide](CONTRIBUTING.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)

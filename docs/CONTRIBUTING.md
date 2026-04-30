# Contributing to crankshaft-ui-slim

Thank you for contributing.

## Ground Rules

- Be respectful and constructive in all discussions.
- Keep changes focused and easy to review.
- Prefer small pull requests over very large ones.
- Follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Development Setup

1. Clone the repository.
2. Install required dependencies (Qt 6, CMake, Ninja, and related dev tools).
3. Make `build.sh` executable if needed:

```bash
chmod +x ./build.sh
```

4. Run a clean build:

```bash
./build.sh --clean
```

## Branch and Commit Guidelines

- Create feature branches from `main`.
- Use clear branch names, for example:
  - `feature/settings-ui`
  - `fix/qml-runtime-crash`
- Write commit messages in imperative style:
  - `fix: include SegmentedOptionButton in qml resources`
  - `ci: add semver tag sanitization`

## Code Quality Expectations

Before opening a pull request, run:

```bash
CODE_QUALITY=ON FORMAT_CHECK=ON ./build.sh --clean
```

Also run tests locally when possible:

```bash
BUILD_TESTS=ON ./build.sh --clean
```

## Pull Request Checklist

- [ ] Build passes locally
- [ ] Tests pass locally (or explanation provided)
- [ ] Formatting/lint checks pass
- [ ] New behavior is documented where relevant
- [ ] PR description explains what changed and why

## Reporting Issues

When reporting a bug, include:

- Steps to reproduce
- Expected behavior
- Actual behavior
- Logs or screenshots when helpful
- Environment details (OS, architecture, Qt version)

## Documentation

Keep documentation under `docs/` (except root `README.md`, which is intentionally minimal).

# Contributing to Nexa

Thank you for helping improve Nexa.

## Before you start

- Search existing issues before opening a new one.
- Use a bug report for reproducible defects.
- Use a feature request for product proposals.
- Do not include passwords, private keys, real server addresses or sensitive logs.
- Keep pull requests focused on one change.

## Development setup

Nexa currently targets Windows with MSYS2 UCRT64.

Required packages:

- GCC with C++20 support
- CMake
- Ninja
- Qt 6 Base
- libssh
- pkg-config

Build and test:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Code style

- Use C++20.
- Prefer Qt value types at UI and signal boundaries.
- Keep network and blocking filesystem work outside the UI thread.
- Use RAII for native, SSH and SFTP resources.
- Do not add secrets, machine-specific paths or generated binaries.
- Match the style of the surrounding file.
- Add or update tests for behavior changes.

## Commit messages

Use short imperative summaries:

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
Improve transfer progress throttling
```

## Pull requests

A pull request should include:

- the problem being solved;
- the implementation approach;
- testing performed;
- screenshots for visible UI changes;
- documentation updates where applicable.

By contributing, you agree that your contribution is licensed under
GPL-3.0-or-later.

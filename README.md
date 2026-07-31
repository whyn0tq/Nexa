<p align="center">
  <img src="docs/assets/nexa-banner.png" alt="Nexa — open-source SSH and SFTP workspace" width="100%">
</p>

<p align="center">
  <a href="README.ru.md">Русский</a> · <strong>English</strong>
</p>

<p align="center">
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-5b5cf0?style=flat-square">
  <img alt="Qt 6" src="https://img.shields.io/badge/Qt-6-41cd52?style=flat-square">
  <img alt="Windows" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4?style=flat-square">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0--or--later-7c83ff?style=flat-square">
  <img alt="Status" src="https://img.shields.io/badge/status-preview-f59e0b?style=flat-square">
</p>

# Nexa

Nexa is a modern, local-first SSH and SFTP workspace for Windows. It combines a
two-pane file manager, secure credential storage, automatic reconnection and
high-throughput asynchronous transfers in one native desktop application.

> **Preview notice:** Nexa is under active development. Keep independent backups
> when working with important servers.

## Why Nexa?

- **Local-first:** no mandatory account or cloud service.
- **Private:** passwords and passphrases are protected with Windows DPAPI.
- **Fast:** pipelined directory listing and up to 64 in-flight SFTP requests.
- **Practical:** drag files, directories and even items from an open ZIP archive.
- **Resilient:** reconnects automatically when a hosting provider closes SFTP.
- **Open:** licensed under GPL-3.0-or-later.

## Features

### Connections and security

- Password and private-key authentication
- SHA-256 server host-key verification
- Detection of changed host keys
- Encrypted saved credentials using Windows DPAPI
- Session restoration and automatic reconnection
- Restoration of the last remote directory

### File management

- Native two-pane local/remote file browser
- Upload and download files or complete directory trees
- Create, rename and recursively delete remote entries
- Keyboard shortcuts and context menus
- Directory caching and background prefetch
- Drag and drop in both directions
- Native Windows Explorer `CF_HDROP` support
- Virtual file extraction when dragging directly from ZIP archives

### Transfer engine

- Asynchronous SFTP reads and writes
- Up to 64 in-flight requests
- Progress, transferred size and average throughput
- Retry after connection loss
- UI updates throttled to keep large transfers responsive

## Quick start

1. Download the latest portable archive from [GitHub Releases](../../releases).
2. Extract it to a regular directory.
3. Run `Nexa.exe`.
4. Add an SFTP host and verify its SSH fingerprint.

Do not run Nexa as administrator unless required. Windows can block drag and drop
from a non-elevated Explorer process into an elevated application.

## Build from source

### Requirements

- Windows 10 or 11 x64
- CMake 3.24+
- Ninja
- C++20 compiler
- Qt 6.5+ (`Core`, `Widgets`, `Network`)
- libssh 0.12+
- pkg-config

MSYS2 UCRT64 is the recommended environment.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Create a portable Windows directory:

```powershell
.\scripts\build.ps1 -OutputDirectory dist-release
```

The result is written to `dist-release\Nexa.exe`.

## Project structure

```text
.
├── .github/           GitHub workflows and templates
├── docs/              Documentation and media
├── scripts/           Build and packaging scripts
├── src/               Application source code
├── tests/             Automated tests
├── CMakeLists.txt
├── README.md
└── VERSION.md
```

## Roadmap

- [ ] Persistent transfer queue
- [ ] Multiple simultaneous file transfers
- [ ] Pause, resume and partial-transfer recovery
- [ ] SSH terminal with tabs
- [ ] Host groups, tags and search
- [ ] Port forwarding and jump hosts
- [ ] Snippets and automation
- [ ] Encrypted user-controlled synchronization
- [ ] Windows installer and automatic updates
- [ ] Linux and macOS support

See [VERSION.md](VERSION.md) for the detailed release history.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before
opening a pull request. For bugs and feature requests, use the repository issue
templates.

## Security

Do not post credentials, private keys, host addresses or sensitive logs in public
issues. See [SECURITY.md](SECURITY.md) for responsible reporting instructions.

## License

Nexa is distributed under the
[GNU General Public License v3.0 or later](LICENSE).

Qt and libssh are dynamically linked and remain covered by their respective
licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

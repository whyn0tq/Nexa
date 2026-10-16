<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="VERSION.ru.md">Русский</a>
</p>

<p align="center"><sub>
  <a href="README.md">Overview</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <strong>Version</strong> ·
  <a href="CONTRIBUTING.md">Contributing</a> ·
  <a href="SECURITY.md">Security</a> ·
  <a href="CODE_OF_CONDUCT.md">Code of Conduct</a> ·
  <a href="THIRD_PARTY_NOTICES.md">Third-party notices</a>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Nexa version

## Current release

| | |
|---|---|
| **Version** | **1.0.0** |
| Channel | Stable |
| Date | October 15, 2026 |
| Platform | Windows 10/11 x64 |
| Language | C++20 |
| Interface | Qt 6 Widgets |
| SFTP/SSH | own pipelined SFTP v3 client on top of libssh 0.12 |
| FTP/FTPS | Qt 6 Network, TLS through OpenSSL |
| Localization | English (US), Russian |

> [!IMPORTANT]
> The version number is written in several places that must change together:
>
> - `project(Nexa VERSION 1.0.0 ...)` in `CMakeLists.txt`;
> - `QApplication::setApplicationVersion("1.0.0")` in `src/main.cpp`;
> - the version badge in `README.md` and `README.ru.md`;
> - the table above in `VERSION.md` and `VERSION.ru.md`.

The full history is in [CHANGELOG.md](CHANGELOG.md).

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## What is in 1.0.0

### <img src="docs/assets/brand/icons/sliders.svg" width="22" height="22" alt=""> Interface

- new design: onyx black, frosted glass panels, a red accent and medium
  rounding on every element;
- own title bar instead of the system one: session tabs live right in it, with
  own window buttons, while resizing, Windows 11 Snap layouts, the system
  shadow and rounded corners are kept;
- no sidebars: a start page with quick connect and server cards, the two file
  panes of a session and the transfer dock at the bottom;
- command palette `Ctrl+K`, in-window sheets over a blurred backdrop and
  notifications in the corner;
- interface in English and Russian, switched in Settings;
- new app icon.

### <img src="docs/assets/brand/icons/server.svg" width="22" height="22" alt=""> Sessions

- several servers at once, each in a tab with its own connections;
- saved servers with names and color labels, passwords encrypted with DPAPI;
  the old "recent connections" are migrated automatically;
- open tabs are restored on start in the same folder.

### <img src="docs/assets/brand/icons/bolt.svg" width="22" height="22" alt=""> Speed

- own SFTP client: up to 64 files and 24 MB of requests in flight per
  connection, pipelined folder creation, tree walks and deletes;
- 1–16 parallel connections per session (4 by default), big files are split
  into segments;
- a larger socket send buffer and AES-GCM preferred.

### <img src="docs/assets/brand/icons/plug.svg" width="22" height="22" alt=""> Reliability

- an idle session is pinged every 15 seconds, and TCP keep-alive is tuned;
- endless reconnection with a growing pause once the first login has
  succeeded; transfers reconnect by themselves and resume files where they
  broke off.

### <img src="docs/assets/brand/icons/check.svg" width="22" height="22" alt=""> Testing

- unit tests for the resume rules and the SFTP codec;
- the `NexaTransferBench` rig: OpenSSH 9.6 in WSL, 50 ms latency through
  `tc netem`, 3,000 small files and a 200 MB file, hashes checked after a round
  trip, and the connection cut in the middle of a 200 MB upload with a
  successful resume.

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## Versioning rules

The project uses Semantic Versioning: `MAJOR.MINOR.PATCH`. Pre-releases use the
`-alpha.1`, `-beta.1` and `-rc.1` suffixes.

## <img src="docs/assets/brand/icons/terminal.svg" width="28" height="28" alt=""> Release procedure

1. Update the version in every place listed above.
2. Move the changes from `Unreleased` in `CHANGELOG.md` and `CHANGELOG.ru.md`.
3. Make a clean Release build without warnings and run the tests.
4. Run `NexaTransferBench` against a test server with added latency.
5. Check connecting, several tabs, reconnection after a dropped link, and
   dragging from Explorer and from a ZIP.
6. If the interface changed, regenerate the screenshots in both languages
   (`--demo-shots`); if the numbers or artwork texts changed, run
   `node scripts\brand-assets.mjs`.
7. Build the portable folder: `.\scripts\build.ps1 -OutputDirectory dist-release`.
8. Add a Git tag such as `v1.0.0`.

## <img src="docs/assets/brand/icons/forward.svg" width="28" height="28" alt=""> Plan for 1.1

- [ ] SSH terminal in a tab
- [ ] Transfer queue that survives a restart
- [ ] Pausing transfers
- [ ] Port forwarding and jump hosts
- [ ] Installer and automatic updates

<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="CHANGELOG.ru.md">Русский</a> · <a href="CHANGELOG.de.md">Deutsch</a>
</p>

<p align="center"><sub>
  <a href="README.md">Overview</a> ·
  <strong>Changelog</strong> ·
  <a href="VERSION.md">Version</a> ·
  <a href="CONTRIBUTING.md">Contributing</a> ·
  <a href="SECURITY.md">Security</a> ·
  <a href="CODE_OF_CONDUCT.md">Code of Conduct</a> ·
  <a href="THIRD_PARTY_NOTICES.md">Third-party notices</a>
</sub></p>

# <img src="docs/assets/brand/icons/refresh.svg" width="32" height="32" alt=""> Changelog

Notable changes to Nexa are collected here. Versions follow
[semantic versioning](https://semver.org/).

| Version | Date | In short |
|---|---|---|
| [**1.2.1**](#121---2026-10-27) | 2026-10-27 | Faster transfers of large files |
| [1.2.0](#120---2026-10-24) | 2026-10-24 | New look and icon, documentation in German |
| [1.1.0](#110---2026-10-21) | 2026-10-21 | German, files from a ZIP |
| [1.0.0](#100---2026-10-15) | 2026-10-15 | Tabs and the new transfer engine |
| [0.3.0](#030---2026-09-02) | 2026-09-02 | FTP and FTPS |
| [0.2.0](#020---2026-08-01) | 2026-08-01 | Transfer queue and resume |
| [0.1.0](#010---2026-07-29) | 2026-07-29 | First two-pane SFTP client |

## [1.2.1] - 2026-10-27

### Changed

- Large files move faster over SFTP: a file from 2 MB is split across all
  transfer connections at once.
- Transfers use 2 connections by default.

### Fixed

- Uploading a large file over SFTP no longer sits still at the start and then
  jumps to done. Progress moves steadily, and Cancel works right away.

## [1.2.0] - 2026-10-24

> [!IMPORTANT]
> New look. A new icon, and the documentation now comes in German too.

### Changed

- New look.
- New app icon.
- New icons.
- The `+` next to the tabs opens the New server window.

### Fixed

- The background behind an open window no longer stretches when you maximize
  Nexa.
- When Nexa is small, windows like New server can be scrolled, and their fields
  no longer squeeze together.

## [1.1.0] - 2026-10-21

### Added

- German interface: pick it in Settings or run `Nexa.exe --language de`.
- `Nexa.exe` carries the product name and version.
- `scripts/package.ps1` builds the release archive with the license texts of all
  libraries.
- A `v*` tag starts a build and a draft release in GitHub Actions.

### Changed

- When you pick another language, Nexa asks right away whether to restart or
  cancel.

### Fixed

- Files dragged out of a ZIP in Explorer upload to the server again.
- When a server drops the connection before sign-in, Nexa explains it in plain
  words instead of `SSH_MSG_DISCONNECT: 11`.

## [1.0.0] - 2026-10-15

### Added

- Several servers at once: each gets its own tab and its own connections. Open
  tabs come back at launch.
- Saved servers with color labels, a home page with quick connect and a command
  palette (`Ctrl` + `K`).
- A shared transfer dock with progress, speed, retry and cancel.
- A rule for existing files: resume, replace, skip or rename.
- Drag and drop of files anywhere in the window.
- Open and edit server files with `F4`, with automatic upload back.
- Ping for every session.
- English and Russian interface.
- `Nexa.exe --demo-shots` for screenshots, and speed benchmarks in `tests/`.

### Changed

- New SFTP client: requests go out in batches instead of one at a time.
  Transfers use 1–16 connections, and big files are split into parts.
- Faster sign-in: the first folder listing arrives 0.44 s after sign-in at 200 ms
  latency.
- New look: own window title bar, new icon.

### Fixed

- `Delete` removed the selection on the server even when it was in the local
  pane.
- Sessions no longer drop when idle. The link is restored on its own, and
  interrupted files resume.
- Thousands of small files no longer upload at a few per second on high-latency
  links.

### Removed

- The sidebar and the light theme.

## [0.3.0] - 2026-09-02

### Added

- FTP alongside SFTP.
- FTPS through `AUTH TLS`, with the certificate checked by SHA-256.
- Resume for FTP.

### Changed

- The panes, the queue and reconnecting no longer depend on the protocol.

## [0.2.0] - 2026-08-01

### Added

- A separate connection for transfers, so browsing folders does not slow down.
- A transfer queue with progress, speed and cancel.
- Resume and rules for existing files.
- File modification time and the executable bit are kept.
- Sign-in through an SSH agent and the keyboard prompt.
- A light theme.
- Breadcrumbs, back and forward, a name filter and sortable columns.

### Changed

- Errors appear in a bar inside the window instead of a separate dialog.
- Downloads go into a `.nexapart` file and are renamed when finished.

### Fixed

- Short SFTP replies no longer corrupt downloaded files.
- Files whose size the server does not report download in full.
- A server error while reading a folder no longer silently cuts the list short.
- Deleting a deep folder tree no longer runs the server out of resources.

## [0.1.0] - 2026-07-29

### Added

- Two panes: a local folder and the server.
- SFTP sign-in with a password and a private key.
- Server key check by SHA-256.
- Passwords protected by DPAPI.
- Automatic reconnecting.
- Upload, download and delete of whole folders.
- Drag and drop from Explorer, including files from a ZIP.
- A portable build for Windows.

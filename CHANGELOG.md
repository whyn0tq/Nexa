# Changelog

All notable changes to Nexa are documented in this file.

The project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.3.0] - 2026-09-02

### Added

- FTP connections alongside SFTP: the protocol is part of the connection
  profile and is remembered with the recent hosts.
- FTPS through explicit `AUTH TLS`, protecting both the control and the data
  channel, with the TLS session reused on data connections for servers that
  require it.
- SHA-256 verification of the server certificate for FTPS, stored and
  re-checked the same way SSH host keys are.
- Passive-mode data connections negotiated with `EPSV` and a `PASV` fallback,
  machine listings through `MLSD` with `LIST` parsing for Unix and DOS output
  when the server has no `MLSD`.
- FTP transfer resume through the `REST` marker, reusing the existing overwrite
  and partial-file rules.

### Changed

- Session handling moved behind a shared worker interface, so the browsing
  pane, the transfer queue and the reconnection logic are protocol independent.

## [0.2.0] - 2026-08-01

### Added

- Dedicated SSH session for transfers, so browsing stays responsive while large
  uploads and downloads are running.
- Transfer queue with per-item progress, speed, state and per-item cancellation.
- Resumable transfers: interrupted downloads and uploads continue from the byte
  they reached instead of restarting.
- Overwrite policies (resume, overwrite, skip, rename) applied per transfer.
- Preservation of modification time and the executable bit across transfers.
- Keyboard-interactive and SSH-agent authentication, negotiated against the
  methods the server actually advertises.
- Light theme alongside the dark one, with the choice remembered (Ctrl+T).
- Breadcrumb navigation with back/forward history for both panes.
- Name filter and sortable columns in both panes.
- Empty-state placeholders and an inline, dismissible notification area.
- Unit tests for the transfer overwrite/resume rules.

### Changed

- Errors are reported in an inline banner with a short history instead of one
  modal dialog per failure.
- Downloads are written to a `.nexapart` scratch file and renamed on success, so
  an interrupted transfer can never be mistaken for a complete file.
- Directory trees are walked iteratively, keeping one server handle open at a
  time instead of one per level.
- SFTP limits are negotiated once per session rather than once per file.
- The colour system is defined as design tokens in one place instead of inline
  stylesheet literals.

### Fixed

- Short SFTP reads no longer corrupt downloads: the pipeline is resynchronised
  at the byte actually received rather than the byte requested.
- Files whose size the server does not report are downloaded completely instead
  of being written as empty.
- A server-side failure during a directory listing is no longer treated as the
  end of the directory, which could silently truncate it.
- Deleting a deep tree no longer risks exhausting server handles or the stack.

## [0.1.0] - 2026-07-29

### Added

- Redesigned application workspace and visual system.
- Two-pane local and remote file manager.
- Password and private-key SFTP authentication.
- SHA-256 host-key verification.
- DPAPI-protected saved credentials.
- Automatic session restoration and reconnection.
- Recursive upload, download and delete operations.
- Pipelined remote directory listing and cache prefetch.
- Native drag and drop in both directions.
- Windows Explorer and ZIP virtual-file drag support.
- Asynchronous transfers with up to 64 in-flight requests.
- Portable Windows packaging script.
- Public development documentation and GitHub project infrastructure.

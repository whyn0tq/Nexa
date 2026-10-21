<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="CHANGELOG.ru.md">Русский</a>
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

All notable changes to Nexa are documented in this file. The project follows
[Semantic Versioning](https://semver.org/).

| Version | Date | In one line |
|---|---|---|
| [**1.1.0**](#110---2026-10-21) | 2026-10-21 | German interface, dropping files from a ZIP |
| [1.0.0](#100---2026-10-15) | 2026-10-15 | Tabs, the new transfer engine and the onyx look |
| [0.3.0](#030---2026-09-02) | 2026-09-02 | FTP and FTPS |
| [0.2.0](#020---2026-08-01) | 2026-08-01 | Transfer queue, resume and overwrite rules |
| [0.1.0](#010---2026-07-29) | 2026-07-29 | First two-pane SFTP client |

## [Unreleased]

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## [1.1.0] - 2026-10-21

### <img src="docs/assets/brand/icons/plus.svg" width="22" height="22" alt=""> Added

- German interface: pick it in Settings or start `Nexa.exe --language de`.
- `Nexa.exe` carries its product name and version, taken from `CMakeLists.txt`.
- `scripts/package.ps1` packs the release archive with the license texts of
  every shipped library.
- Pushing a `v*` tag builds the package in GitHub Actions and drafts the
  release with the archive and its SHA-256.

### <img src="docs/assets/brand/icons/edit.svg" width="22" height="22" alt=""> Changed

- Picking another language in Settings asks right away whether to restart
  Nexa or cancel, instead of a quiet hint under the flags.

### <img src="docs/assets/brand/icons/check.svg" width="22" height="22" alt=""> Fixed

- Files dragged straight out of a ZIP opened in Explorer upload again instead
  of failing with "Could not read the dragged files".
- When a server drops the connection before the handshake (usually a
  temporary IP block or SFTP turned off), Nexa says so in plain words instead
  of showing "Received SSH_MSG_DISCONNECT: 11".

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## [1.0.0] - 2026-10-15

> [!IMPORTANT]
> A rewrite of the interface and the transfer engine.

### <img src="docs/assets/brand/icons/plus.svg" width="22" height="22" alt=""> Added

- Several servers at once: every server opens in its own tab with its own
  browsing and transfer connections. Open tabs are restored on start.
- Saved servers with names and color labels, a launcher page with quick
  connect, and a command palette (`Ctrl+K`).
- A transfer dock shared by all sessions, with progress, speed, remaining
  time, retry and cancel, plus settings for parallel connections and the
  overwrite policy.
- The rule for files that already exist (resume, replace, skip, rename) is
  always in sight: a chip in the transfer bar, buttons with a plain
  explanation in the open dock, and cards in Settings. Choices open in the
  same frosted glass menus as the rest of the app instead of system lists.
- Dropping files anywhere in a session window uploads them to the open server
  folder; hovering a tab while dragging switches to it.
- Live latency for every session.
- View / edit server files in place (`F4`): the file opens in its program from
  a temp folder and every save is uploaded back automatically.
- Local pane file operations: delete to the Recycle Bin, rename, new folder.
- Settings (`Ctrl+,`) with the editor for view / edit: Notepad by default; when
  the chosen editor is missing, VS Code and then Notepad++ are used; any other
  program can be picked by path.
  `Delete` and `F2` act on the pane that has the focus.
- Frosted glass context menus that blur the window behind them.
- Instant view / edit: a selected small file is fetched in the background, so
  view / edit opens it without a network wait; otherwise small files are read
  and saved over the open connection in two round trips instead of a new login.
  The notification shows how long it took.
- Instant folders: while a folder is open, all its subfolders are listed ahead
  in one pipelined burst, so opening them shows the content at once and only
  a quiet check goes to the server.
- Fast connect: a server tab shows the folder it was last left in the moment it
  opens, from a copy kept on disk, while the login runs. The live listing
  arrives together with "connected": the start folder is resolved, opened and
  read in two pipelined round trips right behind the login. One transfer
  connection is opened ahead of time, so the first upload or download starts
  without a login of its own.
- English (US) interface, the default, with Russian one click away: language
  cards with flags in Settings, applied by a quick restart that brings the
  open tabs back. Dates and sizes follow the chosen language.
  `--language en|ru` picks it from the command line.
- `Nexa.exe --demo-shots <folder>` saves the README screenshots: made-up
  servers from an offline fake connection, an empty made-up local folder and
  sample transfers, in a throwaway settings profile.
- `tests/transfer_bench.cpp` to measure throughput against a real server,
  `tests/connect_bench.cpp` to measure login and first listing, and SFTP codec
  tests.
- Documentation in English and Russian, with artwork drawn from the app's own
  colors and icons (`scripts/brand-assets.mjs`).

### <img src="docs/assets/brand/icons/edit.svg" width="22" height="22" alt=""> Changed

- New SFTP client written on top of an SSH channel: up to 64 files and 24 MB
  of requests are kept in flight per connection instead of one request at a
  time, and folders are created, listed and deleted with pipelined batches.
- Transfers run over 1–16 parallel connections per session (4 by default);
  files of 40 MB and more are split into segments moved by several
  connections. FTP and FTPS use the same parallel queue.
- A larger socket send buffer: on Windows the default capped uploads at a few
  MB/s as soon as a link had latency.
- AES-GCM is preferred for SSH.
- Fewer round trips per login: the saved password, key or agent is tried
  first instead of an empty "none" probe, and SFTP packet limits are asked for
  only before the first transfer. Folder listings send their next reads only
  after the previous ones are answered and no longer wait for the close reply.
  At 200 ms ping the first listing now arrives in about 0.44 s after login
  instead of 0.8–1.0 s, and login takes about 2.3 s instead of 2.5 s.
- Completely new look: onyx black, frosted glass panels, red accent, own title
  bar with native resizing, Snap layouts and rounded corners, new icon.

### <img src="docs/assets/brand/icons/check.svg" width="22" height="22" alt=""> Fixed

- `Delete` removed the selection on the server even when a file was selected
  in the local pane. Deleted entries now disappear from the list at once, and
  the local pane refreshes when the window becomes active again.
- Sessions no longer drop when idle: the browsing connection is pinged, TCP
  keep-alive is tuned, and a broken connection is reconnected without limit
  once the server has been reached. Interrupted files resume where they
  stopped.
- Uploading thousands of small files no longer crawls at a few files per
  second on high-latency links.

### <img src="docs/assets/brand/icons/trash.svg" width="22" height="22" alt=""> Removed

- The sidebar layout and the light theme.

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## [0.3.0] - 2026-09-02

### <img src="docs/assets/brand/icons/plus.svg" width="22" height="22" alt=""> Added

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

### <img src="docs/assets/brand/icons/edit.svg" width="22" height="22" alt=""> Changed

- Session handling moved behind a shared worker interface, so the browsing
  pane, the transfer queue and the reconnection logic are protocol independent.

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## [0.2.0] - 2026-08-01

### <img src="docs/assets/brand/icons/plus.svg" width="22" height="22" alt=""> Added

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

### <img src="docs/assets/brand/icons/edit.svg" width="22" height="22" alt=""> Changed

- Errors are reported in an inline banner with a short history instead of one
  modal dialog per failure.
- Downloads are written to a `.nexapart` scratch file and renamed on success, so
  an interrupted transfer can never be mistaken for a complete file.
- Directory trees are walked iteratively, keeping one server handle open at a
  time instead of one per level.
- SFTP limits are negotiated once per session rather than once per file.
- The colour system is defined as design tokens in one place instead of inline
  stylesheet literals.

### <img src="docs/assets/brand/icons/check.svg" width="22" height="22" alt=""> Fixed

- Short SFTP reads no longer corrupt downloads: the pipeline is resynchronised
  at the byte actually received rather than the byte requested.
- Files whose size the server does not report are downloaded completely instead
  of being written as empty.
- A server-side failure during a directory listing is no longer treated as the
  end of the directory, which could silently truncate it.
- Deleting a deep tree no longer risks exhausting server handles or the stack.

<img src="docs/assets/brand/divider.svg" alt="" width="100%">

## [0.1.0] - 2026-07-29

### <img src="docs/assets/brand/icons/plus.svg" width="22" height="22" alt=""> Added

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

<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="VERSION.ru.md">Русский</a> · <a href="VERSION.de.md">Deutsch</a>
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

| Item | Value |
|---|---|
| **Version** | **1.2.0** |
| Date | October 24, 2026 |
| System | Windows 10 and 11, x64 |
| Language | C++20, Qt 6 interface |
| SFTP | own client on top of libssh 0.12 |
| FTP and FTPS | Qt 6 Network and OpenSSL |
| Interface | English (US), Russian, German |

## Where the version number lives

> [!IMPORTANT]
> The number is written by hand in two places. Change them together:
>
> - `project(Nexa VERSION 1.2.0 ...)` in `CMakeLists.txt`;
> - the table above, in `VERSION.md`, `VERSION.ru.md` and `VERSION.de.md`.
>
> The app and `Nexa.exe` take the number from `CMakeLists.txt`. The version badge
> in the README shows the latest GitHub release by itself.

Numbers follow [semantic versioning](https://semver.org/): `MAJOR.MINOR.PATCH`.
A new feature is a MINOR bump (1.3.0), a bug fix is a PATCH (1.2.1). Add `-beta.1`
or `-rc.1` for trial builds.

## How to make a release

1. Change the number everywhere listed above.
2. Move the entries from `Unreleased` into `CHANGELOG.md`, `CHANGELOG.ru.md` and
   `CHANGELOG.de.md`.
3. Make a Release build with no warnings and run the tests.
4. Check by hand: sign-in, several tabs, reconnecting, dragging from Explorer and
   from a ZIP.
5. If the look of the app changed, retake the screenshots in all three languages
   (`--demo-shots`). If the numbers or banner texts changed, run
   `node scripts\brand-assets.mjs`.
6. Tag it and push the tag:

   ```powershell
   git tag -a v1.2.0 -m "Nexa 1.2.0"
   git push origin v1.2.0
   ```

7. GitHub Actions builds the archive and drafts the release. Check the
   description and publish it.

## What is next (1.3)

- [ ] SSH terminal in a tab
- [ ] A transfer queue that survives a restart
- [ ] Pausing transfers
- [ ] Port forwarding and jump hosts
- [ ] Installer and auto-update

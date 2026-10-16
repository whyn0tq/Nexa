<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="CONTRIBUTING.ru.md">Русский</a>
</p>

<p align="center"><sub>
  <a href="README.md">Overview</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="VERSION.md">Version</a> ·
  <strong>Contributing</strong> ·
  <a href="SECURITY.md">Security</a> ·
  <a href="CODE_OF_CONDUCT.md">Code of Conduct</a> ·
  <a href="THIRD_PARTY_NOTICES.md">Third-party notices</a>
</sub></p>

# <img src="docs/assets/brand/icons/edit.svg" width="32" height="32" alt=""> Contributing to Nexa

Thank you for helping improve Nexa.

## <img src="docs/assets/brand/icons/check.svg" width="28" height="28" alt=""> Before you start

- Search existing issues before opening a new one.
- Use a bug report for reproducible defects.
- Use a feature request for product proposals.
- Keep pull requests focused on one change.

> [!WARNING]
> Do not include passwords, private keys, real server addresses or sensitive
> logs — not in issues, not in pull requests, not in screenshots.

## <img src="docs/assets/brand/icons/terminal.svg" width="28" height="28" alt=""> Development setup

Nexa currently targets Windows with MSYS2 UCRT64. Required packages:

| Package | Why |
|---|---|
| GCC with C++20 support | Compiler |
| CMake, Ninja | Build |
| Qt 6 Base | Interface, networking, FTP and FTPS |
| libssh | SSH and SFTP |
| pkg-config | Finding libssh |

Build and test:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## <img src="docs/assets/brand/icons/command.svg" width="28" height="28" alt=""> Code style

- Use C++20.
- Prefer Qt value types at UI and signal boundaries.
- Keep network and blocking filesystem work outside the UI thread.
- Use RAII for native, SSH and SFTP resources.
- Do not add secrets, machine-specific paths or generated binaries.
- Match the style of the surrounding file.
- Add or update tests for behavior changes.

## <img src="docs/assets/brand/icons/folder.svg" width="28" height="28" alt=""> Documentation

Every document exists in two languages: `NAME.md` in English and `NAME.ru.md`
in Russian, with a language switch at the top. When you change one, change the
other in the same pull request.

- Interface texts are written in Russian in the sources and translated to
  English in `src/ui/i18n_en.cpp`; keep the README wording in line with what
  the app actually says in each language.
- Screenshots are taken by the app itself: `Nexa.exe --demo-shots docs\assets`
  and `Nexa.exe --demo-shots docs\assets\ru --language ru`.
- The banner, the numbers, the section icons and the document headers in
  `docs/assets/brand` come from `node scripts\brand-assets.mjs`, which uses the
  colors from `src/ui/theme.h` and the icon shapes from `src/ui/icons.cpp`.
  Edit the script, not the SVG files.

## <img src="docs/assets/brand/icons/refresh.svg" width="28" height="28" alt=""> Commit messages

Use short imperative summaries:

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
Improve transfer progress throttling
```

## <img src="docs/assets/brand/icons/link.svg" width="28" height="28" alt=""> Pull requests

A pull request should include:

- the problem being solved;
- the implementation approach;
- testing performed;
- screenshots for visible UI changes;
- documentation updates in both languages where applicable.

> [!NOTE]
> By contributing, you agree that your contribution is licensed under
> GPL-3.0-or-later.

<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="CONTRIBUTING.ru.md">Русский</a> · <a href="CONTRIBUTING.de.md">Deutsch</a>
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

# <img src="docs/assets/brand/icons/edit.svg" width="32" height="32" alt=""> Contributing

Thanks for wanting to help Nexa.

## Before you start

- Look for your topic among the open issues first.
- Report a bug with the bug report template, an idea with the feature request one.
- One pull request, one change.

> [!WARNING]
> Never post passwords, private keys, real server addresses or logs with
> personal data: not in issues, not in pull requests, not in screenshots.

## Building

Nexa builds on Windows in MSYS2 UCRT64. You need GCC with C++20, CMake, Ninja,
Qt 6 Base, libssh and pkg-config.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Code

- Write C++20, in the style of the file you are editing.
- Keep the network and slow file work off the UI thread.
- Close SSH, SFTP and system resources with RAII.
- Keep secrets, paths from your machine and build output out of the repository.
- If you change behavior, add or fix a test.

## Documentation

Every document has three versions: `NAME.md` in English, `NAME.ru.md` in
Russian and `NAME.de.md` in German. Change one, change the others in the same
pull request. They have the same structure and the same content, and only the
language differs.

- UI strings are written in Russian in the source, and the translations live in
  `src/ui/i18n_en.cpp` and `src/ui/i18n_de.cpp`. Button names in the docs must
  match what you see in the app.
- The app takes its own screenshots:

  ```powershell
  .\build\Nexa.exe --demo-shots docs\assets --language en
  .\build\Nexa.exe --demo-shots docs\assets\ru --language ru
  .\build\Nexa.exe --demo-shots docs\assets\de --language de
  ```

- The banner, numbers and icons in `docs/assets/brand` come from a script. Edit
  the script, not the SVG files:

  ```powershell
  node scripts\brand-assets.mjs
  ```

## Commits

A short sentence in English, in the imperative:

```text
Fix reconnect when listing a remote directory
Add Explorer virtual-file drag support
```

## Pull requests

Say in the description what the problem was, how you solved it and how you
checked it. If the look of the app changed, add screenshots.

> [!NOTE]
> By sending code you agree that it is distributed under the GPL-3.0-or-later
> license.

<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="THIRD_PARTY_NOTICES.ru.md">Русский</a>
</p>

<p align="center"><sub>
  <a href="README.md">Overview</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="VERSION.md">Version</a> ·
  <a href="CONTRIBUTING.md">Contributing</a> ·
  <a href="SECURITY.md">Security</a> ·
  <a href="CODE_OF_CONDUCT.md">Code of Conduct</a> ·
  <strong>Third-party notices</strong>
</sub></p>

# <img src="docs/assets/brand/icons/info.svg" width="32" height="32" alt=""> Third-party notices

Nexa dynamically links to third-party software. Those components are not
relicensed under Nexa's GPL license.

## <img src="docs/assets/brand/icons/link.svg" width="28" height="28" alt=""> Direct dependencies

| Component | Purpose | License |
|---|---|---|
| Qt 6 Core | Application runtime | LGPL-3.0 / GPL-3.0 / commercial |
| Qt 6 Widgets | Desktop user interface | LGPL-3.0 / GPL-3.0 / commercial |
| Qt 6 Network | Network integration, FTP and FTPS transport | LGPL-3.0 / GPL-3.0 / commercial |
| libssh | SSH and SFTP protocol support | LGPL |
| OpenSSL | TLS for FTPS, loaded through the Qt TLS backend | Apache-2.0 |

Windows portable packages may also contain MinGW runtime libraries and
transitive dependencies of Qt and libssh. Every release must include the license
texts corresponding to the exact binaries shipped in that release.

## <img src="docs/assets/brand/icons/check.svg" width="28" height="28" alt=""> Distribution requirements

When packaging Nexa:

1. keep Qt and libssh as replaceable dynamic libraries;
2. include the applicable LGPL license texts;
3. provide notices and source availability information required by Qt;
4. include licenses for all transitive runtime DLLs;
5. document any modifications made to LGPL-covered libraries;
6. generate and review a dependency inventory before publishing a release.

> [!NOTE]
> This file is an inventory aid and not a substitute for the complete license
> terms distributed with each dependency.

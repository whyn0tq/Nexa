<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="THIRD_PARTY_NOTICES.ru.md">Русский</a> · <a href="THIRD_PARTY_NOTICES.de.md">Deutsch</a>
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

Nexa uses other people's libraries as separate DLLs. Their licenses stay as they
are and do not change to Nexa's GPL.

| Component | What it is for | License |
|---|---|---|
| Qt 6 Core | The base of the app | LGPL-3.0 / GPL-3.0 / commercial |
| Qt 6 Widgets | Windows and controls | LGPL-3.0 / GPL-3.0 / commercial |
| Qt 6 Network | Networking, FTP and FTPS | LGPL-3.0 / GPL-3.0 / commercial |
| libssh | SSH and SFTP | LGPL |
| OpenSSL | TLS for FTPS | Apache-2.0 |

The archive also contains the MinGW libraries and the ones Qt and libssh need
themselves. Their license texts are in the `licenses` folder next to `Nexa.exe`.

## If you build your own package

1. Keep Qt and libssh as separate DLLs, so they can be replaced.
2. Include the license texts of every library you put in the package.
3. Say where to get the Qt source code, as its license requires.
4. Describe your own changes to LGPL libraries, if you made any.

> [!NOTE]
> This list is only a reminder. The full terms are in the licenses of the
> libraries themselves.

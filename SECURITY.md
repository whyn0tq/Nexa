<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="SECURITY.ru.md">Русский</a> · <a href="SECURITY.de.md">Deutsch</a>
</p>

<p align="center"><sub>
  <a href="README.md">Overview</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="VERSION.md">Version</a> ·
  <a href="CONTRIBUTING.md">Contributing</a> ·
  <strong>Security</strong> ·
  <a href="CODE_OF_CONDUCT.md">Code of Conduct</a> ·
  <a href="THIRD_PARTY_NOTICES.md">Third-party notices</a>
</sub></p>

# <img src="docs/assets/brand/icons/shield.svg" width="32" height="32" alt=""> Security

## Supported versions

Security fixes are released only for the latest version.

## How to report a vulnerability

> [!CAUTION]
> Do not open a public issue if the problem involves:
>
> - leaked passwords or credentials;
> - private keys;
> - bypassing the server key check;
> - writing files outside the intended folder (path traversal);
> - unsafe unpacking of ZIP files or virtual files;
> - running someone else's code;
> - weak sign-in or encryption.

Open the **Security** tab of the repository and choose **Report a vulnerability**.
Include:

- your Nexa version and Windows version;
- how to reproduce the problem;
- what should have happened and what happened;
- why it is dangerous;
- an example, if it is safe to show one.

Do not attach working passwords or private keys. We will answer within seven
days after private reporting is turned on in the repository.

## What counts as a Nexa vulnerability

We accept reports about Nexa's own code. If the problem is in Qt, libssh or
another library, please tell its authors too.

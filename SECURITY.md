<p align="center">
  <a href="README.md"><img src="docs/assets/brand/header.svg" alt="Nexa" width="100%"></a>
</p>

<p align="center">
  <strong>English</strong> · <a href="SECURITY.ru.md">Русский</a>
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

# <img src="docs/assets/brand/icons/shield.svg" width="32" height="32" alt=""> Security policy

## <img src="docs/assets/brand/icons/info.svg" width="28" height="28" alt=""> Supported versions

Nexa is currently a preview project. Security fixes are applied to the latest
development version only.

| Version | Supported |
|---|---|
| Latest development version | Yes |
| Older releases | No |

## <img src="docs/assets/brand/icons/alert.svg" width="28" height="28" alt=""> Reporting a vulnerability

> [!CAUTION]
> Please do not open a public issue for vulnerabilities involving:
>
> - credential disclosure;
> - private-key handling;
> - host-key verification bypass;
> - arbitrary file overwrite or path traversal;
> - unsafe ZIP or virtual-file extraction;
> - remote code execution;
> - authentication or encryption weaknesses.

Use GitHub's **Private vulnerability reporting** feature on the repository
Security page. Include:

- affected version or commit;
- operating system;
- reproduction steps;
- expected and actual behavior;
- impact assessment;
- a minimal proof of concept, if safe.

Do not include real production credentials or private keys. You should receive
an initial acknowledgement within seven days after the repository enables
private security reporting.

## <img src="docs/assets/brand/icons/eye.svg" width="28" height="28" alt=""> Scope

Security reports should concern Nexa's own source code. Vulnerabilities in Qt,
libssh or other dependencies should also be reported to the corresponding
upstream project.

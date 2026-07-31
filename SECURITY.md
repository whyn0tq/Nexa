# Security policy

## Supported versions

Nexa is currently a preview project. Security fixes are applied to the latest
development version only.

## Reporting a vulnerability

Please do not open a public issue for vulnerabilities involving:

- credential disclosure;
- private-key handling;
- host-key verification bypass;
- arbitrary file overwrite or path traversal;
- unsafe ZIP or virtual-file extraction;
- remote code execution;
- authentication or encryption weaknesses.

Use GitHub's **Private vulnerability reporting** feature on the repository
Security page. Include:

- affected version or commit;
- operating system;
- reproduction steps;
- expected and actual behavior;
- impact assessment;
- a minimal proof of concept, if safe.

Do not include real production credentials or private keys. You should receive an
initial acknowledgement within seven days after the repository enables private
security reporting.

## Scope

Security reports should concern Nexa's own source code. Vulnerabilities in Qt,
libssh or other dependencies should also be reported to the corresponding
upstream project.

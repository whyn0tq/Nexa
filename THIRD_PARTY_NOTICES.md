# Third-party notices

Nexa dynamically links to third-party software. Those components are not
relicensed under Nexa's GPL license.

## Direct dependencies

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

## Distribution requirements

When packaging Nexa:

1. keep Qt and libssh as replaceable dynamic libraries;
2. include the applicable LGPL license texts;
3. provide notices and source availability information required by Qt;
4. include licenses for all transitive runtime DLLs;
5. document any modifications made to LGPL-covered libraries;
6. generate and review a dependency inventory before publishing a release.

This file is an inventory aid and not a substitute for the complete license
terms distributed with each dependency.

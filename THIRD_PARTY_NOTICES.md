# Third-Party Notices and Provenance

This document records third-party material and implementation provenance relevant to BonDriver_Friio_x64 Ver.1.0.

It is intended to preserve attribution and make the origin of the public source easier to trace. It is not a substitute for the license text or for the original notices contained in third-party files.

## IBonDriver.h and IBonDriver2.h

`IBonDriver.h` and `IBonDriver2.h` originate from BonSDK.

The original files identify themselves as part of BonSDK, contain the original copyright notice for `http://2sen.dip.jp/dtv/`, state that BonSDK comes with no warranty, and refer to "GNU GENERAL PUBLIC LICENSE 2.0" for details.

The original notices in these files are retained in this repository.

## BonSDK / Multi2Dec

The BonSDK material used for reference identifies itself as a modified version of the BonSDK included with Multi2Dec Ver.2.10. Its documentation states that licensing follows Multi2Dec and refers the reader to `Multi2Dec.txt`.

The Multi2Dec Ver.2.10 documentation contains both usage notices and source-distribution terms. Among other statements, it says that the sample program source code is handled under the GPL, that source code and binaries in the package may be attached to or incorporated into other software, and that at least the modified source portions are required when doing so.

Because the historical documentation contains conditions in addition to its GPL reference, users who redistribute material derived from BonSDK / Multi2Dec should review the original notices and documentation themselves.

BonDriver_Friio_x64 does not include the complete BonSDK or Multi2Dec package.

## BonDriver_Friio 3.1 / 3.2

BonDriver_Friio 3.1 and 3.2 were used as analysis and behavioral-comparison references during development.

BonDriver_Friio_x64 Ver.1.0 is not a simple x64 rebuild of those binaries. The x64 implementation was reconstructed around the WinUSB path and validated with White Friio hardware.

The historical BonDriver_Friio 3.1 / 3.2 binaries are not included in this repository or in the release package.

At the time this notice was prepared, the Friio download page listed BonDriverFriio-3.1 beta and BonDriverFriio-3.2 beta as downloads.

## White Friio initialization and device behavior

Some device-control knowledge used by BonDriver_Friio_x64 was established by comparing BonSDK material, BonDriver_Friio 3.1 / 3.2 behavior, static analysis, and hardware tests.

In particular, the project records provenance for the White Friio initialization path, PRECONTROL sequence, register initialization, channel selection, TS transfer, and signal-level behavior.

The implementation in this repository was written for the x64 WinUSB version of this project. Historical BonSDK-specific DeviceIoControl / binary-resource paths were treated as reference material rather than copied wholesale into the main implementation path.

## Microsoft Windows SDK / WinUSB

BonDriver_Friio_x64 uses Windows APIs including SetupAPI and WinUSB and is built against the Microsoft Windows SDK.

Microsoft SDK binaries or source code are not included in this repository merely by virtue of those build dependencies.

## Project code

New project code is identified in the source as:

`Copyright (C) 2026 AKT`

`SPDX-License-Identifier: GPL-2.0-only`

See the repository `LICENSE` file for the project license text. Third-party files retain their original notices, which should be read together with this document.

## Source and release

Project source:
https://github.com/AKT-AIC/BonDriver_Friio_x64

Initial public release tag:
`v1.0`

---

This notice describes provenance as understood and documented for Ver.1.0. It does not replace the original third-party notices or constitute a legal determination of their scope.

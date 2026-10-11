# Licensing boundary — publication review pending

This is a local Q17 source/configuration and binary snapshot, not a complete SDK distribution. The repository has no blanket MIT license. Selecting MIT for project-authored code does not relicense SDK components, derived examples or the compiled firmware as a whole.

Original motor bridge code, standalone OTA guard and project tools/tests are candidates for MIT licensing, subject to a license selection. MIT allows commercial reuse and modification without requiring publication of changes, while retaining its copyright and license notice. See the [OSI MIT text](https://opensource.org/license/mit).

`app/app.c`, `app/smartwings_ota_policy.c`, `app/ota-client-policy.h` and multiple application/bootloader configuration headers carry Silicon Labs MSLA Source Code notices. They are modified vendor files, including motor-triggered joining and OTA/version guards. Retaining their notices alone does not grant public redistribution permission. Before publishing them, confirm the license supplied with the installed SDK and any applicable permission. The [current MSLA](https://www.silabs.com/about-us/legal/master-software-license-agreement) distinguishes ordinary Source Code from explicitly identified Silicon Labs Open Source Code; sections 4/5 restrict source redistribution, while section 7.2 grants rights for the latter category. These files do not identify themselves as Silicon Labs Open Source Code.

`main.c` and many configuration headers have vendor Zlib notices. Preserve each original notice and mark modifications where its terms require it. SDK components and the release binaries retain applicable Silicon Labs/third-party terms; use is intended for the Silicon Labs target hardware. The SDK is installed separately. No claim is made that the whole firmware is OSI-open-source or portable under MIT.

This repository must be reviewed for vendor source and binary distribution terms before public publication. A possible source-sharing approach is distributing original project code plus build configuration and obtaining vendor sample files locally from the separately installed SDK; any patches or replacement callbacks still need a provenance review.

This is an independent project. Stock firmware dumps, private keys, Zigbee credentials and local account configuration are excluded. This document records the packaging/licensing boundary and is not a legal opinion.

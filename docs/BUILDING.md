# Rebuilding the Q17 snapshot

Install Silicon Labs Simplicity SDK **2026.6.1** (Zigbee **9.1.1**), SLC CLI **6.0.25**, Java **21.0.6**, ARM GCC **14.2.rel1**, CMake **3.30.2**, Ninja **1.12.1**, ZAP package **2026.06.17**, and Commander **1.22.1**. Python 3.10+ runs the project scripts. Install vendor dependencies through Silicon Labs; they are not vendored here. `pkg.slt` and `pkg.lock` record the original managed package environment.

Copy `tools/toolchain.example.json` to the ignored `tools/toolchain.local.json` and fill in installed paths. `slc` is an argument array: use `["slc"]` for a working launcher or `["/path/to/java", "-jar", "/path/to/slc.jar"]` to bypass a broken launcher. JSON paths on Windows can use forward slashes. `gcc` is the toolchain root containing `bin`, `zap` is the ZAP tool directory.

```text
python tools/test.py
python tools/build.py --config tools/toolchain.local.json
```

The build script copies tracked inputs to a fresh ignored `build/` directory, rebases the ZAP SDK paths there, runs SLC generation and CMake/Ninja, and packages the application as GBL and Zigbee OTA. Generated autogen/CMake files are intentionally excluded because they embed installed SDK paths and are reproducible from these inputs. The custom cluster XML referenced by ZAP, all configuration headers, OTA policy/guard and motor files are required; `app.c`, SLCP and ZAP alone are insufficient.

It never flashes hardware or replaces the published Q17 files. Output is in `build/artifacts`. It compares rebuilt app/bootloader BIN hashes with the retained release to expose differences. Differences require review before substituting a new build for the tested release. A reproduction check is recorded in `docs/PACKAGING_VERIFICATION.md`.

The checked-in ZAP uses `${SDK_ROOT}` placeholders for portability; the script replaces them before invoking SLC. Open the generated build copy in ZAP after rebasing, or manually update its SDK package paths before editing the tracked input. The application needs endpoint 1 and persistent custom token keys 0xB000 (poll interval), 0xB001 (Inverted mode). Do not move these keys in an OTA update.

For a new version, update both SLCP application/OTA version settings, corresponding config headers and ZAP identity/OTA attributes, then requalify the image. The current script deliberately packages version 17 only. Bootloader partition changes and NVM migrations require separate design/validation; source compile success is not hardware qualification.

The tested bootloader binaries are the raw `cmake_gcc/build/base` outputs. Do not silently replace them with differently transformed `*-crc` artifacts produced by a post-build profile.

This source snapshot is local pending licensing review. See `NOTICE.md` before distributing vendor-derived source.

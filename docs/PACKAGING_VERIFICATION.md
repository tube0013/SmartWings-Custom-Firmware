# Q17 packaging verification — 2026-10-11

* Fresh SLC generation and ARM GCC/CMake/Ninja builds completed from the packaged app and bootloader inputs, using separately installed SDK 2026.6.1.
* Rebuilt application BIN SHA-256: `752dfdf59393857e288d42b393d395a7d4a63672a7a69176fb2c5f99db6180f4`; byte-identical to the tested Q17 application.
* Rebuilt bootloader BIN SHA-256: `d997a439b0361cb7e93020857fb1da8c0752e7471fcbaf261454fec5c098e6d3`; byte-identical to the tested custom bootloader.
* Four host regressions passed with AddressSanitizer and UndefinedBehaviorSanitizer: motor parser/control, captured wake-prefix/triplet/network events, complete polling attribute value domain, and OTA ID/version guards.
* All release SHA-256 hashes and HEX/S-record checksums passed. Combined S37 is byte-equivalent to the hardware-tested initial HEX and contains only bootloader, app and explicit blank custom NVM3. Update S37 contains only bootloader/app. No records target the manufacturing page, User Data or DEVINFO.
* OTA header/IDs/version/hardware bounds and embedded GBL matched the published application update container. App loadable bytes: 174704; bootloader: 10856; OTA container: 174858 bytes.
* Only offline tools were run for packaging. No device flashing, coordinator configuration or motor motion was performed.
* Source portability changes only: remove generated `slc_args.json`, replace local SDK paths in ZAP with rebased placeholders, add documentation/tooling. Published release images were preserved.

This validates packaging and build reproducibility on the installed Mac toolchain. It does not add hardware qualification beyond the earlier Q17 bench tests.

# OTA investigation and limits

The stock application accepted OTA image type 0x0000, version 2. Its download writer used 0x66000, while the original Gecko bootloader 1.12.0 had a 0x30000-byte storage slot beginning at 0x44000. That mismatch prevents the bootloader from reading the image the application downloaded.

An original module was restored to its own complete stock main-flash backup and read back byte-for-byte before testing. A full stock-to-custom transfer completed but ended in INVALID_IMAGE (150). Reducing the custom application size did not repair the storage mismatch. Host disassembly and routing/token checks found no supported alternative image type or mutable configuration path that redirected the writer to 0x44000. This is a blocked supported OTA migration, not a claim that every conceivable exploit has been disproven.

GBL application destination addresses describe where an accepted image will install; they do not select where the stock OTA client stores the download. A transition application cannot modify storage until it has first been installed and executed. Including a new bootloader in the same download has the same first-install dependency.

A hardware-assisted stock bootloader slot modification followed by OTA worked during investigation. Because that test already changed the bootloader over SWD, it does not establish a software-only route for an untouched stock device. Initial replacement requires physical SWD access; spare modules can be flashed and swapped individually, followed by a fresh Zigbee join.

The custom layout aligns application storage and bootloader slot at 0x5C000. Future custom-to-custom OTA is supported and has been exercised. Q17 still has an observed SDK OTA cached-size limitation after a previous larger image: a transfer can finish its payload but fail because stale size metadata is used. A retry installed Q17. The code-level explanation is supported by the SDK query/temp-data path, but was not proved by live RAM tracing; this snapshot deliberately preserves the tested Q17 build without introducing a new fix.

These images use CRC integrity verification and version checks, not enforced signatures or cryptographic rollback protection. They do not change OTP secure-boot/debug-access policy.

Vendor intent cannot be inferred from this defect. SmartWings support declined motor firmware modifications and offered replacement hardware or SmartLink Pro; this does not establish that updates were never intended.

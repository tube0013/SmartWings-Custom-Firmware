# Hardware and affiliate links

## Debugger choices

* [Simplicity Link Debugger SI-DBG1015A](https://www.silabs.com/development-tools/wireless/simplicity-link-debugger): lower-cost SWD/J-Link option with VCOM/PTI and optional 3.3 V power. Check the [UG548 guide](https://www.silabs.com/documents/public/user-guides/ug548-brd1015a-user-guide.pdf) for its connector and breakout arrangement. It is not an AEM current profiler.
* [Wireless Pro Kit mainboard / WSTK family](https://www.silabs.com/development-tools/wireless/wireless-pro-kit-mainboard): SWD/debugging plus AEM power profiling on the suitable board. Confirm board revision and external-target connector wiring. The project's external-target measurements used OUT mode and measured VAEM power through the Mini Simplicity connector, with the plug-in radio board removed.
* Pogo-pin clip: verify pad pitch, contact count and orientation against the actual module before buying. An appropriate seven/eight-contact clip can reach UART/SWD pads; an unverified marketplace listing is not a recommendation.
* RJ9 **4P4C** cable/coupler: RJ11 hardware is incompatible. Verify continuity and conductor mapping rather than assuming colors or straight/reversed cable wiring.

The [Tuya ZS13S datasheet](https://developer.tuya.com/en/docs/iot/zs13s-module-datasheet?id=K9n5ccelflel2) is a starting reference. With the component side facing you, RJ9 at the top and antenna at upper right, the left-side contact row is pads 12 through 19 from top to bottom:

| Pad | Signal |
| --- | --- |
| 12 | PC01 |
| 13 | PA00 |
| 14 | PB00 |
| 15 | Reset |
| 16 | PA5 — module UART TX |
| 17 | PA6 — module UART RX |
| 18 | PA01 — SWCLK |
| 19 | PA02 — SWDIO |

UART adapter TX connects to module RX, and adapter RX to module TX, with shared ground and 3.3 V signaling. A receive-only capture leaves adapter TX disconnected. Pin map is for the tested module; confirm orientation before applying power. SWD requires target power/reference and shared ground even when only two debug signals are used.

The motor battery supply is separate from the 3.3 V radio supply. Do not backfeed power between the motor and debugger. Bench tests with independent radio power isolated the radio supply conductor of the RJ9 cable and verified isolation with a continuity meter. A taped contact must be rechecked after reconnecting; proper cable modification is preferable for a durable fixture.

## Affiliate status — checked 2026-10-11

The links above are ordinary manufacturer links; no affiliate tracking has been added. The owner already has Amazon Associates access. Exact tracking links and suitable product listings are still needed.

I could not verify a publicly documented purchase-commission affiliate program for **Mouser or DigiKey** from official sources. Do not interpret this as proof that no private arrangement exists. Distributor supplier, design-referral and systems-integrator programs are different from consumer affiliate programs. Ask the distributor directly before claiming an affiliate relationship.

AliExpress has an official affiliate program with a [published service agreement](https://cdn.contract.alibaba.com/terms/b_platform_service_agreement/20240912121955398/20240912121955398.html?lng=en). The owner's enrollment status has not been verified. Use links generated through the enrolled affiliate account rather than adding guessed tracking parameters.

When actual affiliate links are added, put this disclosure before the shopping links: **“Some links below are affiliate links. I may earn a commission from qualifying purchases.”** Mark each relevant link “affiliate link.” For Amazon add its required wording: **“As an Amazon Associate I earn from qualifying purchases.”** Keep manufacturer datasheets and instructions as direct links. No affiliation with SmartWings or Silicon Labs is implied.

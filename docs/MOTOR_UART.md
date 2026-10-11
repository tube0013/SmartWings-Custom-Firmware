# SmartWings motor UART protocol

Documented 2026-10-11. Scope: the tested WM25/L-Z lift-only honeycomb motor and Tuya ZS13S / EFR32MG21 radio module, with Q17 as the current custom firmware. Other motor families and tilt-capable products are unverified. All frame bytes below are **hexadecimal**; percentages and elapsed times are decimal unless marked otherwise. Byte offsets start at zero.

## How the protocol was discovered and confirmed

We reverse-engineered the supplied **stock Zigbee radio firmware dumps**, tracing ARM Thumb code that configures the UART, builds transmitted packets, checks received frames and writes Zigbee attributes. This recovered command bytes, checksum rules, payload offsets and the direction-specific join/leave behavior. We did not have a firmware dump of the separate motor MCU: radio disassembly explains what the radio sends and consumes, not every internal motor function.

We then checked the interface with a two-channel USB logic analyzer and PulseView/Sigrok: D0 observed module TX → motor RX; D1 observed motor TX → module RX. Physical motor/remote actions and Zigbee commands supplied real traffic. Fifteen retained native recordings total **310 seconds**, sampled at **1 MHz**. Independent decoding checked packet bytes, length/XOR checksums, raw transitions, zero prefixes, repeated reports and timing rather than relying only on screenshots. These captures confirm the core lift command set and its motor responses, including exact final arrival at two percentage targets. They also confirm the leave/join exchange and stock initialization traffic.

Later custom-firmware bench work added USB-UART simulation, real-motor commands, observed movement/Stop, remote-driven feedback, and pairing/wake tests. Synthetic UART injections verify the replacement parser's handling of fragments, corrupt packets, repeats and values; they are not evidence that a real motor emits those synthetic values. Host capture replay accepted **205 motor frames across the 15 recordings** with matching decoded results.

**Not every opcode found in a dump was physically validated.** The tables distinguish static-only paths and unknown meanings. In particular, motor-originated FB and the generic eight-byte lift/tilt handlers must not be described as logic-analyzer-confirmed usable commands.

Sanitized packet/timing excerpts and archive SHA-256 identifiers are provided in [MOTOR_UART_CAPTURES.json](evidence/MOTOR_UART_CAPTURES.json). Stock firmware binaries, device credentials and private local paths are excluded. Function addresses below refer specifically to the original stock main-flash image with SHA-256 `69701ccc5a28a407254ee5ed2850aed47304e56e53d9a26cb88edfae9ac40ac8`; they are not Q17 function addresses.

| Evidence label | Meaning |
| --- | --- |
| Static + captured | Found in the stock binary and observed in native logic recordings |
| Bench confirmed | Also exercised with custom firmware and real hardware and observed behavior |
| Static only | Code path found; physical motor effect has not been established |
| Captured, meaning unknown | Bytes are real, but their purpose is not fully decoded |

## Electrical interface and capture setup

| Item | Value |
| --- | --- |
| UART | 9600 baud, 8 data bits, no parity, 1 stop bit |
| Signaling | 3.3 V logic, idle high, least significant bit first |
| Module TX | PA5 / module pad 16; analyzer D0 |
| Module RX | PA6 / module pad 17; analyzer D1 |
| Reference | Shared ground |
| Logic-analyzer sample rate | 1 MHz in retained recordings |
| Storage | Native Sigrok `.sr` recordings; decoded byte/timing evidence retained |

Connect an adapter's RX to module TX and its TX to module RX. For passive observation of an installed module, disconnect the adapter's TX so it cannot contend with the motor's transmitter. The motor supply and radio's 3.3 V supply are distinct; see [hardware wiring](HARDWARE.md). Module pad numbers are not RJ9 contact numbers; verify cable continuity instead of assuming a mapping.

A USB-UART terminal is not a text-command console for the motor. The interface uses binary frames. Firmware RTT commands such as `open`, `close`, `stop` and `target` are diagnostic inputs to the radio application, not ASCII motor protocol commands.

## Frame format, checksum and timing

Normal messages use:

```text
[length including checksum] [opcode] [payload ...] [checksum]
```

The checksum is the XOR of all preceding frame bytes. XOR of the complete valid frame is zero. Example: `03 XOR 01 = 02`, so `03 01 02` is valid. This is not a CRC, additive sum or ASCII hex string. Wire time is about **1.04 ms per byte** at 9600 8N1.

The stock receive assembler accepts an initial length of 1–9; valid known motor frames are 3, 4 or 7 bytes. This inbound limit does not apply to the 13-byte outgoing F1 command. Q17 uses opcode-specific lengths, rejects bad checksums and expires truncated frames after a 20 ms inter-byte gap.

Every reviewed motor frame has its own standalone **`00` prefix**, usually about **5–6 ms before** its length byte. This prefix is outside the frame and excluded from the XOR calculation. A wake/synchronization role is plausible; the stock binary alone does not establish the motor's intent. Q17 uses the RX edge to wake from EM2, then enables the USART for reception.

Motor position/battery and network-request groups commonly repeat **three times**, with copies about **56.3 ms apart**. Longer gaps were also observed. These are observed repetitions, not proven acknowledgment-driven retries. Inspected F5/F8 handlers send no UART ACK. Do not count every repeated report as a new movement or require all three copies before accepting valid data.

## Module → motor: established lift commands

| Operation in native stock ZCL mapping | Complete frame | Response behavior | Evidence |
| --- | --- | --- | --- |
| Up / Open, ZCL command 0x00 | `03 01 02` | F5 position snapshot observed after command; later position/battery reports follow motor activity | Static + captured; movement/Stop bench confirmed |
| Down / Close, ZCL command 0x01 | `03 02 01` | Same F5/F8 report family | Static + captured; movement/Stop bench confirmed |
| Stop, ZCL command 0x02 | `03 03 00` | F5 position, followed by F8 battery; can refresh a stopped motor's cached radio state | Static + captured + bench confirmed |
| Go To Lift Percentage, ZCL command 0x05 | `0D F1 00 00 00 00 00 00 04 40 PP 00 CC` | Initial F5/F8 snapshot and final target F5/F8 observed; no continuous position stream required | Static + captured; exact arrival confirmed for targets 20 and 55; custom bench confirmed |

These response descriptions describe observed sequences, not an unconditional response-time contract or dedicated success/failure ACK. An initial F5 can contain the previous/current position, so receiving it does not mean the requested move is complete.

### Percentage packet layout

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | `0D` | Total length: 13 |
| 1 | `F1` | Target-position opcode |
| 2–7 | `00 00 00 00 00 00` | Fixed bytes in the confirmed packet |
| 8–9 | `04 40` | Fixed bytes; deeper semantics not established |
| 10 | `PP` | Raw motor target percentage, 0–100 |
| 11 | `00` | Fixed byte |
| 12 | `CC` | `B8 XOR PP` |

Examples:

```text
Raw target   0: 0D F1 00 00 00 00 00 00 04 40 00 00 B8
Raw target  20: 0D F1 00 00 00 00 00 00 04 40 14 00 AC
Raw target  25: 0D F1 00 00 00 00 00 00 04 40 19 00 A1
Raw target  55: 0D F1 00 00 00 00 00 00 04 40 37 00 8F
Raw target  75: 0D F1 00 00 00 00 00 00 04 40 4B 00 F3
Raw target 100: 0D F1 00 00 00 00 00 00 04 40 64 00 DC
```

These examples are computed from the validated format; the capture evidence specifically proves complete target arrival for 20 and 55. Do not imply that every example row was a separate physical test.

### Coordinate conventions and inversion

Canonical Zigbee lift position is **0 = open, 100 = closed**. Home Assistant's displayed percent-open value is the complement: `HA = 100 - ZCL`. The stock radio forwards F1 targets and F5 positions unchanged. The stock SmartWings ZHA quirk separately swaps Open/Close command IDs; it does not invert F1 targets.

The motor's own direction setting can reverse the physical relationship of its raw percentage to openness. Q17 converts targets and feedback together when Inverted mode is enabled: `raw target = 100 - canonical target`, `canonical report = 100 - raw report`. With Inverted disabled, those values pass through. Q17 uses percentage targets for Open/Close and scene recalls; the short frames above document the underlying stock interface. Validate physical direction after installation; raw target equality alone does not prove correct physical UI orientation.

## Motor → module: position report F5

```text
07 F5 S2 S3 PP U5 CC
```

| Offset | Field | Known meaning |
| --- | --- | --- |
| 0 | `07` | Seven-byte frame |
| 1 | `F5` | Position/status report |
| 2–3 | `S2 S3` | Status bytes; associations below are provisional |
| 4 | `PP` | Raw motor position percentage |
| 5 | `U5` | Unknown; observed `00`, `B4`, `7E` |
| 6 | `CC` | XOR checksum |

The stock radio consumes offset 4 and writes Window Covering cluster `0x0102`, Current Position Lift Percentage attribute `0x0008`, without scaling or inversion. It suppresses updates when the percentage is unchanged and ignores the other status fields. Q17 validates the percentage and keeps unknown status bytes in diagnostics.

| Observed status pair | Association and limit |
| --- | --- |
| `1E 10` | Associated with upward/opening motion in explicit manual-stop sequences |
| `1E 20` | Associated with downward/closing motion in explicit manual-stop sequences |
| `0E 00` | Seen in startup/current snapshots, at endpoints, after Stop, and after intermediate target arrival |

`0E 00` is **not** a proven universal instantaneous “stopped” flag: it also appears at movement start. `U5` is not established as direction, inversion, limits, speed, voltage or command source. There is no valid basis for interpreting it as padding either.

Example measured raw position 20:

```text
07 F5 0E 00 14 00 E8
```

## Motor → module: battery report F8

```text
04 F8 BB CC
```

`BB` is offset 2. The original retained native recordings contain `BB = 64` (100 decimal):

```text
04 F8 64 98
```

The stock radio stores the byte directly in Power Configuration cluster `0x0001`, Battery Percentage Remaining `0x0021`; the stock HA quirk compensates for its units. Q17 treats valid 0–100 motor values as percent and reports `2 × BB` in standard Zigbee half-percent units: 100% becomes 200 (`C8`). Invalid battery values become unknown (`FF`).

The real-motor 100% point and packet format are captured; injected lower values verify the custom parser/conversion but do not independently calibrate the motor's full battery scale. This is a percentage report, not pack voltage or cell voltage. Radio firmware cannot infer a measured battery voltage from this byte.

## Stop response and attribute freshness

An independent Stop recording contains exactly one module command and the following first motor response copies:

| Time from capture start | Direction | Frame |
| --- | --- | --- |
| 3.766284 s | Module → motor | `03 03 00` |
| 3.775546 s | Motor → module | `07 F5 0E 00 46 B4 0E` — raw position 70 |
| 4.244439 s | Motor → module | `04 F8 64 98` — battery 100 |

Each motor frame is preceded by its standalone zero and repeated three times. The first F5 starts **9.262 ms** after Stop starts; F8 starts **478.155 ms** after Stop starts. These are UART timings, not mouse-click-to-motion timings or universal motor deadlines.

Stops during movement can produce a direction-associated F5 group followed about 169 ms later by `0E 00`. Automatic intermediate/end-limit arrivals can report `0E 00` directly. No continuous position telemetry appears during the observed motion intervals; one captured run includes 14.66 seconds without a UART transition.

Ordinary Zigbee attribute reads return the radio's stored values and do not generate a new motor query. No dedicated non-moving position/battery query was found in the inspected transmit paths. Stop is a confirmed **motion-affecting refresh**, not a passive read. Q17 sends it once near startup after about one second; a newer movement request cancels the pending startup Stop. It is not periodically sent for telemetry. Reports after local/RF-remote actions can update the radio independently of Zigbee targets.

## Network control: direction matters

The same three bytes can mean a request in one direction and a state notification in the other. Never classify these packets without recording which TX line emitted them.

| Direction | Complete frame | Stock-radio behavior | Evidence |
| --- | --- | --- | --- |
| Motor → module | `03 FA F9` | Start network steering if unjoined; no fresh steering via this branch if already joined | Static + captured; custom motor-triggered pairing bench confirmed |
| Motor → module | `03 F9 FA` | Leave if joined; if already unjoined, reply `03 FB F8` | Static + captured |
| Motor → module | `03 FB F8` | Reply `03 FB F8`, then request leave if joined | **Static only** for real motor-origin traffic |
| Module → motor | `03 FA F9` | Joined-state notification | Static + captured |
| Module → motor | `03 FB F8` | Unjoined-state notification or explicit leave-request reply | Static + captured |

In the reset/rejoin recording, motor F9 requests begin at 10.857670 s, FA requests at 11.026422 s, and the module's joined notification at 15.369364 s. UART confirms the request/notification exchange; HA/stack logs provide evidence of association and interview success. This is leave followed by fresh joining, not necessarily the Zigbee secured-rejoin operation.

These bytes do not establish a CPU reset, full flash erase or removal of motor calibration/RF pairing. The normal pairing gestures were tested: P1 held about six seconds and released after the second jog, or remote **P → Up → P**. Physical gestures act on the motor MCU; the radio cannot press the remote.

Q17 implements motor requests through the transport/network callbacks, with a join window of approximately 60 seconds and state notifications. It does not treat every repetition as a separate reset.

## Other known traffic and unverified paths

| Direction/path | Bytes or behavior | What is established |
| --- | --- | --- |
| Stock module → motor at initialization | `66 66 11 22 33 44 55 66` | Static + captured; not a normal length-prefixed frame. Purpose unknown. Q17 does not reproduce it; Q17 uses the startup Stop policy. |
| Motor → module | `03 F6 F5` | Captured, valid XOR; opcode is F6 and final F5 is its checksum. Stock receive dispatch performs no identified action. No motor-ready or ACK meaning is proven. |
| Stock native ZCL Go To Lift Value, 0x04 | Eight-byte shared-buffer send; modifies offsets 5–6 and sets offset 7 to `66` | Static only; full frame depends on prior buffer contents, motor effect untested |
| Stock native ZCL Go To Tilt Value, 0x07 | Eight-byte shared-buffer send; modifies offsets 5–6 and sets offset 7 to `67` | Static only; outside tested lift-only scope |
| Stock native ZCL Go To Tilt Percentage, 0x08 | Eight-byte shared-buffer send; modifies offset 6 and sets offset 7 to `65` | Static only; outside tested lift-only scope |

The generic eight-byte handlers do not initialize the complete buffer or call the ordinary XOR helper in their own bodies. There is no safe, independently validated command template to publish for them. Their presence is not evidence of usable limit-setting or calibration commands. No dedicated UART inversion, travel-limit programming, RF-pairing, speed-control or passive telemetry-query command has been established. Motor-local inversion produced no separate UART command in its captured interval; that does not prove every setup operation is silent.

## Static audit locations

Addresses in the identified stock image, useful when checking the disassembly against this reference:

| Routine | Address |
| --- | --- |
| XOR checksum helper | `0x4424–0x4440` |
| Receive byte assembly | `0x4490–0x44DC` |
| UART buffer sender | `0x44F0–0x4508` |
| UART configuration/initialization | `0x4510–0x45AE` |
| Network-state notifier | `0x4694–0x4702` |
| Checksum and opcode dispatcher | `0x4894–0x49B6` |
| Motor FB branch | `0x48BA–0x48E6` |
| Motor F9 branch | `0x48E8–0x491A` |
| Motor FA branch | `0x491C–0x4936` |
| F5 branch / percentage load | `0x4938` / `0x493E` |
| F8 branch / battery load | `0x497E` / `0x4984` |
| Open / Stop / Close | `0x4A0C` / `0x4A40` / `0x4A70` |
| Lift percentage / target-byte store | `0x4AA4` / `0x4ABE` |
| Unverified tilt percentage / lift value / tilt value | `0x4AF4` / `0x4B14` / `0x4B38` |
| Window Covering command dispatcher | `0x4B5C–0x4BE8` |

The static sender inventory found twelve direct call sites, all explained by initialization, network state/replies, four established lift commands and the three unverified generic paths. That bounded audit found no additional passive-query transmitter; it is not a formal proof over every computed execution path or the unavailable motor firmware.

## Custom implementation references

* [Parser, frame builders and position conversion](../app/motor/smartwings_motor.c)
* [USART queues, wake handling and network dispatch](../app/motor/smartwings_transport.c)
* [Zigbee lift/battery/scene bridge](../app/motor/smartwings_zcl.c)
* [Motor-triggered network callbacks](../app/app.c)
* [UART architecture overview](ARCHITECTURE.md)
* [Host protocol tests](../tests/test_motor.c) and [wake/repetition tests](../tests/test_motor_wake.c)

Run `python tools/test.py` for host regressions. Those tests check the implementation, not physical motor performance. Reliable group multicast and future OTA migration are separate Zigbee/bootloader issues; they are not solved by adding a new motor UART opcode.

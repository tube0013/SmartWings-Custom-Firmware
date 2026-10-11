# Changing the idle poll interval in Home Assistant

No firmware quirk or poll-control entity is required. In this HA test instance, the ZHA Manage Zigbee Device screen recognizes the manufacturer-specific cluster but does not know its attribute definitions, so its attribute selector is empty. The pictured alternative uses the separately installed **ZHA Toolkit** integration to issue a typed attribute write. Toolkit is an HA-side tool; it is not part of the firmware. Install/enable it separately if you want to use this example.

Open **Settings → Tools → Actions**, select YAML mode, and enter the example below. Older HA versions may call this page Developer Tools. Replace `ieee` with your device's exact name, entity name or IEEE address. The service's field description accepts all three; the screenshot uses the test device name to avoid exposing its IEEE address.

```yaml
action: zha_toolkit.attr_write
data:
  ieee: "YOUR_DEVICE_NAME_OR_IEEE"
  endpoint: 1
  cluster: 64512 # 0xFC00
  attribute: 0 # 0x0000
  manf: 65534 # 0xFFFE
  attr_type: 33 # uint16, 0x21
  attr_val: 500 # milliseconds
  read_before_write: false
  read_after_write: true
  tries: 2
  fail_exception: true
```

Supported values for `attr_val` are **250, 500, 750, 1000** milliseconds. The default is 250 ms. The attribute persists across reboot. This is the configurable idle polling interval; active exchanges can use fast polling.

![Prepared 500 ms typed write in Home Assistant Actions](images/ha-poll-write.jpg)

**This screenshot is a prepared example only. “Perform action” was not pressed, and this documentation capture did not change the device's poll interval.** To deliberately apply your chosen value later, check the target and parameters, then perform the action and review its write/readback response.

## Device page

![Q17 device page, IEEE address redacted](images/ha-device-redacted.png)

Captured from the test instance on 2026-10-11. The page shows Q17 (0x00000011), standard battery reporting and the Inverted control. The Zigbee/IEEE address was blurred using the built-in image-editing tool; the relevant UI labels and states were visually checked against the original capture. The action screenshot is an unedited browser capture using a device-name target.

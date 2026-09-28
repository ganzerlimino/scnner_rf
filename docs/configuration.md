# Configuration

`data/config.json` is the file flashed by `pio run -t uploadfs`. At runtime the same document lives at `/config.json` on LittleFS. The web UI edits that copy. Keys stay in English so one file is the source of truth; labels are translated in `data/locales/en.json` and `data/locales/it.json`.

`_readme_en` and `_readme_it` are hints. The firmware ignores unknown keys, including those two.

## Top level

| Key | Default | Notes |
| --- | --- | --- |
| `schema` | `1` | Document version. The loader still accepts the known keys if this changes. |
| `language` | `en` | `en` or `it`. The UI language. |
| `api_token` | empty | If set, `POST` and `DELETE` need the header `X-Api-Token`. |

## `wifi`

| Key | Default | Applied |
| --- | --- | --- |
| `mode` | `ap` | `ap` or `apsta`. Reboot. |
| `ap_ssid` | `RF-Tracker` | 1–32 characters. Reboot. |
| `ap_password` | `change-me` | Empty opens the network. 1–7 characters are rejected. Reboot. |
| `hostname` | `rf-tracker` | Reboot. |
| `ap_ip` | `192.168.4.1` | Reboot. The mask is `/24`. |
| `ap_channel` | `6` | 1–13. Reboot. |
| `ap_max_clients` | `4` | 1–8. Reboot. |
| `sta_ssid` / `sta_password` | empty | Used only when `mode` is `apsta`. Reboot. |

Change `change-me` before you take the board outside the bench.

## `gps`, `oled`, `storage`, `debug`

| Key | Default | Applied |
| --- | --- | --- |
| `gps.enabled` | `true` | Live. |
| `gps.baud` | `9600` | 4800, 9600, 19200, 38400, 57600 or 115200. Reboot is not required; the GPS task reopens UART2. |
| `oled.enabled` | `true` | Live. |
| `oled.i2c_address` | `60` (`0x3C`) | Decimal. Reboot, because the driver is already started. |
| `oled.refresh_ms` | `2000` | 500–10000. Live. |
| `storage.enabled` | `true` | Next mount. |
| `storage.log_prefix` | `rf_log` | Letters, digits, `_`, `-`, up to 12. Next file. |
| `storage.include_crc_column` | `false` | Adds `crc_ok` on files created after the change. Do not turn it on in the middle of an existing daily file. |
| `storage.queue_depth` | `32` | 4–64. Reboot. |
| `storage.flush_every` | `1` | 1–20 rows. Live. |
| `storage.cs_pin` | `15` | Reboot. |
| `debug.serial_packets` | `true` | Print each CSV row on USB serial. Live. |

## `radios`

Two entries, index 0 = 433 MHz reference, index 1 = 868 MHz reference. `kMaxRadios` in `include/app_types.h` is the compile-time limit.

| Key | 433 default | 868 default | Applied |
| --- | --- | --- | --- |
| `id` | `433` | `868` | Live. Used in logs and on the OLED. |
| `enabled` | `true` | `true` | Live. |
| `frequency_mhz` | `433.92` | `868.30` | Live, must sit inside min/max. |
| `freq_min_mhz` / `freq_max_mhz` | 433.05–434.79 | 863.00–870.00 | Live. Widen only when the module and local rules allow it. |
| `bitrate_kbps` | `4.80` | `4.80` | Live. 0.6–600. |
| `deviation_khz` | `5.00` | `5.00` | Live. RadioLib's CC1101 FSK default. |
| `rx_bandwidth_khz` | `135.00` | `135.00` | Live. |
| `preamble_bits` | `16` | `16` | Live. 16–192, multiple of what the CC1101 accepts. |
| `sync_word` | `D391` | `D391` | Live. Four hex digits, optional `0x`. |
| `variable_length` | `true` | `true` | Live. First byte is the length. |
| `fixed_length` | `16` | `16` | Used when variable length is off. 1–64. |
| `cs_pin` / `gdo0_pin` / `gdo2_pin` | 5 / 25 / -1 | 4 / 26 / -1 | Reboot. `-1` leaves GDO2 unconnected. |

Pins must be unique, in 0–33, and not GPIO 6–11 or a shared bus pin from `board_config.h`.

A CC1101 in packet mode only delivers frames whose frequency, bit rate, deviation and sync word match. Point those four at the protocol you want to study. This build does not sweep the band and does not transmit.

# Software

The firmware is an Arduino / FreeRTOS application built with PlatformIO (`platformio.ini`). Version `0.1.0` is printed on USB serial and returned by `GET /api/status`.

## Tasks

| Task | Core | Job |
| --- | --- | --- |
| Async web server | 0 (AsyncTCP) | Access point UI and JSON API |
| `oled` | 0 | Redraw the SSD1306 every `oled.refresh_ms` (default 2 s) |
| `radio` | 1 | Apply modem settings, read CC1101 FIFOs, push `RfRecord`s |
| `gps` | 1 | Parse NMEA on UART2 with TinyGPS++ |
| `sdlog` | 1 | Pop the queue and append CSV |
| `loop` | 1 | USB serial commands (`help`, `status`, `reload`, `reboot`) |

GDO0 interrupts do not touch SPI. Each ISR sets a flag; the radio task clears it and calls RadioLib `readData()`. Doing the SPI transfer inside the ISR can deadlock the SPI driver and would also block the other CC1101. A slow SD write cannot stall that read: the record is copied into a FreeRTOS queue and the writer task owns the card.

If the queue is full the packet is still kept in the RAM live table and `dropped` increments. CRC failures are counted per radio and are not logged, unless `storage.include_crc_column` is on.

## Boot

1. Mount LittleFS. If `/config.json` is missing or invalid, write the defaults from `defaultAppConfig()`.
2. Start the access point from the config.
3. Serve `/` from LittleFS, or a short EN/IT page if `uploadfs` has not been run.
4. Start the tasks above. A missing OLED, GPS, SD card or radio does not stop the others. The failing piece retries or reports an error string on `/api/status`.

## Web API

All bodies are JSON. Mutating routes check `X-Api-Token` when `api_token` is not empty. On a pure access point the token is visible to anyone who can open the UI; set it when `wifi.mode` is `apsta`.

| Method | Path | Effect |
| --- | --- | --- |
| GET | `/api/status` | Uptime, GPS, per-radio counters, SD queue |
| GET | `/api/packets` | Last packets kept in RAM, newest first |
| GET | `/api/config` | Current config, pretty-printed |
| POST | `/api/config` | Validate, save, live-reload radios |
| GET | `/api/logs` | CSV names on the SD card |
| GET | `/api/logs/download?name=` | Download one CSV |
| DELETE | `/api/logs?name=` | Delete one CSV |
| POST | `/api/system/reboot` | Restart after the response is sent |

`POST /api/config` answers `reboot_required` when Wi-Fi, the OLED address, the queue depth or any pin changed. Frequency, modem and GPS baud are applied by their tasks without a reboot.

Unknown JSON keys are ignored. Missing keys keep the built-in default, so a small file can override one radio.

## CSV

Row layout is `src/csv_format.h`, covered by `test/test_csv_format.cpp`. The header is exactly:

```text
timestamp_utc,latitudine,longitudine,satelliti,frequenza_mhz,rssi_dbm,lunghezza_byte,payload_hex
```

`payload_hex` is two characters per payload byte. `lunghezza_byte` is that byte count. Empty GPS columns mean "unknown", not zero. See [../sql/README.md](../sql/README.md).

## Files

| Path | Role |
| --- | --- |
| `include/board_config.h` | Shared-bus pins |
| `include/app_types.h` | `AppConfig`, `RfRecord`, radio slot count |
| `src/config_store.cpp` | Load, validate and save JSON |
| `src/radio_service.cpp` | Both CC1101 instances |
| `src/gps_service.cpp` | NMEA |
| `src/sd_logger.cpp` | Queue to CSV |
| `src/oled_ui.cpp` | Status display |
| `src/web_server.cpp` | HTTP API and static files |
| `src/wifi_portal.cpp` | AP / optional STA |
| `data/` | LittleFS image |

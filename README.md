# RF-Tracker

Receive-only logger for the 433 MHz and 868 MHz ISM bands. An ESP32 reads two CC1101 radios, stamps each packet with a GPS fix, draws a short status on an OLED, appends a CSV row to a MicroSD card, and serves a phone-friendly web UI from its own access point.

The firmware does not transmit. Carrier, bit rate, deviation, bandwidth and sync word are configurable because a CC1101 only delivers a payload when those match the sender. It is not a scanner that sweeps arbitrary modulations.

## Hardware

Reference wiring is in [docs/hardware.md](docs/hardware.md). Shared bus pins live in `include/board_config.h`. Chip-select, GDO and radio settings live in `data/config.json` and can be changed from the web UI.

## Build

Requirements: [PlatformIO](https://platformio.org/) with the `espressif32` platform.

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio run -e esp32dev -t uploadfs
```

`upload` writes the firmware. `uploadfs` writes the LittleFS image (web UI, locales and the default config). Without the filesystem image the device still boots and logs, and the access point shows how to upload it.

The host-side CSV contract test does not need a board:

```bash
g++ -std=c++17 -Iinclude -Isrc test/test_csv_format.cpp -o /tmp/test_csv_format
/tmp/test_csv_format
```

## First boot

1. Join the access point `RF-Tracker`. The default password is `change-me`.
2. Open `http://192.168.4.1/`.
3. Choose English or Italiano, change the password, and save.
4. Wi-Fi and pin changes start working after **Reboot**.

USB serial is 115200 baud. Type `help` for `status`, `reload` and `reboot`.

## Logs

CSV files are named `rf_log_YYYYMMDD.csv`. Download them from the web UI. Column names match `sql/schema.sql`. Import notes, in both languages, are in [sql/README.md](sql/README.md).

## Where to change things

| You want to… | Edit |
| --- | --- |
| Move SCK / MOSI / MISO / I2C / GPS UART | `include/board_config.h` |
| Move CS, GDO, frequency, modem, Wi-Fi | `data/config.json` or the web UI |
| Change a screen label | `data/locales/en.json` and `data/locales/it.json` |
| Add a third radio or another display controller | [docs/extending.md](docs/extending.md) |

More detail: [docs/software.md](docs/software.md), [docs/configuration.md](docs/configuration.md), [docs/bringup.md](docs/bringup.md).

Italian overview: [README.it.md](README.it.md).

## Rules

Use the bands and power limits that apply where you are. This project only receives. Do not add a transmitter unless you are allowed to use that frequency.

## License

MIT. See [LICENSE](LICENSE).

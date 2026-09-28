# Bring-up

Build each stage on the bench before you stack the next one. Serial is 115200 8N1. `status` prints GPS, both radios and the SD counters.

```bash
pio run -e esp32dev -t upload
pio run -e esp32dev -t uploadfs
pio device monitor -b 115200
```

## 1. Radios only

Wire VSPI and the two CC1101 modules. Leave GPS, OLED and SD unplugged. Boot. You should see:

```text
[radio] 433 433.92 MHz listening
[radio] 868 868.30 MHz listening
```

`begin_<code>` is a RadioLib error. Negative codes are in the RadioLib `TypeDef.h` list (`RADIOLIB_ERR_*`). Check 3.3 V, CS and GDO0 before changing the modem.

A matching transmitter (same frequency, 4.8 kbps, 5 kHz deviation, sync `D391`) should print a CSV row when `debug.serial_packets` is true. No row and a rising `crc` count means the sync matched but the body did not. No count at all means the modem or the sync word does not match the sender.

## 2. GPS and OLED

Add UART2 and the I2C display. `status` moves from `no-fix` to `fix` once the antenna sees the sky. Indoors this can take several minutes or never happen; the logger still runs and writes `rf_log_unsynced.csv`.

`[oled] begin failed` means the address or the controller is wrong. Try `oled.i2c_address` 61 (`0x3D`) or the SH1106 constructor in [extending.md](extending.md).

## 3. Queue and SD

Add the MicroSD on HSPI. `[sd] mounted` should appear. If the ESP32 no longer boots, GPIO 12 is being pulled high: see [hardware.md](hardware.md).

After a packet, the card should contain `rf_log_YYYYMMDD.csv` with the header from `src/csv_format.h`. `dropped` climbing means the card is slower than the burst; raise `storage.queue_depth` (reboot) before you change the radios.

## 4. Access point

Join `RF-Tracker` / `change-me` and open `http://192.168.4.1/`. Status cards and the live table should match `status` on serial. Change the AP password and reboot from the page.

If the page is the short "uploadfs" notice, the filesystem image is missing. Run `pio run -t uploadfs` again. The API still answers at `/api/status`.

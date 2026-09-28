# Extending

Two layers stay separate on purpose.

- PCB buses (VSPI, HSPI, I2C, UART) are constants in `include/board_config.h`. Moving them is a new board revision: change the constants, rebuild, and update [hardware.md](hardware.md).
- Things you retune in the field (frequency, modem, sync word, CS, GDO, Wi-Fi, language) are keys in `data/config.json`. Add a key in `AppConfig`, `defaultAppConfig()`, `applyConfigJson()`, `configToJson()` and `validateAppConfig()`, then add a control in `data/index.html` / `data/app.js` and a string in both locale files.

Keep the CSV header stable. Consumers in `sql/schema.sql` depend on those names. A new column belongs behind a flag, the way `storage.include_crc_column` adds `crc_ok` only when asked.

## Another CC1101

`kMaxRadios` is 2. To add a slot:

1. Raise `kMaxRadios` in `include/app_types.h`.
2. In `src/radio_service.cpp`, add `radioIsrThunk<2>` to the table and update the `static_assert`.
3. Give the new entry its own CS and GDO. It still shares VSPI. Do not put a third radio on the SD bus.
4. Add a default in `fillRadio()` / `defaultAppConfig()` and a third object in `data/config.json`.
5. The OLED currently prints radios `[0]` and `[1]`. Extend `src/oled_ui.cpp` if the third should be on screen.

The web UI already loops over `config.radios`.

## Another OLED controller

`src/oled_ui.cpp` builds `U8G2_SSD1306_128X64_NONAME_F_HW_I2C`. For a SH1106 128x64, switch the type to `U8G2_SH1106_128X64_NONAME_F_HW_I2C` and keep `setI2CAddress`. U8g2's wiki lists the constructor for other panels. The I2C pins stay in `board_config.h`.

## OTA

`partitions.csv` uses one 1.75 MB app slot so the web UI can have about 2.2 MB. The current image is about 995 KB. To add an OTA slot, shrink `littlefs` and add `app1`, then set `board_upload.flash_size` only if the module is not 4 MB. The web files have to fit in what remains.

## Another language

Copy `data/locales/en.json` to `data/locales/<code>.json` with the same keys. Allow the code in `validateAppConfig()` (`language` is `en` or `it` today) and add an `<option>` in `data/index.html`.

## What this tree will not grow into

There is no transmit path, no replay and no roll-jam helper. A CC1101 can transmit; this firmware does not call it. If you add transmission, you are responsible for the band plan where the device is used. Keep that code behind a separate, obvious function so a receive-only build can leave it out.

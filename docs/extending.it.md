# Estensioni

Due livelli restano separati di proposito.

- I bus della scheda (VSPI, HSPI, I2C, UART) sono costanti in `include/board_config.h`. Spostarli è una nuova revisione hardware: cambia le costanti, ricompila e aggiorna [hardware.it.md](hardware.it.md).
- Ciò che si ritocca sul campo (frequenza, modem, sync word, CS, GDO, Wi-Fi, lingua) sono chiavi in `data/config.json`. Aggiungi una chiave in `AppConfig`, `defaultAppConfig()`, `applyConfigJson()`, `configToJson()` e `validateAppConfig()`, poi un controllo in `data/index.html` / `data/app.js` e una stringa in entrambi i file di lingua.

Tieni stabile l'intestazione CSV. Chi importa `sql/schema.sql` dipende da quei nomi. Una colonna nuova sta dietro un flag, come `storage.include_crc_column` che aggiunge `crc_ok` solo quando viene chiesto.

## Un altro CC1101

`kMaxRadios` è 2. Per aggiungere uno slot:

1. Alza `kMaxRadios` in `include/app_types.h`.
2. In `src/radio_service.cpp` aggiungi `radioIsrThunk<2>` alla tabella e aggiorna lo `static_assert`.
3. Dai al nuovo ingresso CS e GDO propri. Condivide comunque VSPI. Non mettere una terza radio sul bus della SD.
4. Aggiungi un default in `fillRadio()` / `defaultAppConfig()` e un terzo oggetto in `data/config.json`.
5. L'OLED oggi stampa le radio `[0]` e `[1]`. Estendi `src/oled_ui.cpp` se anche la terza deve comparire.

L'interfaccia web scorre già `config.radios`.

## Un altro controller OLED

`src/oled_ui.cpp` costruisce `U8G2_SSD1306_128X64_NONAME_F_HW_I2C`. Per un SH1106 128x64 passa a `U8G2_SH1106_128X64_NONAME_F_HW_I2C` e tieni `setI2CAddress`. La wiki di U8g2 elenca il costruttore degli altri pannelli. I pin I2C restano in `board_config.h`.

## OTA

`partitions.csv` usa uno slot applicazione da 1,75 MB così l'interfaccia ha circa 2,2 MB. L'immagine attuale è circa 995 KB. Per aggiungere uno slot OTA riduci `littlefs` e aggiungi `app1`, poi imposta `board_upload.flash_size` solo se il modulo non è da 4 MB. I file web devono entrare nello spazio che resta.

## Un'altra lingua

Copia `data/locales/en.json` in `data/locales/<codice>.json` con le stesse chiavi. Consenti il codice in `validateAppConfig()` (oggi `language` è `en` o `it`) e aggiungi un `<option>` in `data/index.html`.

## Cosa questo albero non deve diventare

Non c'è un percorso di trasmissione, né un replay, né un aiuto per il roll-jam. Un CC1101 può trasmettere; questo firmware non lo chiama. Se aggiungi la trasmissione, sei responsabile del piano di banda del posto in cui usi il dispositivo. Tieni quel codice dietro una funzione separata e evidente, così una build solo ricezione può lasciarlo fuori.

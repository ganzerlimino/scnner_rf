# Software

Il firmware è un'applicazione Arduino / FreeRTOS costruita con PlatformIO (`platformio.ini`). La versione `0.1.0` compare sulla seriale USB e in `GET /api/status`.

## Task

| Task | Core | Compito |
| --- | --- | --- |
| Server web asincrono | 0 (AsyncTCP) | Interfaccia dell'access point e API JSON |
| `oled` | 0 | Ridisegna l'SSD1306 ogni `oled.refresh_ms` (default 2 s) |
| `radio` | 1 | Applica il modem, legge le FIFO dei CC1101, accoda gli `RfRecord` |
| `gps` | 1 | Interpreta l'NMEA su UART2 con TinyGPS++ |
| `sdlog` | 1 | Svuota la coda e aggiunge il CSV |
| `loop` | 1 | Comandi sulla seriale USB (`help`, `status`, `reload`, `reboot`) |

Gli interrupt su GDO0 non usano SPI. Ogni ISR alza un flag; il task radio lo abbassa e chiama `readData()` di RadioLib. Fare il trasferimento SPI dentro l'ISR può bloccare il driver SPI e anche l'altro CC1101. Una scrittura lenta sulla SD non ferma quella lettura: il record viene copiato in una coda FreeRTOS e il task di scrittura possiede la scheda.

Se la coda è piena il pacchetto resta nella tabella in RAM e `dropped` aumenta. Gli errori CRC sono contati per radio e non finiscono nel log, salvo che `storage.include_crc_column` sia attivo.

## Avvio

1. Monta LittleFS. Se `/config.json` manca o non è valido, scrive i default di `defaultAppConfig()`.
2. Avvia l'access point dalla configurazione.
3. Serve `/` da LittleFS, oppure una pagina breve IT/EN se `uploadfs` non è stato eseguito.
4. Avvia i task sopra. OLED, GPS, SD o radio assenti non fermano gli altri. Il pezzo che fallisce riprova oppure riporta una stringa di errore su `/api/status`.

## API web

I body sono JSON. Le route che modificano qualcosa controllano `X-Api-Token` quando `api_token` non è vuoto. Su un access point puro il token è visibile a chi apre l'interfaccia; impostalo quando `wifi.mode` è `apsta`.

| Metodo | Percorso | Effetto |
| --- | --- | --- |
| GET | `/api/status` | Uptime, GPS, contatori per radio, coda SD |
| GET | `/api/packets` | Ultimi pacchetti in RAM, dal più recente |
| GET | `/api/config` | Configurazione attuale, formattata |
| POST | `/api/config` | Valida, salva, ricarica le radio subito |
| GET | `/api/logs` | Nomi dei CSV sulla SD |
| GET | `/api/logs/download?name=` | Scarica un CSV |
| DELETE | `/api/logs?name=` | Cancella un CSV |
| POST | `/api/system/reboot` | Riavvia dopo la risposta |

`POST /api/config` risponde `reboot_required` quando cambiano Wi-Fi, indirizzo OLED, profondità della coda o un pin. Frequenza, modem e baud del GPS li applicano i rispettivi task senza riavvio.

Le chiavi JSON sconosciute sono ignorate. Le chiavi assenti tengono il default interno, quindi un file piccolo può sovrascrivere una sola radio.

## CSV

Il formato di riga è `src/csv_format.h`, coperto da `test/test_csv_format.cpp`. L'intestazione è esattamente:

```text
timestamp_utc,latitudine,longitudine,satelliti,frequenza_mhz,rssi_dbm,lunghezza_byte,payload_hex
```

`payload_hex` ha due caratteri per byte di payload. `lunghezza_byte` è quel numero di byte. Le colonne GPS vuote significano "sconosciuto", non zero. Vedi [../sql/README.md](../sql/README.md).

## File

| Percorso | Ruolo |
| --- | --- |
| `include/board_config.h` | Pin dei bus condivisi |
| `include/app_types.h` | `AppConfig`, `RfRecord`, numero di radio |
| `src/config_store.cpp` | Carica, valida e salva il JSON |
| `src/radio_service.cpp` | Entrambe le istanze CC1101 |
| `src/gps_service.cpp` | NMEA |
| `src/sd_logger.cpp` | Coda verso CSV |
| `src/oled_ui.cpp` | Display di stato |
| `src/web_server.cpp` | API HTTP e file statici |
| `src/wifi_portal.cpp` | AP / STA opzionale |
| `data/` | Immagine LittleFS |

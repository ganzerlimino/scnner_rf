# Configurazione

`data/config.json` è il file scritto da `pio run -t uploadfs`. A runtime lo stesso documento sta in `/config.json` su LittleFS. L'interfaccia modifica quella copia. Le chiavi restano in inglese così un solo file è la fonte; le etichette sono tradotte in `data/locales/en.json` e `data/locales/it.json`.

`_readme_en` e `_readme_it` sono promemoria. Il firmware ignora le chiavi sconosciute, incluse quelle due.

## Livello principale

| Chiave | Default | Note |
| --- | --- | --- |
| `schema` | `1` | Versione del documento. Il loader accetta comunque le chiavi note se questo numero cambia. |
| `language` | `en` | `en` o `it`. Lingua dell'interfaccia. |
| `ui.theme` | `field` | Id di una combinazione in `data/themes.json`. Vale appena la pagina si carica. Da 1 a 15 caratteri: una lettera minuscola, poi lettere, cifre, `_` o `-`. |
| `api_token` | vuoto | Se impostato, `POST` e `DELETE` richiedono l'header `X-Api-Token`. |

## `wifi`

| Chiave | Default | Quando vale |
| --- | --- | --- |
| `mode` | `ap` | `ap` o `apsta`. Riavvio. |
| `ap_ssid` | `RF-Tracker` | 1–32 caratteri. Riavvio. |
| `ap_password` | `change-me` | Vuota apre la rete. 1–7 caratteri vengono rifiutati. Riavvio. |
| `hostname` | `rf-tracker` | Riavvio. |
| `ap_ip` | `192.168.4.1` | Riavvio. La maschera è `/24`. |
| `ap_channel` | `6` | 1–13. Riavvio. |
| `ap_max_clients` | `4` | 1–8. Riavvio. |
| `sta_ssid` / `sta_password` | vuoti | Usati solo se `mode` è `apsta`. Riavvio. |

Cambia `change-me` prima di portare la scheda fuori dal banco.

## `gps`, `oled`, `storage`, `debug`

| Chiave | Default | Quando vale |
| --- | --- | --- |
| `gps.enabled` | `true` | Subito. |
| `gps.baud` | `9600` | 4800, 9600, 19200, 38400, 57600 o 115200. Il task GPS riapre UART2, senza riavvio. |
| `oled.enabled` | `true` | Subito. |
| `oled.i2c_address` | `60` (`0x3C`) | Decimale. Riavvio, perché il driver è già partito. |
| `oled.refresh_ms` | `2000` | 500–10000. Subito. |
| `storage.enabled` | `true` | Al mount successivo. |
| `storage.log_prefix` | `rf_log` | Lettere, cifre, `_`, `-`, fino a 12. File successivo. |
| `storage.include_crc_column` | `false` | Aggiunge `crc_ok` sui file creati dopo il cambio. Non attivarla a metà di un file giornaliero già aperto. |
| `storage.queue_depth` | `32` | 4–64. Riavvio. |
| `storage.flush_every` | `1` | 1–20 righe. Subito. |
| `storage.cs_pin` | `15` | Riavvio. |
| `debug.serial_packets` | `true` | Stampa ogni riga CSV sulla seriale USB. Subito. |

## `radios`

Due voci: indice 0 = riferimento 433 MHz, indice 1 = riferimento 868 MHz. `kMaxRadios` in `include/app_types.h` è il limite di compilazione.

| Chiave | Default 433 | Default 868 | Quando vale |
| --- | --- | --- | --- |
| `id` | `433` | `868` | Subito. Compare nei log e sull'OLED. |
| `enabled` | `true` | `true` | Subito. |
| `frequency_mhz` | `433.92` | `868.30` | Subito, dentro min/max. |
| `freq_min_mhz` / `freq_max_mhz` | 433.05–434.79 | 863.00–870.00 | Subito. Allarga solo se il modulo e le regole locali lo permettono. |
| `bitrate_kbps` | `4.80` | `4.80` | Subito. 0.6–600. |
| `deviation_khz` | `5.00` | `5.00` | Subito. Default FSK di RadioLib per il CC1101. |
| `rx_bandwidth_khz` | `135.00` | `135.00` | Subito. |
| `preamble_bits` | `16` | `16` | Subito. 16–192. |
| `sync_word` | `D391` | `D391` | Subito. Quattro cifre hex, `0x` opzionale. |
| `variable_length` | `true` | `true` | Subito. Il primo byte è la lunghezza. |
| `fixed_length` | `16` | `16` | Usata se la lunghezza variabile è spenta. 1–64. |
| `cs_pin` / `gdo0_pin` / `gdo2_pin` | 5 / 25 / -1 | 4 / 26 / -1 | Riavvio. `-1` lascia GDO2 scollegato. |

I pin devono essere unici, tra 0 e 33, e non GPIO 6–11 né un pin di bus condiviso in `board_config.h`.

Un CC1101 in modo pacchetto consegna solo le trame la cui frequenza, bit rate, deviazione e sync word coincidono. Punta quei quattro valori al protocollo che vuoi studiare. Questa build non spazza la banda e non trasmette.

## Aspetto

Le combinazioni incluse sono `field` (carbone e ambra), `night` (blu e ciano), `paper` (chiara), `olive` (verde) e `signal` (nero e lime). I colori stanno in `data/themes.json`, non nel firmware. La configurazione memorizza solo l'id in `ui.theme`.

Nella pagina, **Aspetto** applica la combinazione subito. **Salva** scrive quell'id insieme al resto. Non serve riavviare.

## Esporta e importa

**Esporta configurazione** scarica il modulo, compresa una modifica non ancora salvata, come `rf-tracker-config.json`. **Importa configurazione** rilegge un JSON nel modulo. **Salva** lo scrive in `/config.json` su LittleFS. L'import da solo non scrive, così puoi controllare i valori prima.

Il file è lo stesso documento di `data/config.json`. Le chiavi restano in inglese. Servono un oggetto `wifi` e un array `radios`; le altre chiavi note sono facoltative e tornano ai default. Le chiavi sconosciute sono ignorate. `_readme_en` e `_readme_it` possono esserci e vengono ignorati.

Per caricare un file senza la pagina, sostituisci `data/config.json` e lancia `pio run -e esp32dev -t uploadfs`, oppure copialo in `/config.json` su LittleFS. Un file non valido viene rifiutato e resta la configurazione precedente.

Come aggiungere una combinazione è descritto in [extending.it.md](extending.it.md).

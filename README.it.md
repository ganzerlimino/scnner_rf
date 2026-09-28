# RF-Tracker

Logger solo in ricezione per le bande ISM a 433 MHz e 868 MHz. Un ESP32 legge due radio CC1101, marca ogni pacchetto con il fix GPS, mostra uno stato breve su OLED, aggiunge una riga CSV su MicroSD e serve un'interfaccia web per telefono dal proprio access point.

Il firmware non trasmette. Portante, bit rate, deviazione, banda e sync word si configurano perché il CC1101 consegna un payload solo quando coincidono con il trasmettitore. Non è uno scanner che spazza modulazioni arbitrarie.

## Hardware

I collegamenti di riferimento sono in [docs/hardware.it.md](docs/hardware.it.md). I pin dei bus condivisi stanno in `include/board_config.h`. Chip-select, GDO e parametri radio stanno in `data/config.json` e si cambiano anche dall'interfaccia web.

## Compilazione

Serve [PlatformIO](https://platformio.org/) con la piattaforma `espressif32`.

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio run -e esp32dev -t uploadfs
```

`upload` scrive il firmware. `uploadfs` scrive l'immagine LittleFS (interfaccia, lingue e configurazione di default). Senza filesystem il dispositivo si avvia e registra comunque, e l'access point spiega come caricarlo.

La prova del contratto CSV non ha bisogno della scheda:

```bash
g++ -std=c++17 -Iinclude -Isrc test/test_csv_format.cpp -o /tmp/test_csv_format
/tmp/test_csv_format
```

## Primo avvio

1. Collegati all'access point `RF-Tracker`. La password di default è `change-me`.
2. Apri `http://192.168.4.1/`.
3. Scegli English o Italiano, cambia la password e salva.
4. Wi-Fi e pin nuovi partono dopo **Riavvia**.

La seriale USB è a 115200 baud. Scrivi `help` per `status`, `reload` e `reboot`.

## Log

I CSV si chiamano `rf_log_YYYYMMDD.csv`. Si scaricano dall'interfaccia. I nomi colonna coincidono con `sql/schema.sql`. Le note di import, in entrambe le lingue, sono in [sql/README.md](sql/README.md).

## Dove si modifica

| Vuoi… | Modifica |
| --- | --- |
| Spostare SCK / MOSI / MISO / I2C / UART del GPS | `include/board_config.h` |
| Spostare CS, GDO, frequenza, modem, Wi-Fi | `data/config.json` o l'interfaccia |
| Cambiare un'etichetta a schermo | `data/locales/en.json` e `data/locales/it.json` |
| Aggiungere una terza radio o un altro display | [docs/extending.it.md](docs/extending.it.md) |

Altri dettagli: [docs/software.it.md](docs/software.it.md), [docs/configuration.it.md](docs/configuration.it.md), [docs/bringup.it.md](docs/bringup.it.md).

Panoramica in inglese: [README.md](README.md).

## Regole

Usa le bande e i limiti di potenza del posto in cui ti trovi. Questo progetto riceve soltanto. Non aggiungere un trasmettitore se non puoi usare quella frequenza.

## Licenza

MIT. Vedi [LICENSE](LICENSE).

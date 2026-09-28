# Hardware

Scheda di riferimento: ESP32 Dev Module, due moduli CC1101 (uno a 433 MHz, uno a 868 MHz), MicroSD su SPI, OLED SSD1306 128x64 su I2C e un GPS UART (classe NEO-6M / NEO-M8, 9600 baud di default).

Tutta la logica è a 3,3 V. Metti un condensatore ceramico da 100 nF e uno elettrolitico da 10 µF in parallelo tra VCC e GND di ogni CC1101, vicini al modulo. I picchi del Wi-Fi fanno calare i 3,3 V e un CC1101 alimentato male corrompe la FIFO.

I moduli 433 MHz e 868 MHz hanno reti di adattamento diverse. Tieni ciascuno dentro la banda indicata da `freq_min_mhz` / `freq_max_mhz`. Scambiare i due moduli non fa ricevere gli 868 MHz a una scheda da 433.

## Perché i bus sono separati

| Bus | Pin | Dispositivi | Motivo |
| --- | --- | --- | --- |
| VSPI | 18 SCK, 19 MISO, 23 MOSI | entrambi i CC1101 | La lettura del pacchetto resta corta e non aspetta la SD |
| HSPI | 14 SCK, 12 MISO, 13 MOSI, 15 CS | MicroSD | La scrittura sulla scheda può fermarsi per millisecondi |
| I2C | 21 SDA, 22 SCL | OLED | Solo stato, aggiornato ogni 2 s |
| UART2 | 16 RX, 17 TX | GPS | Il pin RX dell'ESP32 ascolta il TX del GPS |

Chip-select e GDO sono per radio, in `data/config.json`:

| Radio | CS | GDO0 | GDO2 |
| --- | --- | --- | --- |
| 433 MHz | GPIO 5 | GPIO 25 | non collegato (`-1`) |
| 868 MHz | GPIO 4 | GPIO 26 | non collegato (`-1`) |

GDO0 è l'interrupt di pacchetto. GDO2 è opzionale; lascialo a `-1` se il protocollo non usa quella linea.

I pin dei bus condivisi sono valori di compilazione in `include/board_config.h`. CS, GDO e il chip-select della SD si spostano dall'interfaccia web; valgono dopo il riavvio.

## Pin di strapping

GPIO 12 è MISO di HSPI ed è un pin di strapping. Se è alto al reset, molti moduli ESP32 si aspettano la flash a 1,8 V e non partono. I moduli SD spesso tirano MISO verso l'alto.

- Preferisci un modulo SD che non tiri MISO alto durante il reset, oppure
- aggiungi verso massa una resistenza più forte del pull-up del modulo durante il reset, e verifica che la scheda si legga ancora, oppure
- sposta HSPI in `include/board_config.h` su pin che non sono di strapping.

Anche GPIO 15 (CS della SD) e GPIO 5 (CS dei 433) vengono letti all'avvio. Tieni i pull-up esterni modesti. GPIO 0 deve restare alto per eseguire l'applicazione; non usarlo come chip-select se non conosci lo stato al reset.

Non usare GPIO 6–11. Sono collegati alla flash SPI.

## Alimentazione e GPS

La porta USB della dev board basta in laboratorio. In una build portatile dimensiona il regolatore per i picchi del Wi-Fi (circa 400–500 mA) più le due radio e il GPS. Non portare 5 V sul pin VCC di un CC1101.

Il TX del GPS va al GPIO 16. Il RX del GPS va al GPIO 17. La maggior parte dei moduli parla già UART a 3,3 V; un GPS a 5 V ha bisogno di un level shifter sul pin RX dell'ESP32.

## OLED

Il firmware costruisce `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` all'indirizzo `0x3C` (60 decimale in `config.json`). Un'altra famiglia risponde a `0x3D` (61 decimale). Cambia `oled.i2c_address` e riavvia.

Un SH1106 128x64 ha lo stesso connettore e un controller diverso. Vedi [extending.it.md](extending.it.md).

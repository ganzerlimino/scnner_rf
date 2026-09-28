# Messa in servizio

Costruisci ogni fase sul banco prima di aggiungere la successiva. La seriale è 115200 8N1. `status` stampa GPS, entrambe le radio e i contatori della SD.

```bash
pio run -e esp32dev -t upload
pio run -e esp32dev -t uploadfs
pio device monitor -b 115200
```

## 1. Solo le radio

Collega VSPI e i due CC1101. Lascia staccati GPS, OLED e SD. All'avvio devi vedere:

```text
[radio] 433 433.92 MHz listening
[radio] 868 868.30 MHz listening
```

`begin_<codice>` è un errore di RadioLib. I codici negativi sono nell'elenco `RADIOLIB_ERR_*` di `TypeDef.h`. Controlla 3,3 V, CS e GDO0 prima di cambiare il modem.

Un trasmettitore compatibile (stessa frequenza, 4,8 kbps, deviazione 5 kHz, sync `D391`) deve stampare una riga CSV se `debug.serial_packets` è true. Nessuna riga e un conteggio `crc` che sale significa che la sync coincide ma il corpo no. Nessun conteggio significa che modem o sync word non coincidono con il trasmettitore.

## 2. GPS e OLED

Aggiungi UART2 e il display I2C. `status` passa da `no-fix` a `fix` quando l'antenna vede il cielo. Al chiuso può volerci qualche minuto, o non succedere; il logger continua e scrive `rf_log_unsynced.csv`.

`[oled] begin failed` significa indirizzo o controller sbagliati. Prova `oled.i2c_address` 61 (`0x3D`) o il costruttore SH1106 in [extending.it.md](extending.it.md).

## 3. Coda e SD

Aggiungi la MicroSD su HSPI. Deve comparire `[sd] mounted`. Se l'ESP32 non si avvia più, GPIO 12 è tenuto alto: vedi [hardware.it.md](hardware.it.md).

Dopo un pacchetto la scheda deve contenere `rf_log_YYYYMMDD.csv` con l'intestazione di `src/csv_format.h`. Se `dropped` sale, la scheda è più lenta del burst; alza `storage.queue_depth` (riavvio) prima di toccare le radio.

## 4. Access point

Collegati a `RF-Tracker` / `change-me` e apri `http://192.168.4.1/`. Le schede di stato e la tabella in diretta devono coincidere con `status` sulla seriale. Cambia la password dell'AP e riavvia dalla pagina.

Se la pagina è l'avviso breve su `uploadfs`, manca l'immagine del filesystem. Esegui di nuovo `pio run -t uploadfs`. L'API risponde comunque su `/api/status`.

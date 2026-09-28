# Importing logs / Importare i log

The firmware writes one CSV per UTC day, for example `rf_log_20261027.csv`.
Rows captured before the GPS has a date go to `rf_log_unsynced.csv`.

Il firmware scrive un CSV per giorno UTC, per esempio `rf_log_20261027.csv`.
Le righe catturate prima della data GPS finiscono in `rf_log_unsynced.csv`.

## Columns / Colonne

| CSV header | Meaning | Significato |
| --- | --- | --- |
| `timestamp_utc` | GPS UTC time, empty when unknown | Ora UTC del GPS, vuota se sconosciuta |
| `latitudine` | WGS84 latitude, empty without a fix | Latitudine WGS84, vuota senza fix |
| `longitudine` | WGS84 longitude, empty without a fix | Longitudine WGS84, vuota senza fix |
| `satelliti` | Satellites used for that fix | Satelliti usati per il fix |
| `frequenza_mhz` | Configured center frequency | Frequenza centrale configurata |
| `rssi_dbm` | RSSI of the received packet | RSSI del pacchetto ricevuto |
| `lunghezza_byte` | Payload size in bytes | Dimensione del payload in byte |
| `payload_hex` | Two hex characters per byte | Due caratteri esadecimali per byte |

`lunghezza_byte` is the real byte count. `payload_hex` is always twice that long.
The sample row in the original brief mixed a byte count with a shorter hex string; the firmware does not.

`lunghezza_byte` è il numero reale di byte. `payload_hex` è sempre lungo il doppio.
La riga di esempio nel canovaccio originale mescolava un conteggio di byte con una stringa hex più corta; il firmware non lo fa.

Empty GPS fields are left empty so they can become `NULL`. A CRC failure is counted on the device and is not written, unless you enable `storage.include_crc_column`. That adds a trailing `crc_ok` column (`1` or `0`) and needs the `ALTER TABLE` in `schema.sql`.

I campi GPS vuoti restano vuoti così possono diventare `NULL`. Un errore CRC viene contato sul dispositivo e non viene scritto, a meno di abilitare `storage.include_crc_column`. Quella opzione aggiunge in coda la colonna `crc_ok` (`1` o `0`) e richiede l'`ALTER TABLE` in `schema.sql`.

## MySQL

```sql
LOAD DATA LOCAL INFILE 'rf_log_20261027.csv'
INTO TABLE rf_captures
FIELDS TERMINATED BY ','
LINES TERMINATED BY '\n'
IGNORE 1 ROWS
(@timestamp_utc, @latitudine, @longitudine, @satelliti, frequenza_mhz, rssi_dbm, lunghezza_byte, payload_hex)
SET
  timestamp_utc = NULLIF(@timestamp_utc, ''),
  latitudine = NULLIF(@latitudine, ''),
  longitudine = NULLIF(@longitudine, ''),
  satelliti = NULLIF(@satelliti, '');
```

## SQLite

```sql
.mode csv
.import --skip 1 rf_log_20261027.csv rf_captures_raw
INSERT INTO rf_captures (
  timestamp_utc, latitudine, longitudine, satelliti,
  frequenza_mhz, rssi_dbm, lunghezza_byte, payload_hex
)
SELECT
  NULLIF(timestamp_utc, ''),
  NULLIF(latitudine, ''),
  NULLIF(longitudine, ''),
  NULLIF(satelliti, ''),
  frequenza_mhz, rssi_dbm, lunghezza_byte, payload_hex
FROM rf_captures_raw;
```

Create `rf_captures` from `schema.sql` first. SQLite ignores `AUTO_INCREMENT`; use `INTEGER PRIMARY KEY` if you create the table there.

Crea prima `rf_captures` da `schema.sql`. SQLite ignora `AUTO_INCREMENT`: usa `INTEGER PRIMARY KEY` se crei la tabella lì.

## Example questions / Domande di esempio

```sql
-- 868 MHz packets inside a rough bounding box.
-- Pacchetti a 868 MHz dentro un riquadro geografico.
SELECT *
FROM rf_captures
WHERE frequenza_mhz BETWEEN 868.00 AND 868.99
  AND latitudine BETWEEN 41.90 AND 41.91
  AND longitudine BETWEEN 12.49 AND 12.50;

-- How many times a payload was seen in one hour.
-- Quante volte un payload è stato visto in un'ora.
SELECT payload_hex, COUNT(*) AS n
FROM rf_captures
WHERE timestamp_utc >= '2026-10-27 14:00:00'
  AND timestamp_utc <  '2026-10-27 15:00:00'
GROUP BY payload_hex
ORDER BY n DESC;
```

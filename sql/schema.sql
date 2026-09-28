-- RF-Tracker capture table.
-- Column names match the CSV header written by the firmware.
--
-- Tabella delle catture RF-Tracker.
-- I nomi delle colonne coincidono con l'intestazione CSV del firmware.

CREATE TABLE rf_captures (
  id INT AUTO_INCREMENT PRIMARY KEY,
  timestamp_utc DATETIME NULL,
  latitudine DECIMAL(10, 8) NULL,
  longitudine DECIMAL(11, 8) NULL,
  satelliti INT NULL,
  frequenza_mhz DECIMAL(6, 2) NOT NULL,
  rssi_dbm INT NOT NULL,
  lunghezza_byte INT NOT NULL,
  payload_hex VARCHAR(128) NOT NULL
);

-- Optional column used only when storage.include_crc_column is true.
-- Colonna opzionale, usata solo se storage.include_crc_column è true.
-- ALTER TABLE rf_captures ADD COLUMN crc_ok TINYINT NULL;

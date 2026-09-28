#include "csv_format.h"

#include <stdio.h>
#include <string.h>

static int gFailures = 0;

static void expect(bool ok, const char* message) {
  if (!ok) {
    fprintf(stderr, "FAIL %s\n", message);
    gFailures++;
  }
}

static RfRecord sample() {
  RfRecord record{};
  strcpy(record.timestamp_utc, "2026-10-27 14:32:01");
  record.latitude = 41.902782;
  record.longitude = 12.496366;
  record.satellites = 7;
  record.flags = kFlagLocation | kFlagTime | kFlagCrcOk;
  record.frequency_mhz = 433.92f;
  record.rssi_dbm = -74;
  const uint8_t payload[] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
  record.length = sizeof(payload);
  memcpy(record.payload, payload, sizeof(payload));
  strcpy(record.band_id, "433");
  return record;
}

int main() {
  char header[160];
  csvHeader(header, sizeof(header), false);
  expect(strcmp(header,
                "timestamp_utc,latitudine,longitudine,satelliti,frequenza_mhz,rssi_dbm,"
                "lunghezza_byte,payload_hex\n") == 0,
         "default header");

  char line[256];
  formatRfCsvLine(sample(), false, line, sizeof(line));
  expect(strcmp(line, "2026-10-27 14:32:01,41.902782,12.496366,7,433.92,-74,6,A1B2C3D4E5F6\n") == 0,
         line);

  formatRfCsvLine(sample(), true, line, sizeof(line));
  expect(strcmp(line,
                "2026-10-27 14:32:01,41.902782,12.496366,7,433.92,-74,6,A1B2C3D4E5F6,1\n") == 0,
         "crc column");

  RfRecord blind = sample();
  blind.flags = kFlagCrcOk;
  blind.timestamp_utc[0] = '\0';
  formatRfCsvLine(blind, false, line, sizeof(line));
  expect(strcmp(line, ",,,,433.92,-74,6,A1B2C3D4E5F6\n") == 0, "empty gps fields");

  char name[40];
  expect(logFileName("rf_log", sample(), name, sizeof(name)) &&
             strcmp(name, "rf_log_20261027.csv") == 0,
         "dated file name");
  expect(logFileName("rf_log", blind, name, sizeof(name)) && strcmp(name, "rf_log_unsynced.csv") == 0,
         "unsynced file name");

  if (gFailures != 0) {
    fprintf(stderr, "%d failure(s)\n", gFailures);
    return 1;
  }
  puts("csv format ok");
  return 0;
}

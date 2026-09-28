#pragma once

// CSV row builder shared by the SD logger and the host-side contract test.
// No Arduino types: the column contract can be checked without an ESP32.

#include "app_types.h"

#include <stdio.h>
#include <string.h>

inline void bytesToHex(const uint8_t* data, size_t length, char* out, size_t outLen) {
  static const char kDigits[] = "0123456789ABCDEF";
  size_t pos = 0;
  if (outLen == 0) {
    return;
  }
  for (size_t i = 0; i < length && (pos + 2) < outLen; ++i) {
    out[pos++] = kDigits[(data[i] >> 4) & 0x0F];
    out[pos++] = kDigits[data[i] & 0x0F];
  }
  out[pos] = '\0';
}

inline void csvHeader(char* out, size_t outLen, bool includeCrc) {
  const char* header = includeCrc
      ? "timestamp_utc,latitudine,longitudine,satelliti,frequenza_mhz,rssi_dbm,lunghezza_byte,payload_hex,crc_ok\n"
      : "timestamp_utc,latitudine,longitudine,satelliti,frequenza_mhz,rssi_dbm,lunghezza_byte,payload_hex\n";
  snprintf(out, outLen, "%s", header);
}

// Empty GPS fields stay empty so an importer can turn them into NULL.
// lunghezza_byte is the payload size in bytes; payload_hex is two characters per byte.
inline void formatRfCsvLine(const RfRecord& record, bool includeCrc, char* out, size_t outLen) {
  char lat[20] = "";
  char lon[20] = "";
  char sats[8] = "";
  char hex[(kMaxPayload * 2) + 1];

  if ((record.flags & kFlagLocation) != 0) {
    snprintf(lat, sizeof(lat), "%.6f", record.latitude);
    snprintf(lon, sizeof(lon), "%.6f", record.longitude);
    snprintf(sats, sizeof(sats), "%u", static_cast<unsigned>(record.satellites));
  }
  bytesToHex(record.payload, record.length, hex, sizeof(hex));

  if (includeCrc) {
    snprintf(out, outLen, "%s,%s,%s,%s,%.2f,%d,%u,%s,%u\n",
             record.timestamp_utc, lat, lon, sats, static_cast<double>(record.frequency_mhz),
             static_cast<int>(record.rssi_dbm), static_cast<unsigned>(record.length), hex,
             (record.flags & kFlagCrcOk) != 0 ? 1u : 0u);
  } else {
    snprintf(out, outLen, "%s,%s,%s,%s,%.2f,%d,%u,%s\n", record.timestamp_utc, lat, lon, sats,
             static_cast<double>(record.frequency_mhz), static_cast<int>(record.rssi_dbm),
             static_cast<unsigned>(record.length), hex);
  }
}

inline bool logFileName(const char* prefix, const RfRecord& record, char* out, size_t outLen) {
  const char* safePrefix = (prefix != nullptr && prefix[0] != '\0') ? prefix : "rf_log";
  if ((record.flags & kFlagTime) != 0 && strlen(record.timestamp_utc) >= 10) {
    char day[9];
    // "YYYY-MM-DD" -> "YYYYMMDD"
    memcpy(day, record.timestamp_utc, 4);
    memcpy(day + 4, record.timestamp_utc + 5, 2);
    memcpy(day + 6, record.timestamp_utc + 8, 2);
    day[8] = '\0';
    return snprintf(out, outLen, "%s_%s.csv", safePrefix, day) > 0;
  }
  return snprintf(out, outLen, "%s_unsynced.csv", safePrefix) > 0;
}

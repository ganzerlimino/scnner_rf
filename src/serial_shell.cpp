#include "serial_shell.h"

#include "shared_state.h"

#include <Arduino.h>
#include <string.h>

namespace {

char gLine[96];
size_t gLength = 0;

void printHelp() {
  Serial.println("help    show commands");
  Serial.println("status  print GPS, radios and SD counters");
  Serial.println("reload  re-apply radio settings");
  Serial.println("reboot  restart the ESP32");
}

void printStatus() {
  const AppConfig config = gState.copyConfig();
  const GpsSnapshot gps = gState.copyGps();
  Serial.printf("version %s  lang %s\n", RF_TRACKER_VERSION, config.language);
  Serial.printf("gps %s sats=%u time=%s\n", gps.location_valid ? "fix" : "no-fix",
                static_cast<unsigned>(gps.satellites),
                gps.time_valid ? gps.timestamp_utc : "-");
  if (gps.location_valid) {
    Serial.printf("    %.6f, %.6f\n", gps.latitude, gps.longitude);
  }
  for (size_t i = 0; i < kMaxRadios; ++i) {
    Serial.printf("radio %s ready=%d pkts=%lu crc=%lu rssi=%s err=%s\n", config.radios[i].id,
                  gState.radioReady(i) ? 1 : 0, static_cast<unsigned long>(gState.packets(i)),
                  static_cast<unsigned long>(gState.crcErrors(i)),
                  gState.rssiValid(i) ? String(gState.lastRssi(i)).c_str() : "-",
                  gState.radioError(i));
  }
  const UBaseType_t depth =
      gState.captureQueue() != nullptr ? uxQueueMessagesWaiting(gState.captureQueue()) : 0;
  Serial.printf("sd mounted=%d written=%lu dropped=%lu queue=%u\n", gState.sdMounted() ? 1 : 0,
                static_cast<unsigned long>(gState.written()),
                static_cast<unsigned long>(gState.dropped()), static_cast<unsigned>(depth));
}

void handle(char* line) {
  while (*line == ' ') {
    ++line;
  }
  if (line[0] == '\0' || strcmp(line, "help") == 0) {
    printHelp();
  } else if (strcmp(line, "status") == 0) {
    printStatus();
  } else if (strcmp(line, "reload") == 0) {
    gState.requestRadioReload();
    Serial.println("radio reload requested");
  } else if (strcmp(line, "reboot") == 0) {
    Serial.println("rebooting");
    delay(50);
    ESP.restart();
  } else {
    Serial.println("unknown command, try help");
  }
}

}  // namespace

void pollSerialShell() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      gLine[gLength] = '\0';
      handle(gLine);
      gLength = 0;
      continue;
    }
    if (gLength + 1 < sizeof(gLine)) {
      gLine[gLength++] = ch;
    }
  }
}

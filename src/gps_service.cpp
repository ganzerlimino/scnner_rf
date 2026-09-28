#include "gps_service.h"

#include "board_config.h"
#include "shared_state.h"

#include <TinyGPSPlus.h>

namespace {

TinyGPSPlus gps;

void publishSnapshot() {
  GpsSnapshot snapshot{};
  snapshot.location_valid = gps.location.isValid() && gps.location.age() < 10000;
  snapshot.time_valid = gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024 &&
                        gps.time.age() < 10000;
  snapshot.satellites = gps.satellites.isValid()
                            ? static_cast<uint8_t>(gps.satellites.value() > 255 ? 255
                                                                               : gps.satellites.value())
                            : 0;
  if (snapshot.location_valid) {
    snapshot.latitude = gps.location.lat();
    snapshot.longitude = gps.location.lng();
  }
  if (snapshot.time_valid) {
    snprintf(snapshot.timestamp_utc, sizeof(snapshot.timestamp_utc),
             "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  }
  gState.setGps(snapshot);
}

void task(void*) {
  uint32_t currentBaud = 0;
  for (;;) {
    const AppConfig config = gState.copyConfig();
    if (!config.gps.enabled) {
      if (currentBaud != 0) {
        Serial2.end();
        currentBaud = 0;
      }
      GpsSnapshot idle{};
      gState.setGps(idle);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (currentBaud != config.gps.baud) {
      Serial2.begin(config.gps.baud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
      currentBaud = config.gps.baud;
      Serial.printf("[gps] UART2 %u baud, RX=%d TX=%d\n", static_cast<unsigned>(currentBaud),
                    kGpsRxPin, kGpsTxPin);
    }
    while (Serial2.available() > 0) {
      gps.encode(static_cast<uint8_t>(Serial2.read()));
    }
    publishSnapshot();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void startGpsService() {
  xTaskCreatePinnedToCore(task, "gps", 4096, nullptr, 2, nullptr, 1);
}

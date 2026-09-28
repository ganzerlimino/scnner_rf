#include "board_config.h"
#include "config_store.h"
#include "gps_service.h"
#include "oled_ui.h"
#include "radio_service.h"
#include "sd_logger.h"
#include "serial_shell.h"
#include "shared_state.h"
#include "web_server.h"
#include "wifi_portal.h"

// Core 0 owns the access point, the async web server and the OLED refresh.
// Core 1 owns GPS parsing, CC1101 reads and the SD writer. The radio ISR only
// raises a flag; SPI stays in the radio task so a slow card cannot drop a FIFO
// read that was already in progress.

void setup() {
  Serial.begin(kSerialBaud);
  delay(200);
  Serial.printf("\nRF-Tracker %s\n", RF_TRACKER_VERSION);
  Serial.println("Receive-only 433/868 MHz logger. This firmware does not transmit.");

  AppConfig config;
  loadAppConfig(config);
  char err[64];
  if (!validateAppConfig(config, err, sizeof(err))) {
    Serial.printf("[config] defaults failed validation (%s)\n", err);
    config = defaultAppConfig();
  }
  gState.begin(config.storage.queue_depth);
  gState.setConfig(config);

  Serial.printf("[board] VSPI SCK=%d MISO=%d MOSI=%d\n", kVspiSck, kVspiMiso, kVspiMosi);
  Serial.printf("[board] HSPI SCK=%d MISO=%d MOSI=%d SD_CS=%d\n", kHspiSck, kHspiMiso, kHspiMosi,
                config.storage.cs_pin);
  Serial.printf("[board] I2C SDA=%d SCL=%d  GPS RX=%d TX=%d\n", kI2cSda, kI2cScl, kGpsRxPin, kGpsTxPin);

  startWifi(config);
  startWebServer();
  startOledUi();
  startGpsService();
  startSdLogger();
  startRadioService();
  Serial.println("[boot] tasks started. Type 'help' on this serial port.");
}

void loop() {
  pollSerialShell();
  delay(10);
}

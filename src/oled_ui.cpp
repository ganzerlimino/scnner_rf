#include "oled_ui.h"

#include "board_config.h"
#include "shared_state.h"

#include <U8g2lib.h>
#include <Wire.h>

namespace {

// SSD1306 128x64 is the reference panel. A SH1106 clone is the same I2C bus;
// swap this constructor and rebuild (docs/extending.md).
U8G2_SSD1306_128X64_NONAME_F_HW_I2C gDisplay(U8G2_R0, U8X8_PIN_NONE);

void drawLine(int row, const char* text) {
  gDisplay.drawStr(0, 12 + (row * 12), text);
}

void task(void*) {
  bool started = false;
  for (;;) {
    const AppConfig config = gState.copyConfig();
    if (!config.oled.enabled) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (!started) {
      Wire.begin(kI2cSda, kI2cScl);
      gDisplay.setI2CAddress(static_cast<uint8_t>(config.oled.i2c_address << 1));
      started = gDisplay.begin();
      if (!started) {
        Serial.println("[oled] begin failed, retrying");
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
      gDisplay.setFont(u8g2_font_6x12_tf);
      Serial.printf("[oled] SSD1306 at 0x%02X\n", config.oled.i2c_address);
    }

    const GpsSnapshot gps = gState.copyGps();
    char line0[28];
    char line1[28];
    char line2[28];
    char line3[28];
    char line4[28];
    snprintf(line0, sizeof(line0), "RF-Tracker");
    if (gps.location_valid) {
      snprintf(line1, sizeof(line1), "GPS %usat %.3f %.3f", static_cast<unsigned>(gps.satellites),
               gps.latitude, gps.longitude);
    } else {
      snprintf(line1, sizeof(line1), "GPS searching");
    }
    snprintf(line2, sizeof(line2), "%s %s %lup %s", config.radios[0].id,
             gState.radioReady(0) ? "rx" : "off",
             static_cast<unsigned long>(gState.packets(0)),
             gState.rssiValid(0) ? String(gState.lastRssi(0)).c_str() : "--");
    snprintf(line3, sizeof(line3), "%s %s %lup %s", config.radios[1].id,
             gState.radioReady(1) ? "rx" : "off",
             static_cast<unsigned long>(gState.packets(1)),
             gState.rssiValid(1) ? String(gState.lastRssi(1)).c_str() : "--");
    const UBaseType_t depth =
        gState.captureQueue() != nullptr ? uxQueueMessagesWaiting(gState.captureQueue()) : 0;
    snprintf(line4, sizeof(line4), "SD %s q%u d%lu", gState.sdMounted() ? "ok" : "no",
             static_cast<unsigned>(depth), static_cast<unsigned long>(gState.dropped()));

    gDisplay.clearBuffer();
    drawLine(0, line0);
    drawLine(1, line1);
    drawLine(2, line2);
    drawLine(3, line3);
    drawLine(4, line4);
    gDisplay.sendBuffer();

    uint16_t wait = config.oled.refresh_ms;
    if (wait < 500) {
      wait = 500;
    }
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

}  // namespace

void startOledUi() {
  xTaskCreatePinnedToCore(task, "oled", 6144, nullptr, 1, nullptr, 0);
}

#pragma once

// Shared-bus wiring. These pins are a property of the PCB, so they live here
// instead of in the runtime config. Per-radio chip-select and GDO pins are
// in config.json and can change without editing this file.
//
// The defaults match the project brief:
//   VSPI  -> both CC1101 modules
//   HSPI  -> MicroSD
//   I2C   -> OLED
//   UART2 -> GPS

#include <Arduino.h>

#ifndef RF_TRACKER_VERSION
#define RF_TRACKER_VERSION "0.1.0"
#endif

static constexpr int kVspiSck = 18;
static constexpr int kVspiMiso = 19;
static constexpr int kVspiMosi = 23;

static constexpr int kHspiSck = 14;
static constexpr int kHspiMiso = 12;
static constexpr int kHspiMosi = 13;

static constexpr int kI2cSda = 21;
static constexpr int kI2cScl = 22;

// ESP32 RX listens to the GPS TX pin. ESP32 TX talks to the GPS RX pin.
static constexpr int kGpsRxPin = 16;
static constexpr int kGpsTxPin = 17;

static constexpr unsigned long kSerialBaud = 115200;

// Pins tied to the SPI flash. Using them as GPIO prevents the ESP32 from booting.
inline bool boardPinIsFlash(int pin) {
  return pin >= 6 && pin <= 11;
}

#pragma once

#include "app_types.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Cross-task state. Radio, GPS, SD, OLED and the web server never share
// bare globals; they copy snapshots under a mutex or use atomics.
class SharedState {
 public:
  void begin(uint8_t queueDepth);

  AppConfig copyConfig() const;
  void setConfig(const AppConfig& config);

  GpsSnapshot copyGps() const;
  void setGps(const GpsSnapshot& gps);

  void pushRecent(const RfRecord& record);
  size_t copyRecent(RfRecord* out, size_t max) const;

  QueueHandle_t captureQueue() const { return captureQueue_; }

  void addPacket(size_t radioIndex);
  void addCrcError(size_t radioIndex);
  void addDropped();
  void addWritten();
  void setLastRssi(size_t radioIndex, int16_t rssi);
  void setRadioReady(size_t radioIndex, bool ready);
  void setRadioError(size_t radioIndex, const char* message);
  void setSdMounted(bool mounted);

  uint32_t packets(size_t radioIndex) const;
  uint32_t crcErrors(size_t radioIndex) const;
  int16_t lastRssi(size_t radioIndex) const;
  bool rssiValid(size_t radioIndex) const;
  bool radioReady(size_t radioIndex) const;
  const char* radioError(size_t radioIndex) const;
  uint32_t dropped() const;
  uint32_t written() const;
  bool sdMounted() const;

  void requestRadioReload();
  bool takeRadioReload();
  void requestSdRemount();
  bool takeSdRemount();

 private:
  AppConfig config_{};
  GpsSnapshot gps_{};
  RfRecord recent_[kRecentPackets]{};
  size_t recentHead_ = 0;
  size_t recentCount_ = 0;
  char radioError_[kMaxRadios][48]{};

  SemaphoreHandle_t configMutex_ = nullptr;
  SemaphoreHandle_t gpsMutex_ = nullptr;
  SemaphoreHandle_t recentMutex_ = nullptr;
  QueueHandle_t captureQueue_ = nullptr;

  volatile uint32_t packets_[kMaxRadios] = {};
  volatile uint32_t crcErrors_[kMaxRadios] = {};
  volatile int16_t lastRssi_[kMaxRadios] = {};
  volatile bool radioReady_[kMaxRadios] = {};
  volatile bool rssiValid_[kMaxRadios] = {};
  volatile uint32_t dropped_ = 0;
  volatile uint32_t written_ = 0;
  volatile bool sdMounted_ = false;
  volatile bool radioReload_ = false;
  volatile bool sdRemount_ = false;
};

extern SharedState gState;

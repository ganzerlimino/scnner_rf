#include "shared_state.h"

#include <string.h>

SharedState gState;

void SharedState::begin(uint8_t queueDepth) {
  if (queueDepth < 4) {
    queueDepth = 4;
  }
  if (queueDepth > 64) {
    queueDepth = 64;
  }
  configMutex_ = xSemaphoreCreateMutex();
  gpsMutex_ = xSemaphoreCreateMutex();
  recentMutex_ = xSemaphoreCreateMutex();
  captureQueue_ = xQueueCreate(queueDepth, sizeof(RfRecord));
  for (size_t i = 0; i < kMaxRadios; ++i) {
    lastRssi_[i] = 0;
    radioError_[i][0] = '\0';
  }
}

AppConfig SharedState::copyConfig() const {
  AppConfig copy = config_;
  if (configMutex_ != nullptr) {
    xSemaphoreTake(configMutex_, portMAX_DELAY);
    copy = config_;
    xSemaphoreGive(configMutex_);
  }
  return copy;
}

void SharedState::setConfig(const AppConfig& config) {
  if (configMutex_ != nullptr) {
    xSemaphoreTake(configMutex_, portMAX_DELAY);
  }
  config_ = config;
  if (configMutex_ != nullptr) {
    xSemaphoreGive(configMutex_);
  }
}

GpsSnapshot SharedState::copyGps() const {
  GpsSnapshot copy = gps_;
  if (gpsMutex_ != nullptr) {
    xSemaphoreTake(gpsMutex_, portMAX_DELAY);
    copy = gps_;
    xSemaphoreGive(gpsMutex_);
  }
  return copy;
}

void SharedState::setGps(const GpsSnapshot& gps) {
  if (gpsMutex_ != nullptr) {
    xSemaphoreTake(gpsMutex_, portMAX_DELAY);
  }
  gps_ = gps;
  if (gpsMutex_ != nullptr) {
    xSemaphoreGive(gpsMutex_);
  }
}

void SharedState::pushRecent(const RfRecord& record) {
  if (recentMutex_ != nullptr) {
    xSemaphoreTake(recentMutex_, portMAX_DELAY);
  }
  recent_[recentHead_] = record;
  recentHead_ = (recentHead_ + 1) % kRecentPackets;
  if (recentCount_ < kRecentPackets) {
    recentCount_++;
  }
  if (recentMutex_ != nullptr) {
    xSemaphoreGive(recentMutex_);
  }
}

size_t SharedState::copyRecent(RfRecord* out, size_t max) const {
  if (out == nullptr || max == 0) {
    return 0;
  }
  if (recentMutex_ != nullptr) {
    xSemaphoreTake(recentMutex_, portMAX_DELAY);
  }
  const size_t count = recentCount_ < max ? recentCount_ : max;
  for (size_t i = 0; i < count; ++i) {
    const size_t index = (recentHead_ + kRecentPackets - 1 - i) % kRecentPackets;
    out[i] = recent_[index];
  }
  if (recentMutex_ != nullptr) {
    xSemaphoreGive(recentMutex_);
  }
  return count;
}

void SharedState::addPacket(size_t radioIndex) {
  if (radioIndex < kMaxRadios) {
    packets_[radioIndex]++;
  }
}

void SharedState::addCrcError(size_t radioIndex) {
  if (radioIndex < kMaxRadios) {
    crcErrors_[radioIndex]++;
  }
}

void SharedState::addDropped() { dropped_++; }

void SharedState::addWritten() { written_++; }

void SharedState::setLastRssi(size_t radioIndex, int16_t rssi) {
  if (radioIndex < kMaxRadios) {
    lastRssi_[radioIndex] = rssi;
    rssiValid_[radioIndex] = true;
  }
}

void SharedState::setRadioReady(size_t radioIndex, bool ready) {
  if (radioIndex < kMaxRadios) {
    radioReady_[radioIndex] = ready;
  }
}

void SharedState::setRadioError(size_t radioIndex, const char* message) {
  if (radioIndex >= kMaxRadios) {
    return;
  }
  if (message == nullptr) {
    radioError_[radioIndex][0] = '\0';
    return;
  }
  strncpy(radioError_[radioIndex], message, sizeof(radioError_[radioIndex]) - 1);
  radioError_[radioIndex][sizeof(radioError_[radioIndex]) - 1] = '\0';
}

void SharedState::setSdMounted(bool mounted) { sdMounted_ = mounted; }

uint32_t SharedState::packets(size_t radioIndex) const {
  return radioIndex < kMaxRadios ? packets_[radioIndex] : 0;
}

uint32_t SharedState::crcErrors(size_t radioIndex) const {
  return radioIndex < kMaxRadios ? crcErrors_[radioIndex] : 0;
}

int16_t SharedState::lastRssi(size_t radioIndex) const {
  if (radioIndex >= kMaxRadios || !rssiValid_[radioIndex]) {
    return 0;
  }
  return lastRssi_[radioIndex];
}

bool SharedState::rssiValid(size_t radioIndex) const {
  return radioIndex < kMaxRadios && rssiValid_[radioIndex];
}

bool SharedState::radioReady(size_t radioIndex) const {
  return radioIndex < kMaxRadios && radioReady_[radioIndex];
}

const char* SharedState::radioError(size_t radioIndex) const {
  if (radioIndex >= kMaxRadios) {
    return "";
  }
  return radioError_[radioIndex];
}

uint32_t SharedState::dropped() const { return dropped_; }

uint32_t SharedState::written() const { return written_; }

bool SharedState::sdMounted() const { return sdMounted_; }

void SharedState::requestRadioReload() { radioReload_ = true; }

bool SharedState::takeRadioReload() {
  if (!radioReload_) {
    return false;
  }
  radioReload_ = false;
  return true;
}

void SharedState::requestSdRemount() { sdRemount_ = true; }

bool SharedState::takeSdRemount() {
  if (!sdRemount_) {
    return false;
  }
  sdRemount_ = false;
  return true;
}

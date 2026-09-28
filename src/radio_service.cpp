#include "radio_service.h"

#include "board_config.h"
#include "csv_format.h"
#include "shared_state.h"

#include <RadioLib.h>
#include <SPI.h>

#include <string.h>

namespace {

// The CC1101 interrupt only sets a flag. Reading the FIFO needs SPI, and the
// SPI driver must not run inside an ISR (it takes locks and can corrupt a
// transfer that was already on VSPI). The radio task on core 1 does the read.
volatile bool gIrqFlags[kMaxRadios] = {};

void IRAM_ATTR signalRadio(size_t index) {
  if (index < kMaxRadios) {
    gIrqFlags[index] = true;
  }
}

template <size_t Index>
void IRAM_ATTR radioIsrThunk() {
  signalRadio(Index);
}

using IsrFn = void (*)();
IsrFn isrFor(size_t index) {
  static const IsrFn table[] = {radioIsrThunk<0>, radioIsrThunk<1>};
  static_assert(kMaxRadios == 2, "Add a radioIsrThunk<N> entry when raising kMaxRadios");
  return index < kMaxRadios ? table[index] : nullptr;
}

class RadioService {
 public:
  void start() {
    spi_ = new SPIClass(VSPI);
    spi_->begin(kVspiSck, kVspiMiso, kVspiMosi, -1);
    xTaskCreatePinnedToCore(taskEntry, "radio", 12288, this, 3, nullptr, 1);
  }

 private:
  static void taskEntry(void* arg) { static_cast<RadioService*>(arg)->loop(); }

  void loop() {
    for (;;) {
      if (gState.takeRadioReload() || !configured_) {
        apply(gState.copyConfig());
        configured_ = true;
      }
      bool handled = false;
      for (size_t i = 0; i < kMaxRadios; ++i) {
        if (!gIrqFlags[i] || radios_[i] == nullptr || !ready_[i]) {
          continue;
        }
        gIrqFlags[i] = false;
        handled = true;
        readPacket(i);
      }
      if (!handled) {
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    }
  }

  void apply(const AppConfig& config) {
    config_ = config;
    for (size_t i = 0; i < kMaxRadios; ++i) {
      const RadioConfig& radio = config.radios[i];
      if (!radio.enabled) {
        if (radios_[i] != nullptr && ready_[i]) {
          radios_[i]->standby();
        }
        ready_[i] = false;
        gState.setRadioReady(i, false);
        gState.setRadioError(i, "disabled");
        continue;
      }
      if (!ensureCreated(i, radio)) {
        continue;
      }
      if (builtCs_[i] != radio.cs_pin || builtGdo0_[i] != radio.gdo0_pin ||
          builtGdo2_[i] != radio.gdo2_pin) {
        gState.setRadioReady(i, ready_[i]);
        gState.setRadioError(i, "pin_change_reboot");
        continue;
      }
      arm(i, radio);
    }
  }

  bool ensureCreated(size_t index, const RadioConfig& radio) {
    if (radios_[index] != nullptr) {
      return true;
    }
    const uint32_t gdo2 = radio.gdo2_pin < 0 ? static_cast<uint32_t>(RADIOLIB_NC)
                                             : static_cast<uint32_t>(radio.gdo2_pin);
    modules_[index] = new Module(static_cast<uint32_t>(radio.cs_pin),
                                 static_cast<uint32_t>(radio.gdo0_pin),
                                 static_cast<uint32_t>(RADIOLIB_NC), gdo2, *spi_,
                                 SPISettings(2000000, MSBFIRST, SPI_MODE0));
    radios_[index] = new CC1101(modules_[index]);
    if (modules_[index] == nullptr || radios_[index] == nullptr) {
      gState.setRadioError(index, "alloc_failed");
      return false;
    }
    builtCs_[index] = radio.cs_pin;
    builtGdo0_[index] = radio.gdo0_pin;
    builtGdo2_[index] = radio.gdo2_pin;
    return true;
  }

  void arm(size_t index, const RadioConfig& radio) {
    ConfigFSK_t modem{};
    modem.frequency = radio.frequency_mhz;
    modem.bitRate = radio.bitrate_kbps;
    modem.frequencyDeviation = radio.deviation_khz;
    modem.receiverBandwidth = radio.rx_bandwidth_khz;
    modem.preambleLength = radio.preamble_bits;
    modem.power = 0;

    const int16_t began = radios_[index]->begin(modem);
    if (began != RADIOLIB_ERR_NONE) {
      ready_[index] = false;
      gState.setRadioReady(index, false);
      char message[48];
      snprintf(message, sizeof(message), "begin_%d", began);
      gState.setRadioError(index, message);
      Serial.printf("[radio] %s begin failed (%d)\n", radio.id, began);
      return;
    }
    radios_[index]->setSyncWord(radio.sync_hi, radio.sync_lo);
    int16_t lengthState = RADIOLIB_ERR_NONE;
    if (radio.variable_length) {
      lengthState = radios_[index]->variablePacketLengthMode(static_cast<uint8_t>(kMaxPayload));
    } else {
      lengthState = radios_[index]->fixedPacketLengthMode(radio.fixed_length);
    }
    radios_[index]->setCrcFiltering(true);
    if (lengthState != RADIOLIB_ERR_NONE) {
      Serial.printf("[radio] %s packet mode failed (%d)\n", radio.id, lengthState);
    }
    radios_[index]->setPacketReceivedAction(isrFor(index));
    const int16_t listen = radios_[index]->startReceive();
    ready_[index] = listen == RADIOLIB_ERR_NONE;
    gState.setRadioReady(index, ready_[index]);
    gState.setRadioError(index, ready_[index] ? "" : "listen_failed");
    Serial.printf("[radio] %s %.2f MHz %s\n", radio.id, static_cast<double>(radio.frequency_mhz),
                  ready_[index] ? "listening" : "not listening");
  }

  void readPacket(size_t index) {
    CC1101* radio = radios_[index];
    const size_t available = radio->getPacketLength();
    const size_t length = available > kMaxPayload ? kMaxPayload : available;
    uint8_t buffer[kMaxPayload] = {};
    const int16_t state = radio->readData(buffer, length == 0 ? 1 : length);
    const float rssi = radio->getRSSI();

    if (state == RADIOLIB_ERR_NONE && length > 0) {
      RfRecord record{};
      const GpsSnapshot gps = gState.copyGps();
      const RadioConfig& cfg = config_.radios[index];
      if (gps.time_valid) {
        strncpy(record.timestamp_utc, gps.timestamp_utc, sizeof(record.timestamp_utc) - 1);
        record.flags |= kFlagTime;
      }
      if (gps.location_valid) {
        record.latitude = gps.latitude;
        record.longitude = gps.longitude;
        record.satellites = gps.satellites;
        record.flags |= kFlagLocation;
      }
      record.flags |= kFlagCrcOk;
      record.frequency_mhz = cfg.frequency_mhz;
      record.rssi_dbm = static_cast<int16_t>(rssi);
      record.length = static_cast<uint8_t>(length);
      memcpy(record.payload, buffer, length);
      strncpy(record.band_id, cfg.id, sizeof(record.band_id) - 1);

      gState.addPacket(index);
      gState.setLastRssi(index, record.rssi_dbm);
      gState.pushRecent(record);
      if (config_.storage.enabled && gState.captureQueue() != nullptr) {
        if (xQueueSend(gState.captureQueue(), &record, 0) != pdTRUE) {
          gState.addDropped();
        }
      }
      if (config_.debug.serial_packets) {
        char line[320];
        formatRfCsvLine(record, false, line, sizeof(line));
        Serial.printf("[radio] %s", line);
      }
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      gState.addCrcError(index);
      gState.setLastRssi(index, static_cast<int16_t>(rssi));
    }

    // Return to RX even when the packet was rejected, or the module stays idle.
    const int16_t listen = radio->startReceive();
    ready_[index] = listen == RADIOLIB_ERR_NONE;
    gState.setRadioReady(index, ready_[index]);
  }

  SPIClass* spi_ = nullptr;
  Module* modules_[kMaxRadios] = {};
  CC1101* radios_[kMaxRadios] = {};
  int8_t builtCs_[kMaxRadios] = {};
  int8_t builtGdo0_[kMaxRadios] = {};
  int8_t builtGdo2_[kMaxRadios] = {};
  bool ready_[kMaxRadios] = {};
  bool configured_ = false;
  AppConfig config_{};
};

RadioService gRadio;

}  // namespace

void startRadioService() { gRadio.start(); }

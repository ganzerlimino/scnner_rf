#include "sd_logger.h"

#include "board_config.h"
#include "csv_format.h"
#include "shared_state.h"

#include <SD.h>
#include <SPI.h>

namespace {

SPIClass gSdSpi(HSPI);
File gFile;
char gOpenName[40] = "";
uint8_t gWritesSinceFlush = 0;

bool mountCard(const AppConfig& config) {
  gSdSpi.begin(kHspiSck, kHspiMiso, kHspiMosi, -1);
  const bool ok = SD.begin(config.storage.cs_pin, gSdSpi);
  gState.setSdMounted(ok);
  if (!ok) {
    Serial.printf("[sd] mount failed, CS=%d\n", config.storage.cs_pin);
  } else {
    Serial.printf("[sd] mounted, CS=%d\n", config.storage.cs_pin);
  }
  return ok;
}

void closeFile() {
  if (gFile) {
    gFile.flush();
    gFile.close();
  }
  gOpenName[0] = '\0';
  gWritesSinceFlush = 0;
}

bool openLog(const AppConfig& config, const RfRecord& record) {
  char name[40];
  if (!logFileName(config.storage.log_prefix, record, name, sizeof(name))) {
    return false;
  }
  if (strcmp(name, gOpenName) == 0 && gFile) {
    return true;
  }
  closeFile();
  const bool existed = SD.exists(name);
  gFile = SD.open(name, FILE_APPEND);
  if (!gFile) {
    Serial.printf("[sd] cannot open %s\n", name);
    return false;
  }
  strncpy(gOpenName, name, sizeof(gOpenName) - 1);
  if (!existed) {
    char header[160];
    csvHeader(header, sizeof(header), config.storage.include_crc_column);
    gFile.print(header);
  }
  Serial.printf("[sd] logging to %s\n", name);
  return true;
}

void writeRecord(const AppConfig& config, const RfRecord& record) {
  if (!openLog(config, record)) {
    return;
  }
  char line[256];
  formatRfCsvLine(record, config.storage.include_crc_column, line, sizeof(line));
  gFile.print(line);
  gWritesSinceFlush++;
  if (gWritesSinceFlush >= config.storage.flush_every) {
    gFile.flush();
    gWritesSinceFlush = 0;
  }
  gState.addWritten();
}

void task(void*) {
  AppConfig config = gState.copyConfig();
  bool mounted = config.storage.enabled && mountCard(config);
  for (;;) {
    if (gState.takeSdRemount()) {
      closeFile();
      SD.end();
      config = gState.copyConfig();
      mounted = config.storage.enabled && mountCard(config);
    }
    if (!mounted) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      config = gState.copyConfig();
      if (config.storage.enabled) {
        mounted = mountCard(config);
      }
      continue;
    }
    RfRecord record;
    if (xQueueReceive(gState.captureQueue(), &record, pdMS_TO_TICKS(500)) != pdTRUE) {
      continue;
    }
    config = gState.copyConfig();
    if (!config.storage.enabled) {
      continue;
    }
    writeRecord(config, record);
  }
}

}  // namespace

void startSdLogger() {
  xTaskCreatePinnedToCore(task, "sdlog", 8192, nullptr, 1, nullptr, 1);
}

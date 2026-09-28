#include "config_store.h"

#include "board_config.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <string.h>

namespace {

void copyText(char* dest, size_t destLen, const char* value) {
  if (destLen == 0) {
    return;
  }
  if (value == nullptr) {
    dest[0] = '\0';
    return;
  }
  snprintf(dest, destLen, "%s", value);
}

bool readBool(JsonVariantConst value, bool fallback) {
  if (value.isNull()) {
    return fallback;
  }
  if (value.is<bool>()) {
    return value.as<bool>();
  }
  if (value.is<int>()) {
    return value.as<int>() != 0;
  }
  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    return strcmp(text, "true") == 0 || strcmp(text, "1") == 0;
  }
  return fallback;
}

float readFloat(JsonVariantConst value, float fallback) {
  if (value.is<const char*>()) {
    return strtof(value.as<const char*>(), nullptr);
  }
  if (value.is<float>()) {
    return value.as<float>();
  }
  if (value.is<int>()) {
    return static_cast<float>(value.as<int>());
  }
  return fallback;
}

int readInt(JsonVariantConst value, int fallback) {
  if (value.is<int>()) {
    return value.as<int>();
  }
  if (value.is<float>()) {
    return static_cast<int>(value.as<float>());
  }
  if (value.is<const char*>()) {
    return atoi(value.as<const char*>());
  }
  return fallback;
}

void readSyncWord(JsonVariantConst value, RadioConfig& radio) {
  char text[8];
  if (value.is<const char*>()) {
    copyText(text, sizeof(text), value.as<const char*>());
  } else if (value.is<int>()) {
    snprintf(text, sizeof(text), "%04X", value.as<int>() & 0xFFFF);
  } else {
    return;
  }
  const char* digits = text;
  if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
    digits += 2;
  }
  if (digits[0] == '\0' || strlen(digits) > 4) {
    return;
  }
  char* end = nullptr;
  const unsigned long parsed = strtoul(digits, &end, 16);
  if (end == digits || *end != '\0') {
    return;
  }
  radio.sync_hi = static_cast<uint8_t>((parsed >> 8) & 0xFF);
  radio.sync_lo = static_cast<uint8_t>(parsed & 0xFF);
}

bool textOk(const char* value, size_t maxLen, bool allowEmpty) {
  if (value == nullptr) {
    return false;
  }
  const size_t length = strlen(value);
  if (length > maxLen || (length == 0 && !allowEmpty)) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (ch < 0x20 || ch == 0x7F) {
      return false;
    }
  }
  return true;
}

bool prefixOk(const char* value) {
  if (value == nullptr || value[0] == '\0' || strlen(value) > 12) {
    return false;
  }
  for (const char* p = value; *p != '\0'; ++p) {
    const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '_' || *p == '-';
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool pinFree(int pin, const int* used, size_t usedCount) {
  if (pin < 0) {
    return true;
  }
  if (pin > 33 || boardPinIsFlash(pin)) {
    return false;
  }
  for (size_t i = 0; i < usedCount; ++i) {
    if (used[i] == pin) {
      return false;
    }
  }
  return true;
}

void fillRadio(RadioConfig& radio, const char* id, float frequency, float minMhz, float maxMhz,
               int8_t cs, int8_t gdo0) {
  memset(&radio, 0, sizeof(radio));
  radio.enabled = true;
  copyText(radio.id, sizeof(radio.id), id);
  radio.frequency_mhz = frequency;
  radio.freq_min_mhz = minMhz;
  radio.freq_max_mhz = maxMhz;
  radio.bitrate_kbps = 4.8f;
  radio.deviation_khz = 5.0f;
  radio.rx_bandwidth_khz = 135.0f;
  radio.preamble_bits = 16;
  radio.sync_hi = 0xD3;
  radio.sync_lo = 0x91;
  radio.variable_length = true;
  radio.fixed_length = 16;
  radio.cs_pin = cs;
  radio.gdo0_pin = gdo0;
  radio.gdo2_pin = -1;
}

void overlayRadio(RadioConfig& radio, JsonObjectConst object) {
  if (object.isNull()) {
    return;
  }
  radio.enabled = readBool(object["enabled"], radio.enabled);
  if (object["id"].is<const char*>()) {
    copyText(radio.id, sizeof(radio.id), object["id"].as<const char*>());
  }
  radio.frequency_mhz = readFloat(object["frequency_mhz"], radio.frequency_mhz);
  radio.freq_min_mhz = readFloat(object["freq_min_mhz"], radio.freq_min_mhz);
  radio.freq_max_mhz = readFloat(object["freq_max_mhz"], radio.freq_max_mhz);
  radio.bitrate_kbps = readFloat(object["bitrate_kbps"], radio.bitrate_kbps);
  radio.deviation_khz = readFloat(object["deviation_khz"], radio.deviation_khz);
  radio.rx_bandwidth_khz = readFloat(object["rx_bandwidth_khz"], radio.rx_bandwidth_khz);
  radio.preamble_bits = static_cast<uint16_t>(readInt(object["preamble_bits"], radio.preamble_bits));
  readSyncWord(object["sync_word"], radio);
  radio.variable_length = readBool(object["variable_length"], radio.variable_length);
  radio.fixed_length = static_cast<uint8_t>(readInt(object["fixed_length"], radio.fixed_length));
  radio.cs_pin = static_cast<int8_t>(readInt(object["cs_pin"], radio.cs_pin));
  radio.gdo0_pin = static_cast<int8_t>(readInt(object["gdo0_pin"], radio.gdo0_pin));
  radio.gdo2_pin = static_cast<int8_t>(readInt(object["gdo2_pin"], radio.gdo2_pin));
}

void writeRadio(JsonObject object, const RadioConfig& radio) {
  char frequency[16];
  char minMhz[16];
  char maxMhz[16];
  char bitrate[16];
  char deviation[16];
  char bandwidth[16];
  char sync[5];
  snprintf(frequency, sizeof(frequency), "%.2f", static_cast<double>(radio.frequency_mhz));
  snprintf(minMhz, sizeof(minMhz), "%.2f", static_cast<double>(radio.freq_min_mhz));
  snprintf(maxMhz, sizeof(maxMhz), "%.2f", static_cast<double>(radio.freq_max_mhz));
  snprintf(bitrate, sizeof(bitrate), "%.2f", static_cast<double>(radio.bitrate_kbps));
  snprintf(deviation, sizeof(deviation), "%.2f", static_cast<double>(radio.deviation_khz));
  snprintf(bandwidth, sizeof(bandwidth), "%.2f", static_cast<double>(radio.rx_bandwidth_khz));
  snprintf(sync, sizeof(sync), "%02X%02X", radio.sync_hi, radio.sync_lo);

  object["enabled"] = radio.enabled;
  object["id"] = radio.id;
  object["frequency_mhz"] = frequency;
  object["freq_min_mhz"] = minMhz;
  object["freq_max_mhz"] = maxMhz;
  object["bitrate_kbps"] = bitrate;
  object["deviation_khz"] = deviation;
  object["rx_bandwidth_khz"] = bandwidth;
  object["preamble_bits"] = radio.preamble_bits;
  object["sync_word"] = sync;
  object["variable_length"] = radio.variable_length;
  object["fixed_length"] = radio.fixed_length;
  object["cs_pin"] = radio.cs_pin;
  object["gdo0_pin"] = radio.gdo0_pin;
  object["gdo2_pin"] = radio.gdo2_pin;
}

}  // namespace

AppConfig defaultAppConfig() {
  AppConfig config{};
  config.schema = 1;
  copyText(config.language, sizeof(config.language), "en");
  config.api_token[0] = '\0';
  copyText(config.wifi.mode, sizeof(config.wifi.mode), "ap");
  copyText(config.wifi.ap_ssid, sizeof(config.wifi.ap_ssid), "RF-Tracker");
  copyText(config.wifi.ap_password, sizeof(config.wifi.ap_password), "change-me");
  copyText(config.wifi.hostname, sizeof(config.wifi.hostname), "rf-tracker");
  copyText(config.wifi.ap_ip, sizeof(config.wifi.ap_ip), "192.168.4.1");
  config.wifi.ap_channel = 6;
  config.wifi.ap_max_clients = 4;
  config.gps.enabled = true;
  config.gps.baud = 9600;
  config.oled.enabled = true;
  config.oled.i2c_address = 0x3C;
  config.oled.refresh_ms = 2000;
  config.storage.enabled = true;
  copyText(config.storage.log_prefix, sizeof(config.storage.log_prefix), "rf_log");
  config.storage.include_crc_column = false;
  config.storage.queue_depth = 32;
  config.storage.flush_every = 1;
  config.storage.cs_pin = 15;
  config.debug.serial_packets = true;
  // EU ISM slices used by the reference build. Widen freq_min/max in config
  // only when the connected module and the local rules allow it.
  fillRadio(config.radios[0], "433", 433.92f, 433.05f, 434.79f, 5, 25);
  fillRadio(config.radios[1], "868", 868.30f, 863.00f, 870.00f, 4, 26);
  return config;
}

bool configToJson(const AppConfig& config, char* out, size_t outLen) {
  JsonDocument doc;
  doc["_readme_en"] =
      "Runtime configuration. Keys stay in English. Unknown keys are ignored. See docs/configuration.md.";
  doc["_readme_it"] =
      "Configurazione di runtime. Le chiavi restano in inglese. Le chiavi sconosciute sono ignorate. Vedi docs/configuration.it.md.";
  doc["schema"] = config.schema;
  doc["language"] = config.language;
  doc["api_token"] = config.api_token;

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["mode"] = config.wifi.mode;
  wifi["ap_ssid"] = config.wifi.ap_ssid;
  wifi["ap_password"] = config.wifi.ap_password;
  wifi["hostname"] = config.wifi.hostname;
  wifi["ap_ip"] = config.wifi.ap_ip;
  wifi["ap_channel"] = config.wifi.ap_channel;
  wifi["ap_max_clients"] = config.wifi.ap_max_clients;
  wifi["sta_ssid"] = config.wifi.sta_ssid;
  wifi["sta_password"] = config.wifi.sta_password;

  JsonObject gps = doc["gps"].to<JsonObject>();
  gps["enabled"] = config.gps.enabled;
  gps["baud"] = config.gps.baud;

  JsonObject oled = doc["oled"].to<JsonObject>();
  oled["enabled"] = config.oled.enabled;
  oled["i2c_address"] = config.oled.i2c_address;
  oled["refresh_ms"] = config.oled.refresh_ms;

  JsonObject storage = doc["storage"].to<JsonObject>();
  storage["enabled"] = config.storage.enabled;
  storage["log_prefix"] = config.storage.log_prefix;
  storage["include_crc_column"] = config.storage.include_crc_column;
  storage["queue_depth"] = config.storage.queue_depth;
  storage["flush_every"] = config.storage.flush_every;
  storage["cs_pin"] = config.storage.cs_pin;

  JsonObject debug = doc["debug"].to<JsonObject>();
  debug["serial_packets"] = config.debug.serial_packets;

  JsonArray radios = doc["radios"].to<JsonArray>();
  for (size_t i = 0; i < kMaxRadios; ++i) {
    writeRadio(radios.add<JsonObject>(), config.radios[i]);
  }

  const size_t written = serializeJsonPretty(doc, out, outLen);
  return written > 0 && written < outLen;
}

bool applyConfigJson(const char* json, AppConfig& config, char* err, size_t errLen) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error || !doc.is<JsonObject>()) {
    snprintf(err, errLen, "invalid_json");
    return false;
  }
  config = defaultAppConfig();
  if (doc["language"].is<const char*>()) {
    copyText(config.language, sizeof(config.language), doc["language"].as<const char*>());
  }
  if (doc["api_token"].is<const char*>()) {
    copyText(config.api_token, sizeof(config.api_token), doc["api_token"].as<const char*>());
  }
  JsonObjectConst wifi = doc["wifi"];
  if (!wifi.isNull()) {
    if (wifi["mode"].is<const char*>()) {
      copyText(config.wifi.mode, sizeof(config.wifi.mode), wifi["mode"].as<const char*>());
    }
    if (wifi["ap_ssid"].is<const char*>()) {
      copyText(config.wifi.ap_ssid, sizeof(config.wifi.ap_ssid), wifi["ap_ssid"].as<const char*>());
    }
    if (wifi["ap_password"].is<const char*>()) {
      copyText(config.wifi.ap_password, sizeof(config.wifi.ap_password), wifi["ap_password"].as<const char*>());
    }
    if (wifi["hostname"].is<const char*>()) {
      copyText(config.wifi.hostname, sizeof(config.wifi.hostname), wifi["hostname"].as<const char*>());
    }
    if (wifi["ap_ip"].is<const char*>()) {
      copyText(config.wifi.ap_ip, sizeof(config.wifi.ap_ip), wifi["ap_ip"].as<const char*>());
    }
    config.wifi.ap_channel = static_cast<uint8_t>(readInt(wifi["ap_channel"], config.wifi.ap_channel));
    config.wifi.ap_max_clients =
        static_cast<uint8_t>(readInt(wifi["ap_max_clients"], config.wifi.ap_max_clients));
    if (wifi["sta_ssid"].is<const char*>()) {
      copyText(config.wifi.sta_ssid, sizeof(config.wifi.sta_ssid), wifi["sta_ssid"].as<const char*>());
    }
    if (wifi["sta_password"].is<const char*>()) {
      copyText(config.wifi.sta_password, sizeof(config.wifi.sta_password),
               wifi["sta_password"].as<const char*>());
    }
  }
  JsonObjectConst gps = doc["gps"];
  if (!gps.isNull()) {
    config.gps.enabled = readBool(gps["enabled"], config.gps.enabled);
    config.gps.baud = static_cast<uint32_t>(readInt(gps["baud"], static_cast<int>(config.gps.baud)));
  }
  JsonObjectConst oled = doc["oled"];
  if (!oled.isNull()) {
    config.oled.enabled = readBool(oled["enabled"], config.oled.enabled);
    config.oled.i2c_address =
        static_cast<uint8_t>(readInt(oled["i2c_address"], config.oled.i2c_address));
    config.oled.refresh_ms =
        static_cast<uint16_t>(readInt(oled["refresh_ms"], config.oled.refresh_ms));
  }
  JsonObjectConst storage = doc["storage"];
  if (!storage.isNull()) {
    config.storage.enabled = readBool(storage["enabled"], config.storage.enabled);
    if (storage["log_prefix"].is<const char*>()) {
      copyText(config.storage.log_prefix, sizeof(config.storage.log_prefix),
               storage["log_prefix"].as<const char*>());
    }
    config.storage.include_crc_column =
        readBool(storage["include_crc_column"], config.storage.include_crc_column);
    config.storage.queue_depth =
        static_cast<uint8_t>(readInt(storage["queue_depth"], config.storage.queue_depth));
    config.storage.flush_every =
        static_cast<uint8_t>(readInt(storage["flush_every"], config.storage.flush_every));
    config.storage.cs_pin = static_cast<int8_t>(readInt(storage["cs_pin"], config.storage.cs_pin));
  }
  JsonObjectConst debug = doc["debug"];
  if (!debug.isNull()) {
    config.debug.serial_packets = readBool(debug["serial_packets"], config.debug.serial_packets);
  }
  JsonArrayConst radios = doc["radios"];
  if (!radios.isNull()) {
    size_t index = 0;
    for (JsonObjectConst radio : radios) {
      if (index >= kMaxRadios) {
        break;
      }
      overlayRadio(config.radios[index], radio);
      index++;
    }
  }
  return validateAppConfig(config, err, errLen);
}

bool validateAppConfig(const AppConfig& config, char* err, size_t errLen) {
  if (strcmp(config.language, "en") != 0 && strcmp(config.language, "it") != 0) {
    snprintf(err, errLen, "invalid_language");
    return false;
  }
  if (!textOk(config.api_token, 32, true) || !textOk(config.wifi.ap_ssid, 32, false) ||
      !textOk(config.wifi.ap_password, 63, true) || !textOk(config.wifi.hostname, 31, false) ||
      !textOk(config.wifi.sta_ssid, 32, true) || !textOk(config.wifi.sta_password, 63, true)) {
    snprintf(err, errLen, "invalid_text");
    return false;
  }
  if (strcmp(config.wifi.mode, "ap") != 0 && strcmp(config.wifi.mode, "apsta") != 0) {
    snprintf(err, errLen, "invalid_wifi_mode");
    return false;
  }
  const size_t passwordLength = strlen(config.wifi.ap_password);
  if (passwordLength > 0 && passwordLength < 8) {
    snprintf(err, errLen, "ap_password_short");
    return false;
  }
  int ipA = 0;
  int ipB = 0;
  int ipC = 0;
  int ipD = 0;
  if (sscanf(config.wifi.ap_ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD) != 4 || ipA < 1 || ipA > 223 ||
      ipB < 0 || ipB > 255 || ipC < 0 || ipC > 255 || ipD < 1 || ipD > 254) {
    snprintf(err, errLen, "invalid_ap_ip");
    return false;
  }
  if (config.wifi.ap_channel < 1 || config.wifi.ap_channel > 13 || config.wifi.ap_max_clients < 1 ||
      config.wifi.ap_max_clients > 8) {
    snprintf(err, errLen, "invalid_ap");
    return false;
  }
  switch (config.gps.baud) {
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
      break;
    default:
      snprintf(err, errLen, "invalid_gps_baud");
      return false;
  }
  if (config.oled.i2c_address < 0x08 || config.oled.i2c_address > 0x77 || config.oled.refresh_ms < 500 ||
      config.oled.refresh_ms > 10000) {
    snprintf(err, errLen, "invalid_oled");
    return false;
  }
  if (!prefixOk(config.storage.log_prefix) || config.storage.queue_depth < 4 ||
      config.storage.queue_depth > 64 || config.storage.flush_every < 1 ||
      config.storage.flush_every > 20) {
    snprintf(err, errLen, "invalid_storage");
    return false;
  }

  int used[24];
  size_t usedCount = 0;
  const int busPins[] = {kVspiSck, kVspiMiso, kVspiMosi, kHspiSck, kHspiMiso, kHspiMosi,
                         kI2cSda, kI2cScl, kGpsRxPin, kGpsTxPin};
  for (int pin : busPins) {
    used[usedCount++] = pin;
  }
  if (!pinFree(config.storage.cs_pin, used, usedCount) || config.storage.cs_pin < 0) {
    snprintf(err, errLen, "invalid_sd_cs");
    return false;
  }
  used[usedCount++] = config.storage.cs_pin;

  for (size_t i = 0; i < kMaxRadios; ++i) {
    const RadioConfig& radio = config.radios[i];
    if (radio.id[0] == '\0' || strlen(radio.id) > 6 || !prefixOk(radio.id)) {
      snprintf(err, errLen, "invalid_radio_id");
      return false;
    }
    if (radio.freq_min_mhz > radio.freq_max_mhz || radio.bitrate_kbps < 0.6f ||
        radio.bitrate_kbps > 600.0f || radio.deviation_khz <= 0.0f || radio.rx_bandwidth_khz <= 0.0f ||
        radio.preamble_bits < 16 || radio.preamble_bits > 192 || radio.fixed_length < 1 ||
        radio.fixed_length > kMaxPayload) {
      snprintf(err, errLen, "invalid_radio");
      return false;
    }
    if (radio.enabled &&
        (radio.frequency_mhz < radio.freq_min_mhz || radio.frequency_mhz > radio.freq_max_mhz)) {
      snprintf(err, errLen, "frequency_out_of_range");
      return false;
    }
    const int selects[] = {radio.cs_pin, radio.gdo0_pin, radio.gdo2_pin};
    for (size_t p = 0; p < 3; ++p) {
      if (p < 2 && selects[p] < 0) {
        snprintf(err, errLen, "invalid_radio_pin");
        return false;
      }
      if (!pinFree(selects[p], used, usedCount)) {
        snprintf(err, errLen, "pin_conflict");
        return false;
      }
      if (selects[p] >= 0) {
        if (usedCount >= (sizeof(used) / sizeof(used[0]))) {
          snprintf(err, errLen, "pin_conflict");
          return false;
        }
        used[usedCount++] = selects[p];
      }
    }
  }
  if (errLen > 0) {
    err[0] = '\0';
  }
  return true;
}

bool loadAppConfig(AppConfig& config) {
  config = defaultAppConfig();
  if (!LittleFS.begin(true)) {
    Serial.println("[config] LittleFS mount failed, using defaults");
    return false;
  }
  if (!LittleFS.exists("/config.json")) {
    Serial.println("[config] no config.json, writing defaults");
    return saveAppConfig(config);
  }
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[config] open failed, using defaults");
    return false;
  }
  String body = file.readString();
  file.close();
  char err[64];
  AppConfig loaded;
  if (!applyConfigJson(body.c_str(), loaded, err, sizeof(err))) {
    Serial.printf("[config] rejected (%s), using defaults\n", err);
    return saveAppConfig(config);
  }
  config = loaded;
  Serial.println("[config] loaded /config.json");
  return true;
}

bool saveAppConfig(const AppConfig& config) {
  char body[4096];
  if (!configToJson(config, body, sizeof(body))) {
    Serial.println("[config] serialize failed");
    return false;
  }
  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    Serial.println("[config] write open failed");
    return false;
  }
  const size_t written = file.print(body);
  file.close();
  return written == strlen(body);
}

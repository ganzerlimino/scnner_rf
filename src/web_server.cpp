#include "web_server.h"

#include "config_store.h"
#include "csv_format.h"
#include "shared_state.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SD.h>
#include <WiFi.h>

#include <math.h>
#include <string.h>

namespace {

AsyncWebServer gServer(80);
char gBody[8192];
size_t gBodyLen = 0;
bool gBodyOverflow = false;

const char kFallbackHtml[] = R"html(
<!DOCTYPE html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RF-Tracker</title></head>
<body style="font-family:sans-serif;background:#12140f;color:#e7e2d1;padding:1.5rem">
<h1>RF-Tracker</h1>
<p>The web UI is not on the filesystem yet. Build it with <code>pio run -t uploadfs</code>.</p>
<p>L'interfaccia web non e' ancora sul filesystem. Caricala con <code>pio run -t uploadfs</code>.</p>
<p>API: <a href="/api/status">/api/status</a></p>
</body></html>
)html";

bool authorized(AsyncWebServerRequest* request) {
  const AppConfig config = gState.copyConfig();
  if (config.api_token[0] == '\0') {
    return true;
  }
  if (!request->hasHeader("X-Api-Token")) {
    return false;
  }
  return request->header("X-Api-Token") == config.api_token;
}

void sendJson(AsyncWebServerRequest* request, int code, const char* json) {
  // String overload copies the body. The async task sends it after we return.
  AsyncWebServerResponse* response = request->beginResponse(code, "application/json", String(json));
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

bool sameText(const char* a, const char* b) { return strcmp(a, b) == 0; }

bool nearly(float a, float b) { return fabsf(a - b) < 0.001f; }

bool rebootRequired(const AppConfig& before, const AppConfig& after) {
  if (!sameText(before.wifi.mode, after.wifi.mode) || !sameText(before.wifi.ap_ssid, after.wifi.ap_ssid) ||
      !sameText(before.wifi.ap_password, after.wifi.ap_password) ||
      !sameText(before.wifi.hostname, after.wifi.hostname) ||
      !sameText(before.wifi.ap_ip, after.wifi.ap_ip) || before.wifi.ap_channel != after.wifi.ap_channel ||
      before.wifi.ap_max_clients != after.wifi.ap_max_clients ||
      !sameText(before.wifi.sta_ssid, after.wifi.sta_ssid) ||
      !sameText(before.wifi.sta_password, after.wifi.sta_password) ||
      before.oled.i2c_address != after.oled.i2c_address ||
      before.storage.queue_depth != after.storage.queue_depth ||
      before.storage.cs_pin != after.storage.cs_pin) {
    return true;
  }
  for (size_t i = 0; i < kMaxRadios; ++i) {
    if (before.radios[i].cs_pin != after.radios[i].cs_pin ||
        before.radios[i].gdo0_pin != after.radios[i].gdo0_pin ||
        before.radios[i].gdo2_pin != after.radios[i].gdo2_pin) {
      return true;
    }
  }
  return false;
}

bool radioParamsChanged(const AppConfig& before, const AppConfig& after) {
  for (size_t i = 0; i < kMaxRadios; ++i) {
    const RadioConfig& a = before.radios[i];
    const RadioConfig& b = after.radios[i];
    if (a.enabled != b.enabled || !nearly(a.frequency_mhz, b.frequency_mhz) ||
        !nearly(a.bitrate_kbps, b.bitrate_kbps) || !nearly(a.deviation_khz, b.deviation_khz) ||
        !nearly(a.rx_bandwidth_khz, b.rx_bandwidth_khz) || a.preamble_bits != b.preamble_bits ||
        a.sync_hi != b.sync_hi || a.sync_lo != b.sync_lo || a.variable_length != b.variable_length ||
        a.fixed_length != b.fixed_length) {
      return true;
    }
  }
  return false;
}

void handleConfigBody(AsyncWebServerRequest* request) {
  if (!authorized(request)) {
    sendJson(request, 401, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return;
  }
  const AppConfig before = gState.copyConfig();
  AppConfig after;
  char err[64] = "";
  if (!applyConfigJson(gBody, after, err, sizeof(err))) {
    char body[128];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", err[0] ? err : "invalid_json");
    sendJson(request, 400, body);
    return;
  }
  if (!saveAppConfig(after)) {
    sendJson(request, 500, "{\"ok\":false,\"error\":\"save_failed\"}");
    return;
  }
  gState.setConfig(after);
  if (radioParamsChanged(before, after)) {
    gState.requestRadioReload();
  }
  if (before.storage.include_crc_column != after.storage.include_crc_column ||
      strcmp(before.storage.log_prefix, after.storage.log_prefix) != 0 ||
      before.storage.enabled != after.storage.enabled || before.storage.cs_pin != after.storage.cs_pin) {
    gState.requestSdRemount();
  }
  const bool reboot = rebootRequired(before, after);
  char body[96];
  snprintf(body, sizeof(body), "{\"ok\":true,\"reboot_required\":%s}", reboot ? "true" : "false");
  sendJson(request, 200, body);
}

bool safeLogName(const char* name) {
  if (name == nullptr || name[0] == '\0' || strlen(name) > 32) {
    return false;
  }
  const char* dot = strrchr(name, '.');
  if (dot == nullptr || strcmp(dot, ".csv") != 0) {
    return false;
  }
  for (const char* p = name; *p != '\0'; ++p) {
    const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
                    *p == '_' || *p == '-' || *p == '.';
    if (!ok) {
      return false;
    }
  }
  return strstr(name, "..") == nullptr;
}

void onBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    gBodyLen = 0;
    gBodyOverflow = total >= sizeof(gBody);
  }
  if (gBodyOverflow) {
    if (index + len >= total) {
      sendJson(request, 413, "{\"ok\":false,\"error\":\"body_too_large\"}");
    }
    return;
  }
  memcpy(gBody + gBodyLen, data, len);
  gBodyLen += len;
  if (index + len >= total) {
    gBody[gBodyLen] = '\0';
    handleConfigBody(request);
  }
}

void addStatusRoutes() {
  gServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    const AppConfig config = gState.copyConfig();
    const GpsSnapshot gps = gState.copyGps();
    JsonDocument doc;
    doc["version"] = RF_TRACKER_VERSION;
    doc["uptime_s"] = millis() / 1000;
    doc["heap_free"] = ESP.getFreeHeap();
    doc["language"] = config.language;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["mode"] = config.wifi.mode;
    wifi["ssid"] = config.wifi.ap_ssid;
    wifi["ip"] = WiFi.softAPIP().toString();
    wifi["clients"] = WiFi.softAPgetStationNum();
    wifi["sta_ip"] = WiFi.localIP().toString();
    JsonObject gpsJson = doc["gps"].to<JsonObject>();
    gpsJson["location_valid"] = gps.location_valid;
    gpsJson["time_valid"] = gps.time_valid;
    gpsJson["latitude"] = gps.location_valid ? gps.latitude : 0;
    gpsJson["longitude"] = gps.location_valid ? gps.longitude : 0;
    gpsJson["satellites"] = gps.satellites;
    gpsJson["timestamp_utc"] = gps.timestamp_utc;
    JsonObject sd = doc["sd"].to<JsonObject>();
    sd["enabled"] = config.storage.enabled;
    sd["mounted"] = gState.sdMounted();
    sd["written"] = gState.written();
    sd["dropped"] = gState.dropped();
    sd["queue"] = gState.captureQueue() ? uxQueueMessagesWaiting(gState.captureQueue()) : 0;
    JsonArray radios = doc["radios"].to<JsonArray>();
    for (size_t i = 0; i < kMaxRadios; ++i) {
      JsonObject radio = radios.add<JsonObject>();
      radio["id"] = config.radios[i].id;
      radio["enabled"] = config.radios[i].enabled;
      radio["ready"] = gState.radioReady(i);
      radio["frequency_mhz"] = config.radios[i].frequency_mhz;
      radio["packets"] = gState.packets(i);
      radio["crc_errors"] = gState.crcErrors(i);
      radio["error"] = gState.radioError(i);
      if (gState.rssiValid(i)) {
        radio["last_rssi_dbm"] = gState.lastRssi(i);
      } else {
        radio["last_rssi_dbm"] = nullptr;
      }
    }
    String body;
    serializeJson(doc, body);
    sendJson(request, 200, body.c_str());
  });

  gServer.on("/api/packets", HTTP_GET, [](AsyncWebServerRequest* request) {
    RfRecord records[kRecentPackets];
    const size_t count = gState.copyRecent(records, kRecentPackets);
    JsonDocument doc;
    JsonArray list = doc["packets"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
      char hex[(kMaxPayload * 2) + 1];
      bytesToHex(records[i].payload, records[i].length, hex, sizeof(hex));
      JsonObject item = list.add<JsonObject>();
      item["timestamp_utc"] = records[i].timestamp_utc;
      item["band"] = records[i].band_id;
      item["frequency_mhz"] = records[i].frequency_mhz;
      item["rssi_dbm"] = records[i].rssi_dbm;
      item["length"] = records[i].length;
      item["payload_hex"] = hex;
      item["location_valid"] = (records[i].flags & kFlagLocation) != 0;
      if ((records[i].flags & kFlagLocation) != 0) {
        item["latitude"] = records[i].latitude;
        item["longitude"] = records[i].longitude;
        item["satellites"] = records[i].satellites;
      }
      item["crc_ok"] = (records[i].flags & kFlagCrcOk) != 0;
    }
    String body;
    serializeJson(doc, body);
    sendJson(request, 200, body.c_str());
  });

  gServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    char body[4096];
    if (!configToJson(gState.copyConfig(), body, sizeof(body))) {
      sendJson(request, 500, "{\"ok\":false,\"error\":\"serialize_failed\"}");
      return;
    }
    sendJson(request, 200, body);
  });

  gServer.on("/api/config", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr, onBody);

  gServer.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["mounted"] = gState.sdMounted();
    JsonArray files = doc["files"].to<JsonArray>();
    if (gState.sdMounted()) {
      File root = SD.open("/");
      if (root) {
        for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
          const char* name = entry.name();
          const char* base = strrchr(name, '/');
          base = base == nullptr ? name : base + 1;
          if (!entry.isDirectory() && safeLogName(base)) {
            JsonObject file = files.add<JsonObject>();
            file["name"] = base;
            file["size"] = entry.size();
          }
          entry.close();
        }
        root.close();
      }
    }
    String body;
    serializeJson(doc, body);
    sendJson(request, 200, body.c_str());
  });

  gServer.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest* request) {
    if (!authorized(request)) {
      sendJson(request, 401, "{\"ok\":false,\"error\":\"unauthorized\"}");
      return;
    }
    if (!request->hasParam("name")) {
      sendJson(request, 400, "{\"ok\":false,\"error\":\"missing_name\"}");
      return;
    }
    const String name = request->getParam("name")->value();
    if (!safeLogName(name.c_str()) || !gState.sdMounted()) {
      sendJson(request, 400, "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }
    char path[48];
    snprintf(path, sizeof(path), "/%s", name.c_str());
    if (!SD.remove(path)) {
      sendJson(request, 404, "{\"ok\":false,\"error\":\"not_found\"}");
      return;
    }
    gState.requestSdRemount();
    sendJson(request, 200, "{\"ok\":true}");
  });

  gServer.on("/api/logs/download", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("name") || !gState.sdMounted()) {
      sendJson(request, 404, "{\"ok\":false,\"error\":\"not_found\"}");
      return;
    }
    const String name = request->getParam("name")->value();
    if (!safeLogName(name.c_str())) {
      sendJson(request, 400, "{\"ok\":false,\"error\":\"invalid_name\"}");
      return;
    }
    char path[48];
    snprintf(path, sizeof(path), "/%s", name.c_str());
    if (!SD.exists(path)) {
      sendJson(request, 404, "{\"ok\":false,\"error\":\"not_found\"}");
      return;
    }
    request->send(SD, path, "text/csv", true);
  });

  gServer.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!authorized(request)) {
      sendJson(request, 401, "{\"ok\":false,\"error\":\"unauthorized\"}");
      return;
    }
    sendJson(request, 200, "{\"ok\":true}");
    xTaskCreate(
        [](void*) {
          vTaskDelay(pdMS_TO_TICKS(300));
          ESP.restart();
        },
        "reboot", 2048, nullptr, 1, nullptr);
  });
}

}  // namespace

void startWebServer() {
  addStatusRoutes();
  if (LittleFS.exists("/index.html")) {
    gServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache");
  } else {
    gServer.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(200, "text/html", kFallbackHtml);
    });
  }
  gServer.onNotFound([](AsyncWebServerRequest* request) {
    sendJson(request, 404, "{\"ok\":false,\"error\":\"not_found\"}");
  });
  gServer.begin();
  Serial.printf("[web] http://%s/\n", WiFi.softAPIP().toString().c_str());
}

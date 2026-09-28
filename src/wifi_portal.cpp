#include "wifi_portal.h"

#include <WiFi.h>

namespace {

IPAddress parseIp(const char* text) {
  int a = 192;
  int b = 168;
  int c = 4;
  int d = 1;
  sscanf(text, "%d.%d.%d.%d", &a, &b, &c, &d);
  return IPAddress(static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint8_t>(c),
                   static_cast<uint8_t>(d));
}

}  // namespace

void startWifi(const AppConfig& config) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(config.wifi.hostname);
  const bool useSta = strcmp(config.wifi.mode, "apsta") == 0 && config.wifi.sta_ssid[0] != '\0';
  WiFi.mode(useSta ? WIFI_AP_STA : WIFI_AP);

  const IPAddress ip = parseIp(config.wifi.ap_ip);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  const char* password = strlen(config.wifi.ap_password) >= 8 ? config.wifi.ap_password : nullptr;
  const bool apOk = WiFi.softAP(config.wifi.ap_ssid, password, config.wifi.ap_channel, 0,
                                config.wifi.ap_max_clients);
  WiFi.softAPsetHostname(config.wifi.hostname);
  Serial.printf("[wifi] AP %s -> %s (%s)\n", apOk ? "up" : "failed", config.wifi.ap_ssid,
                WiFi.softAPIP().toString().c_str());
  if (password == nullptr) {
    Serial.println("[wifi] AP password missing or short; network is open");
  }
  if (useSta) {
    WiFi.begin(config.wifi.sta_ssid, config.wifi.sta_password);
    Serial.printf("[wifi] STA joining %s\n", config.wifi.sta_ssid);
  }
}

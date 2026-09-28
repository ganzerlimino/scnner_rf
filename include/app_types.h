#pragma once

#include <stddef.h>
#include <stdint.h>

// Two radios ship on the reference board. Raise this, add an ISR thunk in
// radio_service.cpp, and add a radios[] entry. See docs/extending.md.
static constexpr size_t kMaxRadios = 2;

// CC1101 FIFO is 64 bytes. The CSV hex column stays inside VARCHAR(255).
static constexpr size_t kMaxPayload = 64;

// Packets kept in RAM for the web "live" table. The SD card is the log of record.
static constexpr size_t kRecentPackets = 24;

static constexpr uint8_t kFlagLocation = 0x01;
static constexpr uint8_t kFlagTime = 0x02;
static constexpr uint8_t kFlagCrcOk = 0x04;

struct RadioConfig {
  bool enabled;
  char id[8];
  float frequency_mhz;
  float freq_min_mhz;
  float freq_max_mhz;
  float bitrate_kbps;
  float deviation_khz;
  float rx_bandwidth_khz;
  uint16_t preamble_bits;
  uint8_t sync_hi;
  uint8_t sync_lo;
  bool variable_length;
  uint8_t fixed_length;
  int8_t cs_pin;
  int8_t gdo0_pin;
  int8_t gdo2_pin;  // -1 when the module's GDO2 pin is left unconnected
};

struct AppConfig {
  uint16_t schema;
  char language[3];
  char api_token[33];
  // Palette id from data/themes.json. The page applies it; the firmware only stores it.
  char theme[16];
  struct {
    char mode[8];  // "ap" or "apsta"
    char ap_ssid[33];
    char ap_password[64];
    char hostname[32];
    char ap_ip[16];
    uint8_t ap_channel;
    uint8_t ap_max_clients;
    char sta_ssid[33];
    char sta_password[64];
  } wifi;
  struct {
    bool enabled;
    uint32_t baud;
  } gps;
  struct {
    bool enabled;
    uint8_t i2c_address;
    uint16_t refresh_ms;
  } oled;
  struct {
    bool enabled;
    char log_prefix[16];
    bool include_crc_column;
    uint8_t queue_depth;
    uint8_t flush_every;
    int8_t cs_pin;
  } storage;
  struct {
    bool serial_packets;
  } debug;
  RadioConfig radios[kMaxRadios];
};

struct GpsSnapshot {
  bool location_valid;
  bool time_valid;
  double latitude;
  double longitude;
  uint8_t satellites;
  char timestamp_utc[32];
};

// Plain data, copied into a FreeRTOS queue. Do not put Strings or pointers here.
struct RfRecord {
  char timestamp_utc[32];
  double latitude;
  double longitude;
  uint8_t satellites;
  uint8_t flags;
  float frequency_mhz;
  int16_t rssi_dbm;
  uint8_t length;
  uint8_t payload[kMaxPayload];
  char band_id[8];
};

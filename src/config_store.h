#pragma once

#include "app_types.h"

// Loads and stores /config.json on LittleFS. Missing keys keep the defaults,
// so a partial file is a valid way to override one radio.
AppConfig defaultAppConfig();

// Mounts LittleFS (formatting it on first boot) and loads /config.json.
bool loadAppConfig(AppConfig& config);

bool saveAppConfig(const AppConfig& config);

// Checks ranges and pin clashes. On failure, err receives a short English reason
// that the web UI maps through the locale files when it recognises the code.
bool validateAppConfig(const AppConfig& config, char* err, size_t errLen);

// Overlays a JSON object onto a copy of the defaults-filled config.
// Returns false when the document is not an object.
bool applyConfigJson(const char* json, AppConfig& config, char* err, size_t errLen);

// Writes the canonical config, including bilingual "_readme_*" hints.
bool configToJson(const AppConfig& config, char* out, size_t outLen);

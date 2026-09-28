#pragma once

#include "app_types.h"

// Starts the access point (and optional station) from the loaded config.
// Wi-Fi changes are applied on the next boot; the web API reports that.
void startWifi(const AppConfig& config);

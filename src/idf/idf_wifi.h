#ifndef LAUNCHER_IDF_WIFI_H
#define LAUNCHER_IDF_WIFI_H

#include "esp_wifi_types.h"
#include <stdint.h>
#include <string>
#include <vector>

struct LauncherWifiAp {
    std::string ssid;
    wifi_auth_mode_t authmode;
    int8_t rssi;
};

enum class LauncherWifiConnectState : uint8_t {
    Pending,
    Connected,
    WrongPassword,
    Failed,
};

// False only on boards whose Wi-Fi lives on an ESP-Hosted co-processor that
// failed to come up. Everything else (native Wi-Fi, or hosted working) leaves it
// true, so callers can test it unconditionally.
extern bool hostedWifiAvailable;

bool launcherWifiStartSta();
bool launcherWifiInitHostedSdio(
    int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst
);

// Crash-guarded wrapper around launcherWifiInitHostedSdio(). A co-processor
// running the wrong firmware makes the hosted stack spin for ~20 s and then
// panic, so the board never finishes booting. This records an "attempting" flag
// in NVS before the call and clears it after: finding that flag still set at
// boot means the previous attempt took the device down, so hosted is marked
// unavailable and skipped from then on. Updates hostedWifiAvailable.
bool launcherWifiInitHostedSdioGuarded(
    int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst
);

// Clears a latched "hosted is broken" verdict so the next boot probes again.
// Call this after flashing new co-processor firmware.
void launcherWifiHostedResetGuard();
LauncherWifiConnectState launcherWifiConnectStatus(
    const char *ssid, const char *password, uint32_t timeout_ms
);
bool launcherWifiConnect(const char *ssid, const char *password, uint32_t timeout_ms);
int launcherWifiScan(std::vector<LauncherWifiAp> &out);
bool launcherWifiStartAp(const char *ssid, const char *password, uint8_t channel, uint8_t max_clients);
bool launcherWifiStop();
bool launcherWifiIsConnected();
std::string launcherWifiLocalIp();
std::string launcherWifiApIp();
std::string launcherWifiMac();
bool launcherMdnsStart(const char *host, uint16_t port = 80);
void launcherMdnsStop();

#endif

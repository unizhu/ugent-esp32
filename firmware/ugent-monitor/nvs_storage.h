/**
 * UGENT ESP32 Monitor — NVS Storage
 *
 * Wrapper over ESP32 Preferences (NVS) for persistent settings.
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class NvsStorage {
public:
    bool begin() {
        return prefs_.begin(NVS_NAMESPACE, false);
    }

    void end() {
        prefs_.end();
    }

    // ─── WiFi ─────────────────────────────────────────────────────────────
    String getWifiSsid() const {
        return prefs_.getString(NVS_KEY_WIFI_SSID, "");
    }

    void setWifiSsid(const char* val) {
        prefs_.putString(NVS_KEY_WIFI_SSID, val);
    }

    String getWifiPass() const {
        return prefs_.getString(NVS_KEY_WIFI_PASS, "");
    }

    void setWifiPass(const char* val) {
        prefs_.putString(NVS_KEY_WIFI_PASS, val);
    }

    bool hasWifiCredentials() const {
        return prefs_.isKey(NVS_KEY_WIFI_SSID) &&
               getWifiSsid().length() > 0;
    }

    // ─── UGENT Server ─────────────────────────────────────────────────────
    String getUgentHost() const {
        return prefs_.getString(NVS_KEY_UGENT_HOST, DEFAULT_UGENT_HOST);
    }

    void setUgentHost(const char* val) {
        prefs_.putString(NVS_KEY_UGENT_HOST, val);
    }

    uint16_t getUgentPort() const {
        return (uint16_t)prefs_.getUInt(NVS_KEY_UGENT_PORT, DEFAULT_UGENT_PORT);
    }

    void setUgentPort(uint16_t val) {
        prefs_.putUInt(NVS_KEY_UGENT_PORT, val);
    }

    String getUgentApiKey() const {
        return prefs_.getString(NVS_KEY_UGENT_APIKEY, DEFAULT_UGENT_APIKEY);
    }

    void setUgentApiKey(const char* val) {
        prefs_.putString(NVS_KEY_UGENT_APIKEY, val);
    }

    bool hasUgentConfig() const {
        return prefs_.isKey(NVS_KEY_UGENT_HOST) &&
               getUgentHost().length() > 0;
    }

    // ─── Display ──────────────────────────────────────────────────────────
    uint8_t getBrightness() const {
        return (uint8_t)prefs_.getUInt(NVS_KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS);
    }

    void setBrightness(uint8_t val) {
        prefs_.putUInt(NVS_KEY_BRIGHTNESS, val);
    }

    // ─── Utility ──────────────────────────────────────────────────────────
    void clearAll() {
        prefs_.clear();
    }

private:
    Preferences prefs_;
};

#endif // NVS_STORAGE_H

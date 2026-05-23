/**
 * UGENT ESP32 Monitor — NVS Storage
 *
 * Wrapper over ESP32 Preferences (NVS) for persistent settings.
 * Supports up to MAX_WIFI_SAVED (3) WiFi networks.
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

    // ─── WiFi (multi-network) ─────────────────────────────────────────────

    int getWifiCount() const {
        return prefs_.getInt(NVS_KEY_WIFI_COUNT, 0);
    }

    void setWifiCount(int count) {
        prefs_.putInt(NVS_KEY_WIFI_COUNT, count);
    }

    String getWifiSsid(int idx) const {
        if (idx < 0 || idx >= MAX_WIFI_SAVED) return "";
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_SSID, idx);
        return prefs_.getString(key, "");
    }

    void setWifiSsid(int idx, const char* val) {
        if (idx < 0 || idx >= MAX_WIFI_SAVED) return;
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_SSID, idx);
        prefs_.putString(key, val);
    }

    String getWifiPass(int idx) const {
        if (idx < 0 || idx >= MAX_WIFI_SAVED) return "";
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_PASS, idx);
        return prefs_.getString(key, "");
    }

    void setWifiPass(int idx, const char* val) {
        if (idx < 0 || idx >= MAX_WIFI_SAVED) return;
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_PASS, idx);
        prefs_.putString(key, val);
    }

    bool hasWifiCredentials() const {
        return getWifiCount() > 0;
    }

    // Find slot for an SSID: returns existing index or first empty slot
    int findWifiSlot(const char* ssid) const {
        int count = getWifiCount();
        // Check existing
        for (int i = 0; i < MAX_WIFI_SAVED; i++) {
            if (getWifiSsid(i) == ssid) return i;
        }
        // First empty slot
        for (int i = 0; i < MAX_WIFI_SAVED; i++) {
            if (getWifiSsid(i).length() == 0) return i;
        }
        // Overwrite oldest (slot 0, shift others)
        return 0;
    }

    // Save WiFi credentials (upsert)
    void saveWifi(const char* ssid, const char* pass) {
        int slot = findWifiSlot(ssid);
        setWifiSsid(slot, ssid);
        setWifiPass(slot, pass);
        // Update count
        int count = 0;
        for (int i = 0; i < MAX_WIFI_SAVED; i++) {
            if (getWifiSsid(i).length() > 0) count++;
        }
        setWifiCount(count);
    }

    // Delete a saved WiFi by index
    void deleteWifi(int idx) {
        if (idx < 0 || idx >= MAX_WIFI_SAVED) return;
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_SSID, idx);
        prefs_.remove(key);
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_WIFI_PASS, idx);
        prefs_.remove(key);
        // Recount
        int count = 0;
        for (int i = 0; i < MAX_WIFI_SAVED; i++) {
            if (getWifiSsid(i).length() > 0) count++;
        }
        setWifiCount(count);
    }

    // ─── Legacy single-network compat (maps to slot 0) ────────────────────
    String getWifiSsid() const { return getWifiSsid(0); }
    void setWifiSsid(const char* val) { setWifiSsid(0, val); }
    String getWifiPass() const { return getWifiPass(0); }
    void setWifiPass(const char* val) { setWifiPass(0, val); }

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
    mutable Preferences prefs_;
};

#endif // NVS_STORAGE_H

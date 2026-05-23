/**
 * UGENT ESP32 Monitor — WiFi Manager
 *
 * Handles WiFi: multi-network auto-connect, scan & select, reconnect.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "nvs_storage.h"

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class WifiManager {
public:
    void begin(NvsStorage* nvs) {
        nvs_ = nvs;
        state_ = WifiState::DISCONNECTED;
        lastConnectAttempt_ = 0;

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false);

        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
                state_ = WifiState::DISCONNECTED;
                lastConnectAttempt_ = 0;
            }
        });
    }

    void loop() {
        unsigned long now = millis();

        // Auto-reconnect — try all saved networks
        if (state_ == WifiState::DISCONNECTED &&
            nvs_ && nvs_->hasWifiCredentials() &&
            lastConnectAttempt_ == 0) {
            lastConnectAttempt_ = now;
        }

        if (state_ == WifiState::DISCONNECTED &&
            nvs_ && nvs_->hasWifiCredentials() &&
            lastConnectAttempt_ > 0 &&
            (now - lastConnectAttempt_) >= WIFI_RECONNECT_DELAY_MS) {
            autoConnect();
        }
    }

    // Try all saved networks in order until one connects
    bool autoConnect() {
        if (!nvs_ || !nvs_->hasWifiCredentials()) return false;

        int count = nvs_->getWifiCount();
        for (int i = 0; i < count && i < MAX_WIFI_SAVED; i++) {
            String ssid = nvs_->getWifiSsid(i);
            String pass = nvs_->getWifiPass(i);
            if (ssid.length() == 0) continue;

            setState(WifiState::CONNECTING);
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(ssid.c_str(), pass.c_str());

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED) {
                if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) break;
                delay(200);
            }

            if (WiFi.status() == WL_CONNECTED) {
                setState(WifiState::CONNECTED);
                lastConnectAttempt_ = 0;
                return true;
            }
        }

        setState(WifiState::FAILED);
        lastConnectAttempt_ = millis();
        return false;
    }

    // Connect with specific credentials, save to NVS
    bool connect(const char* ssid, const char* pass) {
        WiFi.disconnect(true);
        delay(100);

        setState(WifiState::CONNECTING);
        WiFi.begin(ssid, pass);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
                setState(WifiState::FAILED);
                return false;
            }
            delay(200);
        }

        // Save to NVS (multi-network upsert)
        if (nvs_) {
            nvs_->saveWifi(ssid, pass);
        }
        setState(WifiState::CONNECTED);
        lastConnectAttempt_ = 0;
        return true;
    }

    // Delete a saved network by index
    void deleteSaved(int idx) {
        if (nvs_) nvs_->deleteWifi(idx);
    }

    // Scan for available networks (blocking, ~3 sec)
    int scanNetworks() {
        int n = WiFi.scanNetworks(false, false, false, 300);
        if (n < 0) n = 0;
        if (n > MAX_WIFI_NETWORKS) n = MAX_WIFI_NETWORKS;
        return n;
    }

    String getScannedSsid(int index) const { return WiFi.SSID(index); }
    int32_t getScannedRssi(int index) const { return WiFi.RSSI(index); }
    wifi_auth_mode_t getScannedAuth(int index) const { return WiFi.encryptionType(index); }
    void clearScanResults() { WiFi.scanDelete(); }

    // ─── Getters ──────────────────────────────────────────────────────────
    WifiState getState() const { return state_; }
    bool isConnected() const { return state_ == WifiState::CONNECTED; }
    String getLocalIp() const { return WiFi.localIP().toString(); }
    int8_t getRssi() const { return WiFi.RSSI(); }
    String getSsid() const { return WiFi.SSID(); }

    // Number of saved networks
    int getSavedCount() const { return nvs_ ? nvs_->getWifiCount() : 0; }
    String getSavedSsid(int idx) const { return nvs_ ? nvs_->getWifiSsid(idx) : ""; }

private:
    void setState(WifiState newState) {
        if (state_ != newState) {
            state_ = newState;
        }
    }

    NvsStorage* nvs_;
    WifiState state_;
    unsigned long lastConnectAttempt_;
};

#endif // WIFI_MANAGER_H

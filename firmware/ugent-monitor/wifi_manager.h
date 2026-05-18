/**
 * UGENT ESP32 Monitor — WiFi Manager
 *
 * Handles WiFi connection: auto-connect, scan & select, SmartConfig fallback.
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
    SMARTCONFIG_WAITING,
    SCAN_RUNNING,
    FAILED
};

// Callback types for UI updates
typedef void (*WifiStateCallback)(WifiState state);
typedef void (*WifiScanCallback)(int numNetworks);

class WifiManager {
public:
    void begin(NvsStorage* nvs) {
        nvs_ = nvs;
        state_ = WifiState::DISCONNECTED;
        lastConnectAttempt_ = 0;
        stateCallback_ = nullptr;
        scanCallback_ = nullptr;

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false); // Lower latency

        // Register disconnect handler for auto-reconnect
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
                setState(WifiState::DISCONNECTED);
                lastConnectAttempt_ = 0; // Trigger reconnect on next loop
            }
        });
    }

    void loop() {
        unsigned long now = millis();

        // Auto-reconnect logic
        if (state_ == WifiState::DISCONNECTED &&
            nvs_->hasWifiCredentials() &&
            lastConnectAttempt_ == 0) {
            // Start reconnect after brief delay
            lastConnectAttempt_ = now;
        }

        if (state_ == WifiState::DISCONNECTED &&
            nvs_->hasWifiCredentials() &&
            lastConnectAttempt_ > 0 &&
            (now - lastConnectAttempt_) >= WIFI_RECONNECT_DELAY_MS) {
            autoConnect();
        }

        // SmartConfig timeout
        if (state_ == WifiState::SMARTCONFIG_WAITING) {
            if (WiFi.smartConfigDone()) {
                // SmartConfig succeeded
                String ssid = WiFi.SSID();
                String pass = WiFi.psk();
                nvs_->setWifiSsid(ssid.c_str());
                nvs_->setWifiPass(pass.c_str());
                setState(WifiState::CONNECTED);
                WiFi.stopSmartConfig();
            }
        }
    }

    // Try connecting with saved credentials
    bool autoConnect() {
        if (!nvs_->hasWifiCredentials()) return false;

        String ssid = nvs_->getWifiSsid();
        String pass = nvs_->getWifiPass();

        setState(WifiState::CONNECTING);
        WiFi.begin(ssid.c_str(), pass.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
                setState(WifiState::FAILED);
                lastConnectAttempt_ = millis();
                return false;
            }
            delay(200);
        }

        setState(WifiState::CONNECTED);
        lastConnectAttempt_ = 0;
        return true;
    }

    // Connect with specific credentials
    bool connect(const char* ssid, const char* pass) {
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

        // Save credentials on success
        nvs_->setWifiSsid(ssid);
        nvs_->setWifiPass(pass);
        setState(WifiState::CONNECTED);
        return true;
    }

    // Start SmartConfig for provisioning via ESP-Touch app
    void startSmartConfig() {
        setState(WifiState::SMARTCONFIG_WAITING);
        WiFi.beginSmartConfig();
    }

    void stopSmartConfig() {
        WiFi.stopSmartConfig();
        if (state_ == WifiState::SMARTCONFIG_WAITING) {
            setState(WifiState::DISCONNECTED);
        }
    }

    // Scan for available networks (blocking)
    int scanNetworks() {
        setState(WifiState::SCAN_RUNNING);
        int n = WiFi.scanNetworks(false, false, false, 300);
        if (n < 0) n = 0;
        if (n > MAX_WIFI_NETWORKS) n = MAX_WIFI_NETWORKS;
        if (scanCallback_) scanCallback_(n);
        setState(state_ == WifiState::CONNECTED ?
                 WifiState::CONNECTED : WifiState::DISCONNECTED);
        return n;
    }

    // Get scan result
    String getScannedSsid(int index) const {
        return WiFi.SSID(index);
    }

    int32_t getScannedRssi(int index) const {
        return WiFi.RSSI(index);
    }

    wifi_auth_mode_t getScannedAuth(int index) const {
        return WiFi.encryptionType(index);
    }

    // Free scan memory
    void clearScanResults() {
        WiFi.scanDelete();
    }

    // ─── Getters ──────────────────────────────────────────────────────────
    WifiState getState() const { return state_; }
    bool isConnected() const { return state_ == WifiState::CONNECTED; }
    String getLocalIp() const { return WiFi.localIP().toString(); }
    int8_t getRssi() const { return WiFi.RSSI(); }
    String getSsid() const { return WiFi.SSID(); }

    // ─── Callbacks ────────────────────────────────────────────────────────
    void onStateChange(WifiStateCallback cb) { stateCallback_ = cb; }
    void onScanComplete(WifiScanCallback cb) { scanCallback_ = cb; }

private:
    void setState(WifiState newState) {
        if (state_ != newState) {
            state_ = newState;
            if (stateCallback_) stateCallback_(state_);
        }
    }

    NvsStorage* nvs_;
    WifiState state_;
    unsigned long lastConnectAttempt_;
    WifiStateCallback stateCallback_;
    WifiScanCallback scanCallback_;
};

#endif // WIFI_MANAGER_H

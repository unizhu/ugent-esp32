/**
 * UGENT ESP32 Monitor — SSE Client
 *
 * Persistent SSE connection to /v1/ugent/events for real-time updates.
 * Falls back gracefully on disconnect with exponential backoff.
 */

#ifndef SSE_CLIENT_H
#define SSE_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "nvs_storage.h"

// ─── SSE Event Types ─────────────────────────────────────────────────────────

enum class SseEventType : uint8_t {
    TASK_STATUS_CHANGED,
    INTERACTION_REQUEST,
    TURN_STARTED,
    TURN_COMPLETED,
    TOOL_CALL_STARTED,
    CHAT_NOTIFICATION,
    CRON_JOB_TRIGGERED,
    MEMORY_UPDATED,
    UNKNOWN
};

struct SseEvent {
    SseEventType type;
    char data[MAX_SSE_DATA_LEN];   // Event payload (JSON string)
    char id[32];                   // Last event ID for reconnection
};

// Callbacks
typedef void (*SseEventCallback)(const SseEvent& event);
typedef void (*SseConnectionCallback)(bool connected);

class SseClient {
public:
    void begin(NvsStorage* nvs) {
        nvs_ = nvs;
        eventCallback_ = nullptr;
        connCallback_ = nullptr;
        connected_ = false;
        reconnectDelay_ = SSE_RECONNECT_BASE_MS;
        lastReconnectAttempt_ = 0;
        lastEventId_[0] = '\0';
        parserState_ = ParserState::IDLE;
        eventDataIdx_ = 0;
        eventType_[0] = '\0';
        eventData_[0] = '\0';
    }

    void loop() {
        if (!nvs_->hasUgentConfig()) return;

        // Maintain connection
        if (!connected_) {
            unsigned long now = millis();
            if (lastReconnectAttempt_ == 0 ||
                (now - lastReconnectAttempt_) >= reconnectDelay_) {
                lastReconnectAttempt_ = now;
                connect();
            }
            return;
        }

        // Read available data
        while (client_.available()) {
            String line = client_.readStringUntil('\n');
            parseLine(line);
        }

        // Check if still connected
        if (!client_.connected()) {
            disconnect();
        }
    }

    void stop() {
        client_.stop();
        connected_ = false;
    }

    bool isConnected() const { return connected_; }

    // ─── Callbacks ────────────────────────────────────────────────────────
    void onEvent(SseEventCallback cb) { eventCallback_ = cb; }
    void onConnectionChange(SseConnectionCallback cb) { connCallback_ = cb; }

private:
    bool connect() {
        String host = nvs_->getUgentHost();
        uint16_t port = nvs_->getUgentPort();
        String apiKey = nvs_->getUgentApiKey();

        String path = "/v1/ugent/events?route_key=esp32";
        if (strlen(lastEventId_) > 0) {
            path += "&Last-Event-ID=";
            path += lastEventId_;
        }

        if (!client_.connect(host.c_str(), port)) {
            return false;
        }

        // Send HTTP request
        client_.printf("GET %s HTTP/1.1\r\n", path.c_str());
        client_.printf("Host: %s:%d\r\n", host.c_str(), port);
        client_.println("Accept: text/event-stream");
        client_.println("Cache-Control: no-cache");
        client_.println("Connection: keep-alive");
        if (apiKey.length() > 0) {
            client_.printf("x-api-key: %s\r\n", apiKey.c_str());
        }
        client_.println();

        // Wait for response
        unsigned long start = millis();
        while (!client_.available()) {
            if (millis() - start > HTTP_TIMEOUT_MS) {
                client_.stop();
                return false;
            }
            delay(10);
        }

        // Read status line
        String statusLine = client_.readStringUntil('\n');
        if (!statusLine.startsWith("HTTP/1.1 200")) {
            client_.stop();
            return false;
        }

        // Skip headers
        while (client_.available()) {
            String header = client_.readStringUntil('\n');
            if (header == "\r" || header.length() == 0) break;
        }

        connected_ = true;
        reconnectDelay_ = SSE_RECONNECT_BASE_MS; // Reset backoff on success
        if (connCallback_) connCallback_(true);

        parserState_ = ParserState::IDLE;
        return true;
    }

    void disconnect() {
        client_.stop();
        connected_ = false;

        // Exponential backoff
        if (reconnectDelay_ < SSE_RECONNECT_MAX_MS) {
            reconnectDelay_ *= 2;
        }

        if (connCallback_) connCallback_(false);
    }

    // ─── SSE Parser ───────────────────────────────────────────────────────

    enum class ParserState { IDLE, IN_EVENT };

    void parseLine(const String& line) {
        // Empty line = dispatch event
        if (line == "\r" || line.length() == 0) {
            if (parserState_ == ParserState::IN_EVENT && eventDataIdx_ > 0) {
                dispatchEvent();
            }
            parserState_ = ParserState::IDLE;
            return;
        }

        // Comment (keep-alive)
        if (line.startsWith(":")) return;

        // Field: value
        int colonPos = line.indexOf(':');
        if (colonPos < 0) return;

        String field = line.substring(0, colonPos);
        String value = line.substring(colonPos + 1);
        // Trim leading space
        if (value.startsWith(" ")) value = value.substring(1);
        // Trim trailing \r
        if (value.endsWith("\r")) value = value.substring(0, value.length() - 1);

        if (field == "event") {
            parserState_ = ParserState::IN_EVENT;
            strncpy(eventType_, value.c_str(), sizeof(eventType_) - 1);
            eventType_[sizeof(eventType_) - 1] = '\0';
        } else if (field == "data") {
            parserState_ = ParserState::IN_EVENT;
            size_t len = min((size_t)value.length(),
                             sizeof(eventData_) - eventDataIdx_ - 1);
            memcpy(eventData_ + eventDataIdx_, value.c_str(), len);
            eventDataIdx_ += len;
            eventData_[eventDataIdx_] = '\0';
        } else if (field == "id") {
            strncpy(lastEventId_, value.c_str(), sizeof(lastEventId_) - 1);
        } else if (field == "retry") {
            // Could parse reconnect interval, but we use our own backoff
        }
    }

    void dispatchEvent() {
        SseEvent event;
        event.type = parseEventType(eventType_);
        strncpy(event.data, eventData_, sizeof(event.data) - 1);
        strncpy(event.id, lastEventId_, sizeof(event.id) - 1);

        // Reset parser state for next event
        eventDataIdx_ = 0;
        eventData_[0] = '\0';
        eventType_[0] = '\0';

        if (eventCallback_) eventCallback_(event);
    }

    static SseEventType parseEventType(const char* type) {
        if (strcmp(type, "TaskStatusChanged") == 0)   return SseEventType::TASK_STATUS_CHANGED;
        if (strcmp(type, "InteractionRequest") == 0)   return SseEventType::INTERACTION_REQUEST;
        if (strcmp(type, "TurnStarted") == 0)          return SseEventType::TURN_STARTED;
        if (strcmp(type, "TurnCompleted") == 0)        return SseEventType::TURN_COMPLETED;
        if (strcmp(type, "ToolCallStarted") == 0)      return SseEventType::TOOL_CALL_STARTED;
        if (strcmp(type, "ChatNotification") == 0)     return SseEventType::CHAT_NOTIFICATION;
        if (strcmp(type, "CronJobTriggered") == 0)     return SseEventType::CRON_JOB_TRIGGERED;
        if (strcmp(type, "MemoryUpdated") == 0)        return SseEventType::MEMORY_UPDATED;
        return SseEventType::UNKNOWN;
    }

    NvsStorage* nvs_;
    WiFiClient client_;
    bool connected_;
    unsigned long lastReconnectAttempt_;
    unsigned long reconnectDelay_;

    // Parser state
    ParserState parserState_;
    char eventType_[64];
    char eventData_[MAX_SSE_DATA_LEN];
    size_t eventDataIdx_;
    char lastEventId_[32];

    SseEventCallback eventCallback_;
    SseConnectionCallback connCallback_;
};

#endif // SSE_CLIENT_H

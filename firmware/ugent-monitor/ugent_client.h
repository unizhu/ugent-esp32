/**
 * UGENT ESP32 Monitor — UGENT HTTP Client
 *
 * Polls /v1/ugent/status and parses JSON response.
 * Sends commands via POST /v1/ugent/commands.
 */

#ifndef UGENT_CLIENT_H
#define UGENT_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "nvs_storage.h"

// ─── Data Structures ─────────────────────────────────────────────────────────

enum class TaskStatus : uint8_t {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    CANCELLED,
    SKIPPED,
    UNKNOWN
};

struct TaskInfo {
    char id[24];
    char name[MAX_TASK_NAME];
    TaskStatus status;
    uint8_t progress; // 0–100
};

struct UgentStatus {
    char version[MAX_VERSION_LEN];
    bool agentRunning;
    uint8_t sessionsActive;
    uint8_t tasksRunning;
    uint8_t tasksCompleted;
    uint8_t tasksFailed;
    uint8_t tasksPending;
    uint16_t memoryEntries;
    uint8_t cronJobs;
    char nextCron[32];
    uint8_t skillsLoaded;
    uint8_t pluginsRunning;
    bool swarmActive;

    TaskInfo tasks[MAX_TASKS_DISPLAY];
    uint8_t taskCount;
};

// Callback when new status is received
typedef void (*StatusCallback)(const UgentStatus& status);
typedef void (*CommandResultCallback)(bool success, const char* response);

class UgentClient {
public:
    void begin(NvsStorage* nvs) {
        nvs_ = nvs;
        statusCallback_ = nullptr;
        cmdResultCallback_ = nullptr;
        lastPoll_ = 0;
        connected_ = false;
        memset(&status_, 0, sizeof(status_));
    }

    void loop() {
        if (!nvs_->hasUgentConfig()) return;

        unsigned long now = millis();
        if (now - lastPoll_ >= STATUS_POLL_INTERVAL_MS || lastPoll_ == 0) {
            lastPoll_ = now;
            fetchStatus();
        }
    }

    // ─── Fetch Status ─────────────────────────────────────────────────────
    bool fetchStatus() {
        String url = buildUrl("/v1/ugent/status");
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.begin(url);

        String apiKey = nvs_->getUgentApiKey();
        if (apiKey.length() > 0) {
            http.addHeader("x-api-key", apiKey);
        }

        int code = http.GET();
        if (code != 200) {
            http.end();
            connected_ = false;
            return false;
        }

        String body = http.getString();
        http.end();
        connected_ = true;

        // Parse JSON
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            return false;
        }

        parseStatus(doc);
        if (statusCallback_) statusCallback_(status_);
        return true;
    }

    // ─── Test Connection ──────────────────────────────────────────────────
    bool testConnection() {
        String url = buildUrl("/v1/health");
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.begin(url);

        String apiKey = nvs_->getUgentApiKey();
        if (apiKey.length() > 0) {
            http.addHeader("x-api-key", apiKey);
        }

        int code = http.GET();
        http.end();
        connected_ = (code == 200);
        return connected_;
    }

    // ─── Send Command ─────────────────────────────────────────────────────
    bool sendCommand(const char* command, const char* args = "") {
        String url = buildUrl("/v1/ugent/commands");
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String apiKey = nvs_->getUgentApiKey();
        if (apiKey.length() > 0) {
            http.addHeader("x-api-key", apiKey);
        }

        // Build JSON body
        JsonDocument doc;
        doc["command"] = command;
        if (strlen(args) > 0) {
            doc["args"] = args;
        }

        String body;
        serializeJson(doc, body);

        int code = http.POST(body);
        http.end();

        bool success = (code == 200 || code == 202);
        if (cmdResultCallback_) cmdResultCallback_(success, code == 200 ? "OK" : "FAIL");
        return success;
    }

    // ─── Send Interaction Response ────────────────────────────────────────
    bool sendInteractionResponse(const char* response) {
        return sendCommand("/interact", response);
    }

    bool approveAction() {
        return sendCommand("/approve");
    }

    bool rejectAction() {
        return sendCommand("/reject");
    }

    // ─── Getters ──────────────────────────────────────────────────────────
    const UgentStatus& getStatus() const { return status_; }
    bool isConnected() const { return connected_; }

    // ─── Callbacks ────────────────────────────────────────────────────────
    void onStatus(StatusCallback cb) { statusCallback_ = cb; }
    void onCommandResult(CommandResultCallback cb) { cmdResultCallback_ = cb; }

private:
    String buildUrl(const char* path) const {
        String url = "http://";
        url += nvs_->getUgentHost();
        url += ":";
        url += nvs_->getUgentPort();
        url += path;
        return url;
    }

    void parseStatus(JsonDocument& doc) {
        // Version
        const char* ver = doc["version"] | "unknown";
        strncpy(status_.version, ver, MAX_VERSION_LEN - 1);

        // Agent
        status_.agentRunning = doc["session"]["active_sessions"] | 0;
        status_.sessionsActive = doc["session"]["active_sessions"] | 0;

        // Tasks summary
        status_.tasksRunning   = doc["tasks"]["running"]   | 0;
        status_.tasksCompleted = doc["tasks"]["completed"] | 0;
        status_.tasksFailed    = doc["tasks"]["failed"]    | 0;
        status_.tasksPending   = doc["tasks"]["pending"]   | 0;

        // Tasks list
        status_.taskCount = 0;
        if (doc["tasks"].containsKey("task_list")) {
            JsonArray tasks = doc["tasks"]["task_list"].as<JsonArray>();
            uint8_t count = min((int)tasks.size(), MAX_TASKS_DISPLAY);
            for (uint8_t i = 0; i < count; i++) {
                JsonObject t = tasks[i];
                const char* id = t["id"] | "";
                const char* name = t["name"] | "unnamed";
                const char* st = t["status"] | "unknown";

                strncpy(status_.tasks[i].id, id, sizeof(status_.tasks[i].id) - 1);
                strncpy(status_.tasks[i].name, name, MAX_TASK_NAME - 1);
                status_.tasks[i].progress = t["progress"] | 0;
                status_.tasks[i].status = parseTaskStatus(st);
            }
            status_.taskCount = count;
        }

        // Memory
        status_.memoryEntries = doc["memory"]["total_entries"] | 0;

        // Cron
        status_.cronJobs = doc["cron"]["total_jobs"] | 0;
        if (doc["cron"].containsKey("next_run")) {
            const char* nr = doc["cron"]["next_run"] | "";
            strncpy(status_.nextCron, nr, sizeof(status_.nextCron) - 1);
        }

        // Skills & Plugins
        status_.skillsLoaded  = doc["skills"]["loaded"]  | 0;
        status_.pluginsRunning = doc["plugins"]["running"] | 0;

        // Swarm
        status_.swarmActive = doc["swarm"]["active"] | false;
    }

    static TaskStatus parseTaskStatus(const char* s) {
        if (strcmp(s, "pending") == 0)     return TaskStatus::PENDING;
        if (strcmp(s, "in_progress") == 0) return TaskStatus::IN_PROGRESS;
        if (strcmp(s, "completed") == 0)   return TaskStatus::COMPLETED;
        if (strcmp(s, "failed") == 0)      return TaskStatus::FAILED;
        if (strcmp(s, "cancelled") == 0)   return TaskStatus::CANCELLED;
        if (strcmp(s, "skipped") == 0)     return TaskStatus::SKIPPED;
        return TaskStatus::UNKNOWN;
    }

    NvsStorage* nvs_;
    UgentStatus status_;
    unsigned long lastPoll_;
    bool connected_;
    StatusCallback statusCallback_;
    CommandResultCallback cmdResultCallback_;
};

#endif // UGENT_CLIENT_H

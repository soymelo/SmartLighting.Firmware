#include "TelemetryService.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_system.h>
#include <time.h>
#include <cstring>

#include "../../DeviceConfig.h"

namespace {
constexpr uint8_t MaxBatch = 50;
constexpr uint8_t QueueCapacity = 64;
constexpr unsigned long RetryMs = 5000;
constexpr time_t EarliestValidUtc = 1735689600; // 2025-01-01

enum class EventType : uint8_t { Sensor, Luminaire };
struct Event {
  EventType type;
  char identifier[37];
  char componentId[37];
  unsigned long occurredMs;
  uint8_t brightness;
};

QueueHandle_t eventQueue = nullptr;
Event pending[MaxBatch];
uint8_t pendingCount = 0;
unsigned long lastAttempt = 0;
unsigned long lastSuccess = 0;

void generateUuid(char (&destination)[37]) {
  uint8_t bytes[16];
  for (uint8_t& byte : bytes) byte = static_cast<uint8_t>(esp_random());
  bytes[6] = (bytes[6] & 0x0F) | 0x40;
  bytes[8] = (bytes[8] & 0x3F) | 0x80;
  snprintf(destination, sizeof(destination),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
           bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
           bytes[12], bytes[13], bytes[14], bytes[15]);
}

bool formatUtc(time_t utc, char (&destination)[21]) {
  struct tm value;
  if (gmtime_r(&utc, &value) == nullptr) return false;
  return strftime(destination, sizeof(destination),
                  "%Y-%m-%dT%H:%M:%SZ", &value) == 20;
}

void queueEvent(EventType type, const char* id, uint8_t brightness) {
  if (eventQueue == nullptr || id == nullptr || strlen(id) != 36) return;
  Event event = {};
  event.type = type;
  generateUuid(event.identifier);
  strcpy(event.componentId, id);
  event.occurredMs = millis();
  event.brightness = brightness;
  if (xQueueSend(eventQueue, &event, 0) != pdTRUE)
    Serial.println("Fila de telemetria cheia: evento descartado.");
}
}

void TelemetryService::begin() {
  eventQueue = xQueueCreate(QueueCapacity, sizeof(Event));
  if (eventQueue == nullptr) Serial.println("Falha ao criar fila de telemetria.");
  lastSuccess = millis();
}

void TelemetryService::queueSensorEvent(const char* sensorId) {
  queueEvent(EventType::Sensor, sensorId, 0);
}

void TelemetryService::queueLuminaireChange(const char* luminaireId,
                                            uint8_t brightnessPercentage) {
  queueEvent(EventType::Luminaire, luminaireId, brightnessPercentage);
}

void TelemetryService::update() {
  if (eventQueue == nullptr || WiFi.status() != WL_CONNECTED ||
      DeviceConfig::DeviceId[0] == '\0') return;
  const time_t nowUtc = time(nullptr);
  if (nowUtc < EarliestValidUtc) return;

  while (pendingCount < MaxBatch &&
         xQueueReceive(eventQueue, &pending[pendingCount], 0) == pdTRUE) {
    ++pendingCount;
  }

  const unsigned long nowMs = millis();
  if (pendingCount == 0 &&
      nowMs - lastSuccess < DeviceConfig::HeartbeatMs) return;
  if (lastAttempt != 0 && nowMs - lastAttempt < RetryMs) return;
  lastAttempt = nowMs;

  cJSON* root = cJSON_CreateObject();
  cJSON* sensors = cJSON_AddArrayToObject(root, "sensorEvents");
  cJSON* luminaires = cJSON_AddArrayToObject(root, "luminaireStateChanges");
  char timestamp[21];
  if (root == nullptr || sensors == nullptr || luminaires == nullptr ||
      !formatUtc(nowUtc, timestamp)) {
    cJSON_Delete(root);
    return;
  }
  cJSON_AddStringToObject(root, "sentAtUtc", timestamp);

  bool valid = true;
  for (uint8_t index = 0; index < pendingCount; ++index) {
    const Event& event = pending[index];
    const time_t occurredUtc = nowUtc -
      static_cast<time_t>((nowMs - event.occurredMs) / 1000UL);
    if (!formatUtc(occurredUtc, timestamp)) { valid = false; break; }
    cJSON* item = cJSON_CreateObject();
    if (item == nullptr) { valid = false; break; }
    cJSON_AddStringToObject(item, "eventIdentifier", event.identifier);
    if (event.type == EventType::Sensor) {
      cJSON_AddStringToObject(item, "sensorId", event.componentId);
      cJSON_AddStringToObject(item, "occurredAtUtc", timestamp);
      cJSON_AddItemToArray(sensors, item);
    } else {
      cJSON_AddStringToObject(item, "luminaireId", event.componentId);
      cJSON_AddNumberToObject(item, "brightnessPercentage", event.brightness);
      cJSON_AddStringToObject(item, "changedAtUtc", timestamp);
      cJSON_AddItemToArray(luminaires, item);
    }
  }
  char* body = valid ? cJSON_PrintUnformatted(root) : nullptr;
  cJSON_Delete(root);
  if (body == nullptr) return;

  WiFiClient client;
  HTTPClient http;
  const String url = String(DeviceConfig::ApiBaseUrl) + "/devices/" +
                     DeviceConfig::DeviceId + "/telemetry";
  int status = -1;
  String errorResponse;
  if (http.begin(client, url)) {
    http.setTimeout(1500);
    http.addHeader("Content-Type", "application/json");
    status = http.POST(reinterpret_cast<uint8_t*>(body), strlen(body));
    if (status != HTTP_CODE_OK && status > 0)
      errorResponse = http.getString();
    http.end();
  }
  cJSON_free(body);
  if (status == HTTP_CODE_OK) {
    Serial.printf("Telemetria enviada: %u evento(s).\n", pendingCount);
    pendingCount = 0;
    lastSuccess = nowMs;
  } else {
    Serial.printf("Falha ao enviar telemetria (HTTP %d); nova tentativa pendente.\n", status);
    if (!errorResponse.isEmpty())
      Serial.println(errorResponse.substring(0, 300));
  }
}

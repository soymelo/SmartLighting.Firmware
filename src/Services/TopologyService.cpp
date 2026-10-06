#include "TopologyService.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cJSON.h>
#include <cstring>

#include "../../DeviceConfig.h"

namespace {

bool readGuid(const cJSON* object, const char* key, char (&destination)[37]) {
  const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!cJSON_IsString(value) || value->valuestring == nullptr ||
      strlen(value->valuestring) != 36) {
    return false;
  }
  strcpy(destination, value->valuestring);
  return true;
}

bool readSlotAndGpio(const cJSON* object, int maxSlot, bool sensor,
                     uint8_t& slot, uint8_t& gpio) {
  const cJSON* slotValue = cJSON_GetObjectItemCaseSensitive(object, "slot");
  const cJSON* gpioValue = cJSON_GetObjectItemCaseSensitive(object, "gpioNumber");
  if (!cJSON_IsNumber(slotValue) || !cJSON_IsNumber(gpioValue) ||
      slotValue->valuedouble != slotValue->valueint ||
      gpioValue->valuedouble != gpioValue->valueint ||
      slotValue->valueint < 1 || slotValue->valueint > maxSlot) {
    return false;
  }

  const int pin = gpioValue->valueint;
  const bool allowed = sensor
    ? (pin == 32 || pin == 34 || pin == 35)
    : (pin == 18 || pin == 19 || pin == 21 || pin == 25 || pin == 26 || pin == 27);
  if (!allowed) return false;

  slot = static_cast<uint8_t>(slotValue->valueint);
  gpio = static_cast<uint8_t>(pin);
  return true;
}

bool parse(const String& json, TopologyConfig& topology) {
  cJSON* root = cJSON_Parse(json.c_str());
  if (root == nullptr) return false;

  TopologyConfig next;
  bool valid = readGuid(root, "deviceId", next.deviceId) &&
               String(next.deviceId).equalsIgnoreCase(DeviceConfig::DeviceId);
  const cJSON* zones = cJSON_GetObjectItemCaseSensitive(root, "zones");
  valid = valid && cJSON_IsArray(zones);

  bool sensorSlots[4] = {};
  bool luminaireSlots[7] = {};
  bool usedPins[40] = {};

  if (valid) {
    const cJSON* zone = nullptr;
    cJSON_ArrayForEach(zone, zones) {
      char zoneId[37] = {};
      const cJSON* sensors = cJSON_GetObjectItemCaseSensitive(zone, "sensors");
      const cJSON* luminaires = cJSON_GetObjectItemCaseSensitive(zone, "luminaires");
      if (!readGuid(zone, "zoneId", zoneId) ||
          !cJSON_IsArray(sensors) || !cJSON_IsArray(luminaires)) {
        valid = false;
        break;
      }

      const cJSON* item = nullptr;
      cJSON_ArrayForEach(item, sensors) {
        if (next.sensorCount >= 3) { valid = false; break; }
        SensorConfig& sensor = next.sensors[next.sensorCount];
        valid = readGuid(item, "sensorId", sensor.id) &&
                readSlotAndGpio(item, 3, true, sensor.slot, sensor.gpio);
        if (!valid || sensorSlots[sensor.slot] || usedPins[sensor.gpio]) {
          valid = false;
          break;
        }
        strcpy(sensor.zoneId, zoneId);
        sensorSlots[sensor.slot] = true;
        usedPins[sensor.gpio] = true;
        ++next.sensorCount;
      }
      if (!valid) break;

      cJSON_ArrayForEach(item, luminaires) {
        if (next.luminaireCount >= 6) { valid = false; break; }
        LuminaireConfig& luminaire = next.luminaires[next.luminaireCount];
        valid = readGuid(item, "luminaireId", luminaire.id) &&
                readSlotAndGpio(item, 6, false, luminaire.slot, luminaire.gpio);
        if (!valid || luminaireSlots[luminaire.slot] || usedPins[luminaire.gpio]) {
          valid = false;
          break;
        }
        strcpy(luminaire.zoneId, zoneId);
        luminaireSlots[luminaire.slot] = true;
        usedPins[luminaire.gpio] = true;
        ++next.luminaireCount;
      }
      if (!valid) break;
    }
  }
  cJSON_Delete(root);
  if (valid) topology = next;
  return valid;
}

} // namespace

bool TopologyService::loadCached(TopologyConfig& topology) {
  Preferences preferences;
  if (!preferences.begin("smart-light", true)) return false;
  const String json = preferences.getString("topology", "");
  preferences.end();
  return json.length() > 0 && parse(json, topology);
}

bool TopologyService::fetch(TopologyConfig& topology) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  const String url = String(DeviceConfig::ApiBaseUrl) + "/devices/" +
                            DeviceConfig::DeviceId + "/topology";
  if (!http.begin(client, url)) return false;
  http.setTimeout(1500);
  const int status = http.GET();
  const String json = status == HTTP_CODE_OK ? http.getString() : "";
  http.end();
  if (status != HTTP_CODE_OK || !parse(json, topology)) {
    Serial.printf("Topologia rejeitada (HTTP %d).\n", status);
    return false;
  }

  Preferences preferences;
  if (preferences.begin("smart-light", false)) {
    if (preferences.getString("topology", "") != json)
      preferences.putString("topology", json);
    preferences.end();
  }
  return true;
}

void TopologyService::print(const TopologyConfig& topology) {
  Serial.printf("Dispositivo %s: %u sensor(es), %u luminaria(s)\n",
                topology.deviceId, topology.sensorCount, topology.luminaireCount);
  for (const SensorConfig& sensor : topology.sensors) {
    if (sensor.slot == 0) continue;
    Serial.printf("Sensor %u | ID %s | zona %s | GPIO %u\n",
                  sensor.slot, sensor.id, sensor.zoneId, sensor.gpio);
  }
  for (const LuminaireConfig& luminaire : topology.luminaires) {
    if (luminaire.slot == 0) continue;
    Serial.printf("LED %u | ID %s | zona %s | GPIO %u\n",
                  luminaire.slot, luminaire.id, luminaire.zoneId, luminaire.gpio);
  }
}

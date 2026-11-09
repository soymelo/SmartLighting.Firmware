#include "TopologyService.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cJSON.h>
#include <cstddef>
#include <cstring>

#include "../../DeviceConfig.h"

namespace {
constexpr char CacheKeyV2[] = "topology-v2";
constexpr char CacheKeyV3[] = "topology-v3";

// Layout binário gravado pelas versões anteriores. Não alterar: permite
// recuperar o acionamento local mesmo ao iniciar sem API após a atualização.
struct LegacyTopologyV2 {
  char deviceId[37] = {};
  SensorConfig sensors[3] = {};
  LuminaireConfig luminaires[6] = {};
  ControlSensorConfig controlSensors[6] = {};
  uint8_t luminaireSensorMask[7] = {};
  uint8_t sensorCount = 0;
  uint8_t luminaireCount = 0;
  uint8_t controlSensorCount = 0;
};

static_assert(sizeof(LegacyTopologyV2) == 959);
static_assert(offsetof(TopologyConfig, zones) == sizeof(LegacyTopologyV2));

template <typename T>
bool validBase(const T& cached) {
  return memchr(cached.deviceId, '\0', sizeof(cached.deviceId)) != nullptr &&
         String(cached.deviceId).equalsIgnoreCase(DeviceConfig::DeviceId) &&
         cached.sensorCount <= 3 && cached.luminaireCount <= 6 &&
         cached.controlSensorCount <= 6;
}

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

  TopologyConfig next{};
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
      if (next.zoneCount >= TopologyConfig::MaxZones) {
        valid = false;
        break;
      }
      for (uint8_t index = 0; index < next.zoneCount; ++index)
        if (String(next.zones[index].zoneId).equalsIgnoreCase(zoneId)) {
          valid = false;
          break;
        }
      if (!valid) break;

      uint8_t zoneSensorMask = 0;
      uint8_t zoneLuminaireMask = 0;
      const cJSON* item = nullptr;
      cJSON_ArrayForEach(item, sensors) {
        char sensorId[37] = {};
        char ownerId[37] = {};
        uint8_t slot = 0;
        uint8_t gpio = 0;
        valid = readGuid(item, "sensorId", sensorId) &&
                readGuid(item, "deviceId", ownerId) &&
                readSlotAndGpio(item, 3, true, slot, gpio);
        if (!valid) break;

        uint8_t index = 0;
        while (index < next.controlSensorCount &&
               !String(next.controlSensors[index].id).equalsIgnoreCase(sensorId))
          ++index;
        if (index == next.controlSensorCount) {
          if (next.controlSensorCount >= 6) { valid = false; break; }
          ControlSensorConfig& controlSensor = next.controlSensors[index];
          strcpy(controlSensor.id, sensorId);
          strcpy(controlSensor.deviceId, ownerId);
          controlSensor.slot = slot;
          ++next.controlSensorCount;
        } else if (!String(next.controlSensors[index].deviceId).equalsIgnoreCase(ownerId) ||
                   next.controlSensors[index].slot != slot) {
          valid = false;
          break;
        }
        zoneSensorMask |= 1U << index;

        if (!String(ownerId).equalsIgnoreCase(next.deviceId)) continue;
        if (sensorSlots[slot] || usedPins[gpio] || next.sensorCount >= 3) {
          valid = false;
          break;
        }
        SensorConfig& sensor = next.sensors[next.sensorCount++];
        strcpy(sensor.id, sensorId);
        strcpy(sensor.zoneId, zoneId);
        sensor.slot = slot;
        sensor.gpio = gpio;
        sensorSlots[slot] = true;
        usedPins[gpio] = true;
      }
      if (!valid) break;

      cJSON_ArrayForEach(item, luminaires) {
        char luminaireId[37] = {};
        char ownerId[37] = {};
        uint8_t slot = 0;
        uint8_t gpio = 0;
        valid = readGuid(item, "luminaireId", luminaireId) &&
                readGuid(item, "deviceId", ownerId) &&
                readSlotAndGpio(item, 6, false, slot, gpio);
        if (!valid) break;
        if (!String(ownerId).equalsIgnoreCase(next.deviceId)) continue;

        uint8_t index = 0;
        while (index < next.luminaireCount &&
               !String(next.luminaires[index].id).equalsIgnoreCase(luminaireId))
          ++index;
        if (index == next.luminaireCount) {
          if (next.luminaireCount >= 6 || luminaireSlots[slot] || usedPins[gpio]) {
            valid = false;
            break;
          }
          LuminaireConfig& luminaire = next.luminaires[next.luminaireCount++];
          strcpy(luminaire.id, luminaireId);
          luminaire.slot = slot;
          luminaire.gpio = gpio;
          luminaireSlots[slot] = true;
          usedPins[gpio] = true;
        } else if (next.luminaires[index].slot != slot ||
                   next.luminaires[index].gpio != gpio) {
          valid = false;
          break;
        }
        zoneLuminaireMask |= 1U << (slot - 1);
      }
      if (!valid) break;
      ZoneConfig& zoneConfig = next.zones[next.zoneCount++];
      strcpy(zoneConfig.zoneId, zoneId);
      zoneConfig.controlSensorMask = zoneSensorMask;
      zoneConfig.localLuminaireMask = zoneLuminaireMask;
      for (uint8_t slot = 1; slot <= 6; ++slot)
        if (zoneLuminaireMask & (1U << (slot - 1)))
          next.luminaireSensorMask[slot] |= zoneSensorMask;
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
  TopologyConfig current{};
  const bool loadedCurrent =
      preferences.getBytesLength(CacheKeyV3) == sizeof(current) &&
      preferences.getBytes(CacheKeyV3, &current, sizeof(current)) == sizeof(current) &&
      validBase(current) && current.zoneCount <= TopologyConfig::MaxZones;
  if (loadedCurrent) {
    preferences.end();
    topology = current;
    return true;
  }

  LegacyTopologyV2 previous{};
  const bool loadedPrevious =
      preferences.getBytesLength(CacheKeyV2) == sizeof(previous) &&
      preferences.getBytes(CacheKeyV2, &previous, sizeof(previous)) == sizeof(previous) &&
      validBase(previous);
  preferences.end();
  if (!loadedPrevious) return false;

  TopologyConfig migrated{};
  memcpy(&migrated, &previous, sizeof(previous));
  topology = migrated;
  Serial.println("Cache v2 recuperado: sensores ativos; zonas ML aguardam a API.");
  return true;
}

bool TopologyService::fetch(TopologyConfig& topology) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  HTTPClient http;
  const String url = String(DeviceConfig::ApiBaseUrl) + "/devices/" +
                            DeviceConfig::DeviceId + "/topology";
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  const int status = http.GET();
  const String json = status == HTTP_CODE_OK ? http.getString() : "";
  http.end();
  if (status != HTTP_CODE_OK) {
    if (status < 0)
      Serial.printf("Falha ao buscar topologia: %s (HTTP %d).\n",
                    HTTPClient::errorToString(status).c_str(), status);
    else
      Serial.printf("Topologia rejeitada pela API (HTTP %d).\n", status);
    return false;
  }
  if (!parse(json, topology)) {
    Serial.printf("Topologia recebida, mas invalida (%u bytes).\n",
                  static_cast<unsigned>(json.length()));
    return false;
  }

  Preferences preferences;
  if (preferences.begin("smart-light", false)) {
    // O JSON do projeto pode ultrapassar o limite de strings da NVS.
    if (preferences.isKey("topology")) preferences.remove("topology");
    TopologyConfig cached{};
    const bool unchanged = preferences.getBytesLength(CacheKeyV3) == sizeof(cached) &&
                           preferences.getBytes(CacheKeyV3, &cached, sizeof(cached)) == sizeof(cached) &&
                           memcmp(&cached, &topology, sizeof(topology)) == 0;
    if (!unchanged && preferences.putBytes(CacheKeyV3, &topology, sizeof(topology)) != sizeof(topology))
      Serial.println("Nao foi possivel salvar a topologia local.");
    preferences.end();
  }
  return true;
}

void TopologyService::print(const TopologyConfig& topology) {
  Serial.printf("Dispositivo %s: %u sensor(es), %u luminaria(s), %u zona(s) ML\n",
                topology.deviceId, topology.sensorCount, topology.luminaireCount,
                topology.zoneCount);
  for (uint8_t index = 0; index < topology.zoneCount; ++index)
    Serial.printf("Zona %s | sensores 0x%02X | LEDs locais 0x%02X\n",
                  topology.zones[index].zoneId,
                  topology.zones[index].controlSensorMask,
                  topology.zones[index].localLuminaireMask);
  for (const SensorConfig& sensor : topology.sensors) {
    if (sensor.slot == 0) continue;
    Serial.printf("Sensor %u | ID %s | zona %s | GPIO %u\n",
                  sensor.slot, sensor.id, sensor.zoneId, sensor.gpio);
  }
  for (const LuminaireConfig& luminaire : topology.luminaires) {
    if (luminaire.slot == 0) continue;
    Serial.printf("LED %u | ID %s | GPIO %u | sensores:",
                  luminaire.slot, luminaire.id, luminaire.gpio);
    for (uint8_t index = 0; index < topology.controlSensorCount; ++index)
      if (topology.luminaireSensorMask[luminaire.slot] & (1U << index))
        Serial.printf(" %s", topology.controlSensors[index].id);
    Serial.println();
  }
}

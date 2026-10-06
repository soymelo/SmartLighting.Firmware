#include "DeviceConfig.h"
#include "src/Models.h"
#include "src/Control/LightingController.h"
#include "src/Services/TelemetryService.h"
#include "src/Services/TopologyService.h"
#include "src/Services/WifiService.h"

#include <WiFi.h>
#include <cstring>

namespace {
QueueHandle_t topologyUpdates = nullptr;
TopologyConfig lastNetworkTopology;

void networkTask(void*) {
  WifiService::begin();
  unsigned long lastFetch = 0;
  for (;;) {
    WifiService::update();
    if (WiFi.status() == WL_CONNECTED &&
        DeviceConfig::DeviceId[0] != '\0' &&
        (lastFetch == 0 || millis() - lastFetch >= DeviceConfig::TopologyRefreshMs)) {
      lastFetch = millis();
      TopologyConfig downloaded;
      if (TopologyService::fetch(downloaded) &&
          memcmp(&downloaded, &lastNetworkTopology, sizeof(downloaded)) != 0) {
        lastNetworkTopology = downloaded;
        xQueueOverwrite(topologyUpdates, &downloaded);
      }
    }
    TelemetryService::update();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}

void setup() {
  Serial.begin(115200);
  LightingController::begin();
  TelemetryService::begin();
  topologyUpdates = xQueueCreate(1, sizeof(TopologyConfig));
  if (topologyUpdates == nullptr) {
    Serial.println("Falha ao criar fila de topologia.");
    return;
  }

  TopologyConfig cached;
  if (TopologyService::loadCached(cached)) {
    Serial.println("Topologia local carregada.");
    if (LightingController::applyTopology(cached))
      lastNetworkTopology = cached;
  } else {
    Serial.println("Sem topologia local: aguardando primeira resposta da API.");
  }

  if (DeviceConfig::DeviceId[0] == '\0')
    Serial.println("Configure DeviceId em DeviceConfig.h.");
  if (xTaskCreatePinnedToCore(networkTask, "network", 12288, nullptr, 1,
                              nullptr, 0) != pdPASS)
    Serial.println("Falha ao iniciar tarefa de rede; controle local continua ativo.");
}

void loop() {
  TopologyConfig downloaded;
  if (topologyUpdates != nullptr &&
      xQueueReceive(topologyUpdates, &downloaded, 0) == pdTRUE) {
    if (!LightingController::applyTopology(downloaded))
      Serial.println("Topologia recebida, mas os LEDs nao puderam ser iniciados.");
  }
  LightingController::update();
  delay(1);
}

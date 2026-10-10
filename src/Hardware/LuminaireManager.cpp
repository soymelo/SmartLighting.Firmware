#include "LuminaireManager.h"

namespace {
constexpr uint8_t BaseBrightness = 20;
constexpr uint8_t MaximumBrightness = 255;
constexpr unsigned long FadeIntervalMs = 10;
LuminaireConfig luminaires[6];
uint8_t count = 0;
uint8_t brightness[7] = {};
uint8_t lastReported[7] = {};
unsigned long lastFade[7] = {};
}

bool LuminaireManager::begin(const TopologyConfig& topology) {
  for (uint8_t index = 0; index < count; ++index) {
    ledcWrite(luminaires[index].gpio, 0);
    ledcDetach(luminaires[index].gpio);
    pinMode(luminaires[index].gpio, OUTPUT);
    digitalWrite(luminaires[index].gpio, LOW);
  }
  count = 0;
  for (uint8_t index = 0; index < topology.luminaireCount; ++index) {
    const LuminaireConfig& luminaire = topology.luminaires[index];
    if (!ledcAttach(luminaire.gpio, 5000, 8)) {
      Serial.printf("Falha ao configurar PWM no GPIO %u.\n", luminaire.gpio);
      return false;
    }
    luminaires[count++] = luminaire;
    brightness[luminaire.slot] = BaseBrightness;
    lastReported[luminaire.slot] = BaseBrightness;
    lastFade[luminaire.slot] = millis();
    ledcWrite(luminaire.gpio, BaseBrightness);
  }
  return true;
}

uint8_t LuminaireManager::update(unsigned long now, uint8_t activeLuminaires) {
  uint8_t changed = 0;
  for (uint8_t index = 0; index < count; ++index) {
    const LuminaireConfig& luminaire = luminaires[index];
    const uint8_t slot = luminaire.slot;
    const uint8_t target = (activeLuminaires & (1U << (slot - 1)))
                             ? MaximumBrightness : BaseBrightness;
    if (brightness[slot] != target && now - lastFade[slot] >= FadeIntervalMs) {
      brightness[slot] += brightness[slot] < target ? 1 : -1;
      ledcWrite(luminaire.gpio, brightness[slot]);
      lastFade[slot] = now;
    }
    // Registra a mudança quando o fade alcança o alvo, sem gerar um evento por passo.
    if (brightness[slot] == target && lastReported[slot] != target) {
      lastReported[slot] = target;
      changed |= 1U << (slot - 1);
    }
  }
  return changed;
}

uint8_t LuminaireManager::brightnessPercentage(uint8_t slot) {
  if (slot < 1 || slot > 6) return 0;
  return static_cast<uint8_t>((brightness[slot] * 100U + 127U) / 255U);
}

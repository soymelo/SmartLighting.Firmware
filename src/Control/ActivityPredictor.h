#pragma once

#include <cstdint>

namespace ActivityPredictor {

struct Prediction {
  double expectedCount = 0.0;
  double activityProbability = 0.0;
  uint8_t restPwm = 0;
};

// Recebe hora local (0–23), dia da semana (0=segunda, 6=domingo) e
// ativações dos 30 minutos já encerrados. Não lê sensores nem altera LEDs.
bool predict(uint8_t localHour, uint8_t localWeekday,
             uint32_t previous30mCount, Prediction& output);

}  // namespace ActivityPredictor

#pragma once

#include <cstdint>
#include <ctime>

namespace ActivityClock {

struct LocalTime {
  uint8_t hour = 0;
  uint8_t weekday = 0;  // 0=segunda, 6=domingo, como no treino.
};

// Os cenários usados para treinar os pesos atuais usam UTC-3 fixo.
bool validUtc(std::time_t utc);
bool fromUtc(std::time_t utc, LocalTime& output);

}  // namespace ActivityClock

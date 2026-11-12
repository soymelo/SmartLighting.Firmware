#include <cassert>
#include <ctime>

#include "../src/Control/AdaptiveRestController.h"

int main() {
  TopologyConfig topology{};
  topology.controlSensorCount = 2;
  topology.zoneCount = 2;
  topology.zones[0].controlSensorMask = 1U << 0;
  topology.zones[0].localLuminaireMask = (1U << 0) | (1U << 1);
  topology.zones[1].controlSensorMask = 1U << 1;
  topology.zones[1].localLuminaireMask = 1U << 1;

  ZoneActivityCounter history;
  AdaptiveRestController rest;
  history.reset(topology);
  rest.reset();

  // 06h UTC = 03h locais. Sem duas janelas completas, o repouso é padrão.
  constexpr std::time_t atThreeLocal = 1735711200;
  rest.update(topology, history, atThreeLocal);
  assert(!rest.predicted(0));
  assert(rest.luminairePwm(1) == 20);
  assert(rest.luminairePwm(2) == 20);

  // Z01 tem zero ativações; Z02 tem três nos 30 minutos anteriores.
  assert(history.advance(atThreeLocal - 30 * 60) ==
         ZoneActivityCounter::WindowChange::Started);
  assert(history.record(1) == 2);
  assert(history.advance(atThreeLocal - 15 * 60) ==
         ZoneActivityCounter::WindowChange::Completed);
  assert(history.record(1) == 2);
  assert(history.record(1) == 2);
  assert(history.advance(atThreeLocal) ==
         ZoneActivityCounter::WindowChange::Completed);
  rest.update(topology, history, atThreeLocal);
  assert(rest.predicted(0) && rest.zonePwm(0) == 10);
  assert(rest.predicted(1) && rest.zonePwm(1) == 20);
  assert(rest.luminairePwm(1) == 10);
  assert(rest.luminairePwm(2) == 20);  // Compartilhado: vence o maior pedido.
  assert(rest.luminairePwm(3) == 20);  // Sem zona associada.

  // Uma zona sem sensor pede padrão e prevalece sobre 10 no LED compartilhado.
  topology.zoneCount = 3;
  topology.zones[2].localLuminaireMask = 1U << 0;
  rest.update(topology, history, atThreeLocal);
  assert(!rest.predicted(2));
  assert(rest.zonePwm(2) == 20);
  assert(rest.luminairePwm(1) == 20);
  topology.zoneCount = 2;

  // Às 18h locais, uma ativação recente em cada zona leva ao PWM alto.
  constexpr std::time_t atEighteenLocal = 1735765200;
  history.reset(topology);
  history.advance(atEighteenLocal - 30 * 60);
  assert(history.record(0) == 1);
  assert(history.record(1) == 2);
  history.advance(atEighteenLocal - 15 * 60);
  history.advance(atEighteenLocal);
  rest.update(topology, history, atEighteenLocal);
  assert(rest.zonePwm(0) == 40 && rest.zonePwm(1) == 40);
  assert(rest.luminairePwm(1) == 40 && rest.luminairePwm(2) == 40);

  // Horário inválido volta ao padrão, inclusive após previsão alta.
  rest.update(topology, history, 0);
  assert(!rest.predicted(0));
  assert(rest.luminairePwm(1) == 20);
  assert(rest.luminairePwm(2) == 20);
}

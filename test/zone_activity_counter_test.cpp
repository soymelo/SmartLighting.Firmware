#include <cassert>
#include <cstdint>
#include <ctime>

#include "../src/Control/ActivityClock.h"
#include "../src/Control/ZoneActivityCounter.h"

int main() {
  constexpr std::time_t midnightUtc = 1735689600;  // 2025-01-01 00:00 UTC.
  ActivityClock::LocalTime local;
  assert(!ActivityClock::fromUtc(midnightUtc - 1, local));
  assert(ActivityClock::fromUtc(midnightUtc, local));
  assert(local.hour == 21);     // 31/12 às 21h no UTC-3.
  assert(local.weekday == 1);  // Terça-feira; segunda=0.

  TopologyConfig topology{};
  topology.zoneCount = 2;
  topology.controlSensorCount = 2;
  topology.zones[0].controlSensorMask = 1U << 0;
  topology.zones[1].controlSensorMask = 1U << 1;
  ZoneActivityCounter counter;
  counter.reset(topology);

  // Começar às 00h08 não transforma 00h00–00h15 em janela completa.
  assert(counter.advance(midnightUtc + 8 * 60) ==
         ZoneActivityCounter::WindowChange::Started);
  assert(!counter.currentWindowIsComplete());
  assert(counter.record(0) == 1);
  assert(counter.total(0) == 1);
  assert(counter.advance(midnightUtc + 15 * 60) ==
         ZoneActivityCounter::WindowChange::PartialDiscarded);

  // 00h15–00h30 tem duas ativações; 00h30–00h45 tem zero.
  assert(counter.record(0) == 1);
  assert(counter.record(0) == 1);
  assert(counter.advance(midnightUtc + 30 * 60) ==
         ZoneActivityCounter::WindowChange::Completed);
  uint32_t previous30m = 0;
  assert(counter.lastClosedCount(0) == 2);
  assert(!counter.previous30mCount(0, previous30m));
  assert(counter.advance(midnightUtc + 45 * 60) ==
         ZoneActivityCounter::WindowChange::Completed);
  assert(counter.lastClosedCount(0) == 0);
  assert(counter.previous30mCount(0, previous30m) && previous30m == 2);
  assert(counter.previous30mCount(1, previous30m) && previous30m == 0);

  // A ativação da janela atual não entra na previsão feita às 00h45.
  assert(counter.record(1) == 2);
  assert(counter.previous30mCount(1, previous30m) && previous30m == 0);
  assert(counter.advance(midnightUtc + 60 * 60) ==
         ZoneActivityCounter::WindowChange::Completed);
  assert(counter.previous30mCount(1, previous30m) && previous30m == 1);

  // Um relógio que anda para trás invalida o histórico das janelas.
  assert(counter.advance(midnightUtc + 59 * 60) ==
         ZoneActivityCounter::WindowChange::ClockReset);
  assert(!counter.previous30mCount(0, previous30m));
  assert(counter.advance(midnightUtc - 1) ==
         ZoneActivityCounter::WindowChange::ClockLost);
  assert(!counter.previous30mCount(0, previous30m));

  // Se a virada for observada com atraso, a janela seguinte é parcial e
  // não pode ser somada a janelas anteriores como se fossem consecutivas.
  counter.reset(topology);
  assert(counter.advance(midnightUtc) ==
         ZoneActivityCounter::WindowChange::Started);
  assert(counter.record(0) == 1);
  assert(counter.advance(midnightUtc + 15 * 60 + 2) ==
         ZoneActivityCounter::WindowChange::Completed);
  assert(!counter.currentWindowIsComplete());
  assert(counter.advance(midnightUtc + 30 * 60) ==
         ZoneActivityCounter::WindowChange::PartialDiscarded);
  assert(counter.advance(midnightUtc + 45 * 60) ==
         ZoneActivityCounter::WindowChange::Completed);
  assert(!counter.previous30mCount(0, previous30m));

  // Avançar mais de uma janela sem observação também invalida o histórico.
  counter.reset(topology);
  assert(counter.advance(midnightUtc) ==
         ZoneActivityCounter::WindowChange::Started);
  assert(counter.record(0) == 1);
  assert(counter.advance(midnightUtc + 30 * 60) ==
         ZoneActivityCounter::WindowChange::ClockReset);
  assert(!counter.previous30mCount(0, previous30m));

  // A topologia nova reinicia também os totais acumulados.
  counter.reset(topology);
  assert(counter.total(0) == 0);
  assert(counter.total(1) == 0);
}

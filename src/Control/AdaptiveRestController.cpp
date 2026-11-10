#include "AdaptiveRestController.h"

#include "ActivityClock.h"
#include "ActivityModelWeights.h"
#include "ActivityPredictor.h"

namespace {
constexpr uint8_t StandardPwm = ActivityModelWeights::StandardPwm;
static_assert(ActivityModelWeights::LowPwm > 0 &&
              ActivityModelWeights::LowPwm <= ActivityModelWeights::StandardPwm &&
              ActivityModelWeights::StandardPwm <= ActivityModelWeights::HighPwm &&
              ActivityModelWeights::HighPwm <= 255);
}

void AdaptiveRestController::reset() {
  predictedZones_ = 0;
  for (uint8_t index = 0; index < TopologyConfig::MaxZones; ++index) {
    zonePwm_[index] = StandardPwm;
    probabilities_[index] = 0.0;
  }
  for (uint8_t slot = 0; slot <= 6; ++slot) luminairePwm_[slot] = StandardPwm;
}

void AdaptiveRestController::update(const TopologyConfig& topology,
                                    const ZoneActivityCounter& history,
                                    std::time_t predictionAtUtc) {
  reset();
  ActivityClock::LocalTime local;
  if (!ActivityClock::fromUtc(predictionAtUtc, local)) return;

  for (uint8_t zone = 0; zone < topology.zoneCount; ++zone) {
    uint32_t count = 0;
    ActivityPredictor::Prediction prediction;
    if (topology.zones[zone].controlSensorMask == 0 ||
        !history.previous30mCount(zone, count) ||
        !ActivityPredictor::predict(local.hour, local.weekday, count, prediction))
      continue;
    zonePwm_[zone] = prediction.restPwm;
    probabilities_[zone] = prediction.activityProbability;
    predictedZones_ |= static_cast<uint16_t>(1U << zone);
  }

  for (uint8_t slot = 1; slot <= 6; ++slot) {
    uint8_t requested = 0;
    for (uint8_t zone = 0; zone < topology.zoneCount; ++zone)
      if (topology.zones[zone].localLuminaireMask & (1U << (slot - 1)) &&
          zonePwm_[zone] > requested)
        requested = zonePwm_[zone];
    // Sem zona associada, não há previsão aplicável para esse LED.
    luminairePwm_[slot] = requested == 0 ? StandardPwm : requested;
  }
}

uint8_t AdaptiveRestController::zonePwm(uint8_t zoneIndex) const {
  return zoneIndex < TopologyConfig::MaxZones ? zonePwm_[zoneIndex] : StandardPwm;
}

bool AdaptiveRestController::predicted(uint8_t zoneIndex) const {
  return zoneIndex < TopologyConfig::MaxZones &&
         (predictedZones_ & (1U << zoneIndex)) != 0;
}

double AdaptiveRestController::probability(uint8_t zoneIndex) const {
  return zoneIndex < TopologyConfig::MaxZones ? probabilities_[zoneIndex] : 0.0;
}

uint8_t AdaptiveRestController::luminairePwm(uint8_t slot) const {
  return slot >= 1 && slot <= 6 ? luminairePwm_[slot] : StandardPwm;
}

const uint8_t (&AdaptiveRestController::luminairePwms() const)[7] {
  return luminairePwm_;
}

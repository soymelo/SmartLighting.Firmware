#include "ActivityPredictor.h"

#include <cmath>

#include "ActivityModelWeights.h"

namespace {
constexpr double MinScore = -10.0;
constexpr double MaxScore = 5.0;

static_assert(ActivityModelWeights::WeightCount == 27);
static_assert(sizeof(ActivityModelWeights::Values) /
                  sizeof(ActivityModelWeights::Values[0]) == ActivityModelWeights::WeightCount);
static_assert(ActivityModelWeights::HourStartIndex + 24 == ActivityModelWeights::WeekendIndex);
static_assert(ActivityModelWeights::WeekendIndex + 1 == ActivityModelWeights::Previous30mIndex);
static_assert(ActivityModelWeights::Previous30mIndex + 1 == ActivityModelWeights::WeightCount);
}  // namespace

bool ActivityPredictor::predict(uint8_t localHour, uint8_t localWeekday,
                                uint32_t previous30mCount, Prediction& output) {
  if (localHour > 23 || localWeekday > 6) return false;

  const double* weights = ActivityModelWeights::Values;
  const double score =
      weights[ActivityModelWeights::InterceptIndex] +
      weights[ActivityModelWeights::HourStartIndex + localHour] +
      (localWeekday >= 5 ? weights[ActivityModelWeights::WeekendIndex] : 0.0) +
      weights[ActivityModelWeights::Previous30mIndex] *
          std::log1p(static_cast<double>(previous30mCount));
  const double boundedScore = score < MinScore ? MinScore :
                              score > MaxScore ? MaxScore : score;
  const double expectedCount = std::exp(boundedScore);
  const double probability = -std::expm1(-expectedCount);

  output.expectedCount = expectedCount;
  output.activityProbability = probability;
  output.restPwm = static_cast<uint8_t>(
      probability >= ActivityModelWeights::HighAt ? ActivityModelWeights::HighPwm :
      probability >= ActivityModelWeights::StandardAt ? ActivityModelWeights::StandardPwm :
      ActivityModelWeights::LowPwm);
  return true;
}

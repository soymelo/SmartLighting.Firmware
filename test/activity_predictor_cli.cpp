// Programa nativo de teste; não faz parte do firmware gravado no ESP32.
#include <iomanip>
#include <iostream>
#include <string>

#include "../src/Control/ActivityPredictor.h"

int main(int argc, char* argv[]) {
  if (argc != 4) return 2;
  const int hour = std::stoi(argv[1]);
  const int weekday = std::stoi(argv[2]);
  const unsigned long count = std::stoul(argv[3]);
  if (hour < 0 || hour > 255 || weekday < 0 || weekday > 255 || count > UINT32_MAX)
    return 2;

  ActivityPredictor::Prediction prediction;
  if (!ActivityPredictor::predict(static_cast<uint8_t>(hour),
                                  static_cast<uint8_t>(weekday),
                                  static_cast<uint32_t>(count), prediction))
    return 2;
  std::cout << std::setprecision(17) << prediction.expectedCount << ' '
            << prediction.activityProbability << ' '
            << static_cast<int>(prediction.restPwm) << '\n';
  return 0;
}

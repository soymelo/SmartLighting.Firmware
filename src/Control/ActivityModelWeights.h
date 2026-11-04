// Gerado por SmartLighting.ML; não edite os pesos manualmente.
// Fonte: config/firmware_model.json (persistent_activity, 28 dias).
// SHA-256 do JSON: 7d4709501f3e7ef4138cccdd27d13c576e0f8067766cd26ca9d8bb60248396d6
#pragma once

namespace ActivityModelWeights {
constexpr int WeightCount = 27;
constexpr int InterceptIndex = 0;
constexpr int HourStartIndex = 1;
constexpr int WeekendIndex = 25;
constexpr int Previous30mIndex = 26;

constexpr double Values[WeightCount] = {
    -2.0847036334414248,  // intercept
    -0.78531466635992109,  // hour_0
    -0.84263148203581295,  // hour_1
    -0.96110593301317526,  // hour_2
    -0.98180893000184633,  // hour_3
    -1.0019263328333485,  // hour_4
    -0.98173745791740852,  // hour_5
    -0.049886440583258876,  // hour_6
    0.47001653810152838,  // hour_7
    0.37498700260933771,  // hour_8
    0.0090662570602524587,  // hour_9
    0.10307642232412224,  // hour_10
    0.10686626505559162,  // hour_11
    0.20649562566446872,  // hour_12
    0.043402327220757778,  // hour_13
    -0.061450981321693826,  // hour_14
    0.034246230566136078,  // hour_15
    0.3716431923205874,  // hour_16
    0.62715075431982137,  // hour_17
    1.0170550591389387,  // hour_18
    1.0280023456414202,  // hour_19
    0.91338137562887922,  // hour_20
    0.53336800804783435,  // hour_21
    0.11120567445076404,  // hour_22
    -0.28410540286770197,  // hour_23
    -0.089531907417822784,  // weekend
    0.24465873906223831,  // log1p_previous_30m_count
};
}  // namespace ActivityModelWeights

// Gerado por SmartLighting.ML; não edite os pesos manualmente.
// Fonte: config/firmware_model.json (persistent_activity, 28 dias).
// SHA-256 do JSON: 7d4709501f3e7ef4138cccdd27d13c576e0f8067766cd26ca9d8bb60248396d6
// SHA-256 da política: 3d8fe26c17cb4a70fd28c216c9a01d916b554b4a55c12433952075115988d16b
#pragma once

namespace ActivityModelWeights {
constexpr int WeightCount = 27;
constexpr int InterceptIndex = 0;
constexpr int HourStartIndex = 1;
constexpr int WeekendIndex = 25;
constexpr int Previous30mIndex = 26;

constexpr double StandardAt = 0.05;
constexpr double HighAt = 0.25;
constexpr int LowPwm = 10;
constexpr int StandardPwm = 20;
constexpr int HighPwm = 50;

constexpr double Values[WeightCount] = {
    -2.084703633441425,  // intercept
    -0.7853146663599211,  // hour_0
    -0.842631482035813,  // hour_1
    -0.9611059330131753,  // hour_2
    -0.9818089300018463,  // hour_3
    -1.0019263328333485,  // hour_4
    -0.9817374579174085,  // hour_5
    -0.049886440583258876,  // hour_6
    0.4700165381015284,  // hour_7
    0.3749870026093377,  // hour_8
    0.009066257060252459,  // hour_9
    0.10307642232412224,  // hour_10
    0.10686626505559162,  // hour_11
    0.20649562566446872,  // hour_12
    0.04340232722075778,  // hour_13
    -0.061450981321693826,  // hour_14
    0.03424623056613608,  // hour_15
    0.3716431923205874,  // hour_16
    0.6271507543198214,  // hour_17
    1.0170550591389387,  // hour_18
    1.0280023456414202,  // hour_19
    0.9133813756288792,  // hour_20
    0.5333680080478344,  // hour_21
    0.11120567445076404,  // hour_22
    -0.284105402867702,  // hour_23
    -0.08953190741782278,  // weekend
    0.2446587390622383,  // log1p_previous_30m_count
};
}  // namespace ActivityModelWeights

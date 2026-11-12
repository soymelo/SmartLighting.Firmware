// Gerado por SmartLighting.ML; não edite os pesos manualmente.
// Fonte: config/firmware_model.json (persistent_activity, 28 dias).
// SHA-256 do JSON: d2530619ae5cc341e70ba0bb03e089c0b8dad5bff03040d9fa05f9a30e9a679d
// SHA-256 da política: 90eb58a95b9adaf1745fb47ff3ed517b3f343406ab7dc1b648bcf7149bb7cffe
#pragma once

namespace ActivityModelWeights {
constexpr int WeightCount = 27;
constexpr int InterceptIndex = 0;
constexpr int HourStartIndex = 1;
constexpr int WeekendIndex = 25;
constexpr int Previous30mIndex = 26;

constexpr double StandardAt = 0.05;
constexpr double HighAt = 0.3;
constexpr int LowPwm = 10;
constexpr int StandardPwm = 20;
constexpr int HighPwm = 40;

constexpr double Values[WeightCount] = {
    -2.0608062898873145,  // intercept
    -0.7059126875118099,  // hour_0
    -0.7468665602167676,  // hour_1
    -1.0171183950907254,  // hour_2
    -1.0785813220499654,  // hour_3
    -0.8711847128017897,  // hour_4
    -0.43634938485616814,  // hour_5
    -0.08897183424209712,  // hour_6
    0.5344667210324126,  // hour_7
    0.422451602007098,  // hour_8
    0.08458453539224527,  // hour_9
    -0.006552800529633889,  // hour_10
    0.11250961505500322,  // hour_11
    0.08925123983639933,  // hour_12
    0.0026945444636619527,  // hour_13
    -0.11542711516654136,  // hour_14
    0.10270831128688883,  // hour_15
    0.41004683924919966,  // hour_16
    0.6181148496399387,  // hour_17
    0.9254892428019974,  // hour_18
    0.849724030946015,  // hour_19
    0.8385354729760586,  // hour_20
    0.5288305397008326,  // hour_21
    0.06575297123016667,  // hour_22
    -0.5181883358059062,  // hour_23
    -0.12456557181645525,  // weekend
    0.31370436064977375,  // log1p_previous_30m_count
};
}  // namespace ActivityModelWeights

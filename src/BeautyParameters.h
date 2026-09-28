#pragma once
#include <array>
#include <algorithm>
#include <cmath>

struct BeautyParameters {
    enum Index { Smoothing, Whitening, Temperature, Tint, Slimming, Eyes, Lipstick, Blush, Count };
    std::array<float, Count> values{};
    bool enabled = true;
    static BeautyParameters recommended() {
        BeautyParameters p;
        p.values = {.20f, .20f, 0, 0, 0, 0, 0, 0};
        return p;
    }
    bool active() const {
        return enabled && std::any_of(values.begin(), values.end(), [](float v) { return std::abs(v) > .0001f; });
    }
    bool needsFace() const { return values[Slimming] > 0 || values[Eyes] > 0 || values[Lipstick] > 0 || values[Blush] > 0; }
};

#pragma once
#include <array>
// Keep initialization validation and extraction in sync. v1.3.1 whitening
// dereferences LUT images unconditionally, even with whitening strength zero.
inline constexpr std::array<const char *, 9> androidBeautyResources = {
    "models/face_align.mars_model", "models/face_det.mars_model",
    "res/mouth.png", "res/blusher.png", "res/lookup_gray.png", "res/lookup_origin.png",
    "res/lookup_skin.png", "res/lookup_light.png", "res/lookup_custom.png"
};

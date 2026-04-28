#include "demo/wind_tunnel/synthetic_roi.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace osr::demo::wind_tunnel {

namespace {

bool GlyphBit(char glyph, int col, int row) noexcept {
    if (col < 0 || col >= 5 || row < 0 || row >= 7) {
        return false;
    }
    const char* bits = nullptr;
    switch (glyph) {
    case '0': bits = "11110"
                     "10010"
                     "10010"
                     "10010"
                     "10010"
                     "10010"
                     "11110"; break;
    case '6': bits = "01110"
                     "10000"
                     "10000"
                     "11110"
                     "10010"
                     "10010"
                     "01110"; break;
    case '7': bits = "11110"
                     "00010"
                     "00100"
                     "00100"
                     "01000"
                     "01000"
                     "01000"; break;
    case 'M': bits = "10001"
                     "11011"
                     "10101"
                     "10101"
                     "10001"
                     "10001"
                     "10001"; break;
    case 'O': bits = "01110"
                     "10001"
                     "10001"
                     "10001"
                     "10001"
                     "10001"
                     "01110"; break;
    case 'R': bits = "11110"
                     "10001"
                     "10001"
                     "11110"
                     "10100"
                     "10010"
                     "10001"; break;
    case 'S': bits = "01111"
                     "10000"
                     "10000"
                     "01110"
                     "00001"
                     "00001"
                     "11110"; break;
    case 'T': bits = "11111"
                     "00100"
                     "00100"
                     "00100"
                     "00100"
                     "00100"
                     "00100"; break;
    default:
        return false;
    }
    return bits[row * 5 + col] == '1';
}

bool TextGlyphAt(float u, float v, float left, float top, float glyph_height, const char* text) noexcept {
    constexpr int glyph_w = 5;
    constexpr int glyph_h = 7;
    constexpr int gap = 1;
    const float cell = glyph_height / static_cast<float>(glyph_h);
    const float total_width = static_cast<float>(std::max(0, static_cast<int>(std::strlen(text))) * (glyph_w + gap) - gap) * cell;
    if (u < left || v < top || u >= left + total_width || v >= top + glyph_height) {
        return false;
    }
    const int cell_x = static_cast<int>((u - left) / cell);
    const int cell_y = static_cast<int>((v - top) / cell);
    const int advance = glyph_w + gap;
    const int glyph_index = cell_x / advance;
    const int col = cell_x - glyph_index * advance;
    if (col >= glyph_w || glyph_index < 0 || glyph_index >= static_cast<int>(std::strlen(text))) {
        return false;
    }
    return GlyphBit(text[glyph_index], col, cell_y);
}

bool TextPanelAt(float u, float v, float left, float top, float glyph_height, const char* text) noexcept {
    constexpr int glyph_w = 5;
    constexpr int glyph_h = 7;
    constexpr int gap = 1;
    const float cell = glyph_height / static_cast<float>(glyph_h);
    const float total_width = static_cast<float>(std::max(0, static_cast<int>(std::strlen(text))) * (glyph_w + gap) - gap) * cell;
    const float pad = cell * 1.2f;
    return u >= left - pad && u < left + total_width + pad &&
           v >= top - pad && v < top + glyph_height + pad;
}

} // namespace

SyntheticTextCoverage EvaluateSyntheticTextCoverage(float u,
                                                    float v,
                                                    uint64_t frame_id,
                                                    bool enabled) noexcept {
    SyntheticTextCoverage coverage;
    if (!enabled) {
        return coverage;
    }
    const float t = static_cast<float>(frame_id) * 0.03125f;
    const float cube_x = 0.5f + std::sin(t * 0.9f) * 0.18f;

    const float moving_left = cube_x - 0.060f;
    constexpr float moving_top = 0.438f;
    constexpr const char* moving_text = "OSR";
    constexpr float moving_height = 0.050f;
    if (TextPanelAt(u, v, moving_left, moving_top, moving_height, moving_text)) {
        coverage.panel = true;
        coverage.moving = true;
    }
    if (TextGlyphAt(u, v, moving_left, moving_top, moving_height, moving_text)) {
        coverage.glyph = true;
        coverage.moving = true;
    }

    constexpr const char* static_text = "760M";
    constexpr float static_left = 0.088f;
    constexpr float static_top = 0.104f;
    constexpr float static_height = 0.060f;
    if (TextPanelAt(u, v, static_left, static_top, static_height, static_text)) {
        coverage.panel = true;
    }
    if (TextGlyphAt(u, v, static_left, static_top, static_height, static_text)) {
        coverage.glyph = true;
    }
    return coverage;
}

SyntheticMaterialCoverage EvaluateSyntheticMaterialCoverage(float u,
                                                           float v,
                                                           uint64_t frame_id,
                                                           bool enabled) noexcept {
    SyntheticMaterialCoverage coverage;
    if (!enabled) {
        return coverage;
    }
    const float t = static_cast<float>(frame_id) * 0.03125f;
    const float glint_x = 0.80f + std::sin(t * 3.1f) * 0.075f;
    const float glint_y = 0.24f + std::cos(t * 2.3f) * 0.040f;
    coverage.specular = std::hypot(u - glint_x, v - glint_y) < 0.010f;

    const bool pane = u > 0.855f && u < 0.925f && v > 0.48f && v < 0.65f;
    const float stripe = std::abs(std::fmod((u - 0.855f) * 10.0f + (v - 0.48f) * 13.0f + t * 0.16f, 1.0f) - 0.5f);
    coverage.transparent = pane && stripe < 0.040f;
    return coverage;
}

} // namespace osr::demo::wind_tunnel

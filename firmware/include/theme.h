#pragma once
// Visual design tokens (RGB565). Defined from 24-bit hex so they stay readable.

#include <stdint.h>

namespace theme {

constexpr uint16_t rgb(uint32_t hex) {
    return static_cast<uint16_t>(((hex >> 8) & 0xF800) |   // R: top 5 bits
                                 ((hex >> 5) & 0x07E0) |   // G: top 6 bits
                                 ((hex >> 3) & 0x001F));   // B: top 5 bits
}

constexpr uint16_t BG         = rgb(0x121417);  // dark charcoal
constexpr uint16_t CARD       = rgb(0x1E2228);  // panel / card surface
constexpr uint16_t CARD_EDGE  = rgb(0x2A3038);
constexpr uint16_t ACCENT     = rgb(0x00D1FF);  // cyan
constexpr uint16_t ACCENT_ALT = rgb(0x3A7BFF);  // subtle blue
constexpr uint16_t TEXT       = rgb(0xFFFFFF);
constexpr uint16_t TEXT_MUTED = rgb(0x8A929C);
// STATUS_ prefix avoids clashes with framework macros/enums (OK, ERROR, ...)
constexpr uint16_t STATUS_OK    = rgb(0x3DDC84);
constexpr uint16_t STATUS_WARN  = rgb(0xFFB020);
constexpr uint16_t STATUS_ERROR = rgb(0xFF4D5E);

constexpr int16_t PAD    = 8;    // standard spacing
constexpr int16_t RADIUS = 6;    // card corner radius

}  // namespace theme

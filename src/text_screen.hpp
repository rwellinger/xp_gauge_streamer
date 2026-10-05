/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace xp_gauge_streamer
{

constexpr int SCREEN_COLUMNS = 24;
constexpr int SCREEN_ROWS    = 14;

// An add-on that draws its CDU itself publishes the display as one dataref per
// line, colour and font size — "AirbusFBW/MCDU1cont3g" is the green, large text
// of the ToLiss MCDU's third data line. Each covers the full width, blank where
// another layer draws.
struct ScreenLayer
{
    std::string suffix; // appended to the unit's dataref prefix, e.g. "cont3g"
    int         row;    // 0 title, 1-12 label and data lines, 13 scratchpad
    char        color;  // w g b y a m, i for inverse video — or s, the symbol layer
    bool        small;
};

// A character the aircraft's font draws as something else on a colour layer,
// which keeps its colour. The first match wins.
struct Substitution
{
    char        code;
    char        layer_color; // only on layers of this colour; 0 on every one
    const char *glyph;
};

// What a code on the symbol layer draws, and in which colour.
struct Symbol
{
    char        code;
    const char *glyph;
    char        color;
};

// Everything that differs between two aircraft's screen datarefs.
struct ScreenFormat
{
    std::vector<ScreenLayer>  layers; // in the order they are painted over each other
    std::vector<Substitution> substitutions;
    std::vector<Symbol>       symbols;
};

class TextScreen
{
  public:
    explicit TextScreen(const ScreenFormat &format);

    // Paints the layer's non-blank characters over what is there. Stops at the
    // first NUL or after the last column.
    void apply(const ScreenLayer &layer, std::string_view text);

    // {"rows": [{"text": "...", "colors": "...", "sizes": "..."}, ...]} with one
    // character per column in each string. Colours use the layer letters, sizes
    // are L (large) and s (small).
    std::string to_json() const;

  private:
    struct Cell
    {
        std::string glyph = " ";
        char        color = 'w';
        bool        small = false;
    };

    const ScreenFormat                                       *format;
    std::array<std::array<Cell, SCREEN_COLUMNS>, SCREEN_ROWS> cells;
};

} // namespace xp_gauge_streamer

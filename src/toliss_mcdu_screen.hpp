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

constexpr int MCDU_COLUMNS = 24;
constexpr int MCDU_ROWS    = 14;

// The ToLiss MCDU publishes its display as one dataref per line, colour and
// font size — "AirbusFBW/MCDU1cont3g" is the green, large text of the third
// data line. Each holds all 24 columns, blank where another layer draws.
struct McduLayer
{
    std::string suffix; // appended to the unit's prefix, e.g. "cont3g"
    int         row;    // 0 title, 1-12 label and data lines, 13 scratchpad
    char        color;  // w g b y a m — or s, the symbol layer
    bool        small;
};

// Every layer of one unit, the order in which they are painted over each other.
const std::vector<McduLayer> &toliss_mcdu_layers();

class McduScreen
{
  public:
    McduScreen();

    // Paints the layer's non-blank characters over what is there. Stops at the
    // first NUL or after the last column.
    void apply(const McduLayer &layer, std::string_view text);

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

    std::array<std::array<Cell, MCDU_COLUMNS>, MCDU_ROWS> cells;
};

} // namespace xp_gauge_streamer

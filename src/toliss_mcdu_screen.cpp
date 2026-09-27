/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "toliss_mcdu_screen.hpp"

#include <json.hpp>

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

constexpr int  TITLE_ROW      = 0;
constexpr int  SCRATCHPAD_ROW = 13;
constexpr int  LINE_COUNT     = 6;
constexpr char SYMBOL_LAYER   = 's';

constexpr char TEXT_COLORS[]  = {'w', 'g', 'b', 'y', 'a', 'm'};
constexpr char TITLE_COLORS[] = {'w', 'g', 'b', 'y'};

struct Symbol
{
    char        code;
    const char *glyph;
    char        color;
};

// The symbol layer carries codes, not colours. The colour each symbol takes is
// the one the MCDU draws it in, read off the IDENT, INIT, PERF, RADIO NAV and
// DIR TO pages of the ToLiss A319. Glyphs are spelled as UTF-8 bytes: MSVC
// would recode a literal character to the local code page.
constexpr char BOX[]         = "\xe2\x98\x90"; // U+2610
constexpr char LEFT_ARROW[]  = "\xe2\x86\x90"; // U+2190
constexpr char RIGHT_ARROW[] = "\xe2\x86\x92"; // U+2192
constexpr char DEGREE[]      = "\xc2\xb0";     // U+00B0

constexpr Symbol SYMBOLS[] = {
    {'A', "[", 'b'},        {'B', "]", 'b'},         // cyan brackets around an optional entry
    {'E', BOX, 'a'},                                 // amber box of a mandatory entry
    {'0', LEFT_ARROW, 'b'}, {'1', RIGHT_ARROW, 'b'}, // select arrows next to a line key
    {'2', LEFT_ARROW, 'w'}, {'3', RIGHT_ARROW, 'w'}, // page slew arrows in the title
};

// Every text layer spells the degree sign with a backtick.
constexpr char DEGREE_CODE   = '`';
constexpr char UNKNOWN_COLOR = 'w';

bool is_printable_ascii(char character) { return character > ' ' && character < 0x7f; }

std::vector<McduLayer> build_layers()
{
    std::vector<McduLayer> layers;
    const auto add_text_and_symbol_layers = [&layers](const std::string &stem, int row, bool small, auto &&colors)
    {
        for (const char color : colors)
            layers.push_back({stem + color, row, color, small});
        layers.push_back({stem + SYMBOL_LAYER, row, SYMBOL_LAYER, small});
    };

    add_text_and_symbol_layers("title", TITLE_ROW, false, TITLE_COLORS);
    for (const char color : TITLE_COLORS)
        layers.push_back({std::string("stitle") + color, TITLE_ROW, color, true});

    for (int line = 1; line <= LINE_COUNT; ++line)
    {
        const int         label_row = 2 * line - 1;
        const std::string number    = std::to_string(line);

        add_text_and_symbol_layers("label" + number, label_row, true, TEXT_COLORS);
        layers.push_back({"label" + number + "Lw", label_row, 'w', false});
        layers.push_back({"label" + number + "Lg", label_row, 'g', false});

        add_text_and_symbol_layers("cont" + number, label_row + 1, false, TEXT_COLORS);
        add_text_and_symbol_layers("scont" + number, label_row + 1, true, TEXT_COLORS);
    }

    layers.push_back({"spw", SCRATCHPAD_ROW, 'w', false});
    layers.push_back({"spa", SCRATCHPAD_ROW, 'a', false});
    return layers;
}

const Symbol *find_symbol(char code)
{
    for (const Symbol &symbol : SYMBOLS)
    {
        if (symbol.code == code)
            return &symbol;
    }
    return nullptr;
}

} // namespace

const std::vector<McduLayer> &toliss_mcdu_layers()
{
    static const std::vector<McduLayer> layers = build_layers();
    return layers;
}

McduScreen::McduScreen() = default;

void McduScreen::apply(const McduLayer &layer, std::string_view text)
{
    if (layer.row < 0 || layer.row >= MCDU_ROWS)
        return;

    const size_t columns = std::min(text.find('\0'), std::min(text.size(), static_cast<size_t>(MCDU_COLUMNS)));
    for (size_t column = 0; column < columns; ++column)
    {
        const char code = text[column];
        if (!is_printable_ascii(code))
            continue;

        Cell &cell = cells[static_cast<size_t>(layer.row)][column];
        cell.small = layer.small;

        if (layer.color != SYMBOL_LAYER)
        {
            cell.glyph = code == DEGREE_CODE ? DEGREE : std::string(1, code);
            cell.color = layer.color;
            continue;
        }

        const Symbol *symbol = find_symbol(code);
        cell.glyph           = symbol != nullptr ? symbol->glyph : std::string(1, code);
        cell.color           = symbol != nullptr ? symbol->color : UNKNOWN_COLOR;
    }
}

std::string McduScreen::to_json() const
{
    nlohmann::json rows = nlohmann::json::array();
    for (const auto &row : cells)
    {
        std::string text;
        std::string colors;
        std::string sizes;
        for (const Cell &cell : row)
        {
            text += cell.glyph;
            colors += cell.color;
            sizes += cell.small ? 's' : 'L';
        }
        rows.push_back({{"text", text}, {"colors", colors}, {"sizes", sizes}});
    }

    return nlohmann::json{{"rows", rows}}.dump();
}

} // namespace xp_gauge_streamer

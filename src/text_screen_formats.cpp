/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "text_screen_formats.hpp"

#include <cstdio>

namespace xp_gauge_streamer
{

namespace
{

constexpr int  TITLE_ROW      = 0;
constexpr int  SCRATCHPAD_ROW = 13;
constexpr int  LINE_COUNT     = 6;
constexpr char SYMBOL_LAYER   = 's';
constexpr char INVERSE        = 'i';

// Spelled as UTF-8 bytes: MSVC would recode a literal character to the local
// code page.
constexpr char BOX[]         = "\xe2\x98\x90"; // U+2610
constexpr char LEFT_ARROW[]  = "\xe2\x86\x90"; // U+2190
constexpr char RIGHT_ARROW[] = "\xe2\x86\x92"; // U+2192
constexpr char DEGREE[]      = "\xc2\xb0";     // U+00B0

// Both aircraft spell the degree sign with a backtick.
constexpr char DEGREE_CODE = '`';

int label_row(int line) { return 2 * line - 1; }
int data_row(int line) { return 2 * line; }

// ── ToLiss ───────────────────────────────────────────────────────────────────

constexpr char TOLISS_TEXT_COLORS[]  = {'w', 'g', 'b', 'y', 'a', 'm'};
constexpr char TOLISS_TITLE_COLORS[] = {'w', 'g', 'b', 'y'};

std::vector<ScreenLayer> build_toliss_layers()
{
    std::vector<ScreenLayer> layers;
    const auto add_text_and_symbol_layers = [&layers](const std::string &stem, int row, bool small, auto &&colors)
    {
        for (const char color : colors)
            layers.push_back({stem + color, row, color, small});
        layers.push_back({stem + SYMBOL_LAYER, row, SYMBOL_LAYER, small});
    };

    add_text_and_symbol_layers("title", TITLE_ROW, false, TOLISS_TITLE_COLORS);
    for (const char color : TOLISS_TITLE_COLORS)
        layers.push_back({std::string("stitle") + color, TITLE_ROW, color, true});

    for (int line = 1; line <= LINE_COUNT; ++line)
    {
        const std::string number = std::to_string(line);

        add_text_and_symbol_layers("label" + number, label_row(line), true, TOLISS_TEXT_COLORS);
        layers.push_back({"label" + number + "Lw", label_row(line), 'w', false});
        layers.push_back({"label" + number + "Lg", label_row(line), 'g', false});

        add_text_and_symbol_layers("cont" + number, data_row(line), false, TOLISS_TEXT_COLORS);
        add_text_and_symbol_layers("scont" + number, data_row(line), true, TOLISS_TEXT_COLORS);
    }

    layers.push_back({"spw", SCRATCHPAD_ROW, 'w', false});
    layers.push_back({"spa", SCRATCHPAD_ROW, 'a', false});
    return layers;
}

// The symbol layer carries codes, not colours. The colour each symbol takes is
// the one the MCDU draws it in, read off the IDENT, INIT, PERF, RADIO NAV and
// DIR TO pages of the ToLiss A319.
std::vector<Symbol> toliss_symbols()
{
    return {
        {'A', "[", 'b'},        {'B', "]", 'b'},         // cyan brackets around an optional entry
        {'E', BOX, 'a'},                                 // amber box of a mandatory entry
        {'0', LEFT_ARROW, 'b'}, {'1', RIGHT_ARROW, 'b'}, // select arrows next to a line key
        {'2', LEFT_ARROW, 'w'}, {'3', RIGHT_ARROW, 'w'}, // page slew arrows in the title
    };
}

// ── Zibo ─────────────────────────────────────────────────────────────────────

// Layer suffix and colour as the 737's panel draws them: each dataref is a
// gen_text instrument in b738.acf whose font texture (TXT.fmc.big.green,
// .small_i.white, …) gives colour and size. Line04_SI exists for line 4 only.
std::string zibo_suffix(int line, const char *layer)
{
    char suffix[16];
    std::snprintf(suffix, sizeof(suffix), "Line%02d_%s", line, layer);
    return suffix;
}

void add_zibo_data_layers(std::vector<ScreenLayer> &layers, int line, int row)
{
    layers.push_back({zibo_suffix(line, "L"), row, 'w', false});
    layers.push_back({zibo_suffix(line, "S"), row, 'w', true});
    layers.push_back({zibo_suffix(line, "I"), row, INVERSE, false});
    layers.push_back({zibo_suffix(line, "M"), row, 'm', false});
    layers.push_back({zibo_suffix(line, "G"), row, 'g', false});
}

std::vector<ScreenLayer> build_zibo_layers()
{
    std::vector<ScreenLayer> layers;

    add_zibo_data_layers(layers, 0, TITLE_ROW);
    layers.push_back({zibo_suffix(0, "C"), TITLE_ROW, 'b', false});

    for (int line = 1; line <= LINE_COUNT; ++line)
    {
        layers.push_back({zibo_suffix(line, "X"), label_row(line), 'w', true});
        layers.push_back({zibo_suffix(line, "LX"), label_row(line), 'w', false});
        layers.push_back({zibo_suffix(line, "GX"), label_row(line), 'g', false});
        add_zibo_data_layers(layers, line, data_row(line));
    }
    layers.push_back({zibo_suffix(4, "SI"), data_row(4), INVERSE, true});

    layers.push_back({"Line_entry", SCRATCHPAD_ROW, 'w', false});
    layers.push_back({"Line_entry_I", SCRATCHPAD_ROW, INVERSE, false});
    return layers;
}

} // namespace

const ScreenFormat &toliss_mcdu_format()
{
    static const ScreenFormat format{build_toliss_layers(), {{DEGREE_CODE, 0, DEGREE}}, toliss_symbols()};
    return format;
}

// The 737's fonts reuse the asterisk. In white, green and magenta it is the box
// of a mandatory entry — DEST shows "****" on an empty RTE page. In the inverse
// font it is a lit blank, so a highlight reads "DES*NOW" without a gap: a space
// would leave the bar unlit. ToLiss keeps the asterisk for its own prompts.
const ScreenFormat &zibo_fmc_format()
{
    static const ScreenFormat format{
        build_zibo_layers(), {{DEGREE_CODE, 0, DEGREE}, {'*', INVERSE, " "}, {'*', 0, BOX}}, {}};
    return format;
}

const ScreenFormat *screen_format_for(std::string_view device_type)
{
    if (device_type == "toliss_mcdu")
        return &toliss_mcdu_format();
    if (device_type == "zibo_fmc")
        return &zibo_fmc_format();
    return nullptr;
}

} // namespace xp_gauge_streamer

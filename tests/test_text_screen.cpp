/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "text_screen_formats.hpp"

#include <catch_amalgamated.hpp>
#include <json.hpp>

#include <algorithm>
#include <set>

using namespace xp_gauge_streamer;

namespace
{

const ScreenLayer &find_layer(const ScreenFormat &format, const std::string &suffix)
{
    const auto match = std::find_if(format.layers.begin(), format.layers.end(),
                                    [&suffix](const ScreenLayer &candidate) { return candidate.suffix == suffix; });
    REQUIRE(match != format.layers.end());
    return *match;
}

const ScreenLayer &layer(const std::string &suffix) { return find_layer(toliss_mcdu_format(), suffix); }

const ScreenLayer &zibo_layer(const std::string &suffix) { return find_layer(zibo_fmc_format(), suffix); }

void check_layers_fit_the_grid(const std::vector<ScreenLayer> &layers)
{
    std::set<std::string> suffixes;
    for (const ScreenLayer &entry : layers)
    {
        suffixes.insert(entry.suffix);
        CHECK(entry.row >= 0);
        CHECK(entry.row < SCREEN_ROWS);
    }
    CHECK(suffixes.size() == layers.size());
}

nlohmann::json row(const TextScreen &screen, size_t index)
{
    return nlohmann::json::parse(screen.to_json())["rows"][index];
}

} // namespace

TEST_CASE("the layers match the datarefs of one ToLiss unit", "[text_screen]")
{
    // Counted in the A319: 149 screen datarefs per unit.
    const std::vector<ScreenLayer> &layers = toliss_mcdu_format().layers;
    CHECK(layers.size() == 149);
    check_layers_fit_the_grid(layers);
}

TEST_CASE("lines land on alternating label and data rows", "[text_screen]")
{
    CHECK(layer("titlew").row == 0);
    CHECK(layer("label1w").row == 1);
    CHECK(layer("cont1g").row == 2);
    CHECK(layer("scont1g").row == 2);
    CHECK(layer("label6w").row == 11);
    CHECK(layer("cont6b").row == 12);
    CHECK(layer("spa").row == 13);

    CHECK(layer("label2w").small);
    CHECK_FALSE(layer("label2Lg").small);
    CHECK_FALSE(layer("cont2w").small);
    CHECK(layer("scont2w").small);
    CHECK(layer("stitlew").small);
}

TEST_CASE("an empty screen is 14 blank rows of 24 columns", "[text_screen]")
{
    const TextScreen     screen(toliss_mcdu_format());
    const nlohmann::json rows = nlohmann::json::parse(screen.to_json())["rows"];

    REQUIRE(rows.size() == SCREEN_ROWS);
    for (const nlohmann::json &entry : rows)
    {
        CHECK(entry["text"] == std::string(SCREEN_COLUMNS, ' '));
        CHECK(entry["colors"].get<std::string>().size() == SCREEN_COLUMNS);
        CHECK(entry["sizes"].get<std::string>().size() == SCREEN_COLUMNS);
    }
}

TEST_CASE("colour layers overlay each other column by column", "[text_screen]")
{
    // IDENT page, line 2 of the A319: database id in green, validity in cyan.
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont2g"), std::string_view("              ABV2609001\0", 25));
    screen.apply(layer("cont2b"), std::string_view("03SEP-30SEP\0", 12));

    const nlohmann::json line = row(screen, 4);
    CHECK(line["text"] == "03SEP-30SEP   ABV2609001");
    CHECK(line["colors"] == "bbbbbbbbbbbwwwgggggggggg");
    CHECK(line["sizes"] == std::string(SCREEN_COLUMNS, 'L'));
}

TEST_CASE("small text keeps its size on a data row", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont6w"), "<TO DATA");
    screen.apply(layer("scont6g"), "          +0.0");

    const std::string sizes = row(screen, 12)["sizes"];
    CHECK(sizes.substr(0, 8) == "LLLLLLLL");
    CHECK(sizes.substr(10, 4) == "ssss");
}

TEST_CASE("the symbol layer draws glyphs in their own colours", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont1s"), "EE A B 0 1");

    const nlohmann::json line = row(screen, 2);
    CHECK(line["text"] == "\xe2\x98\x90\xe2\x98\x90 [ ] \xe2\x86\x90 \xe2\x86\x92              ");
    CHECK(line["colors"].get<std::string>().substr(0, 10) == "aawbwbwbwb");
}

TEST_CASE("the title's slew arrows are white", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("titles"), "                      23");

    CHECK(row(screen, 0)["colors"].get<std::string>().substr(22) == "ww");
}

TEST_CASE("a backtick is the degree sign", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont6w"), "-----/---`");

    CHECK(row(screen, 12)["text"].get<std::string>().substr(0, 11) == "-----/---\xc2\xb0");
}

TEST_CASE("text past the NUL, the last column or ASCII is ignored", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont3w"), std::string_view("AB\0CD", 5));
    screen.apply(layer("cont4w"), std::string(30, 'X'));
    screen.apply(layer("cont5w"), "A\x01\xff");

    CHECK(row(screen, 6)["text"] == "AB                      ");
    CHECK(row(screen, 8)["text"] == std::string(SCREEN_COLUMNS, 'X'));
    CHECK(row(screen, 10)["text"] == "A                       ");
}

TEST_CASE("unknown device types have no screen format", "[text_screen]")
{
    CHECK(screen_format_for("toliss_mcdu") == &toliss_mcdu_format());
    CHECK(screen_format_for("zibo_fmc") == &zibo_fmc_format());
    CHECK(screen_format_for("cdu739") == nullptr);
}

TEST_CASE("the layers match the datarefs of one Zibo unit", "[text_screen]")
{
    // Counted in the Zibo 737-800: 57 laminar/B738/fmc1/Line* datarefs.
    const std::vector<ScreenLayer> &layers = zibo_fmc_format().layers;
    CHECK(layers.size() == 57);
    check_layers_fit_the_grid(layers);
}

TEST_CASE("Zibo lines land on alternating label and data rows", "[text_screen]")
{
    CHECK(zibo_layer("Line00_L").row == 0);
    CHECK(zibo_layer("Line00_C").row == 0);
    CHECK(zibo_layer("Line01_X").row == 1);
    CHECK(zibo_layer("Line01_GX").row == 1);
    CHECK(zibo_layer("Line01_L").row == 2);
    CHECK(zibo_layer("Line04_SI").row == 8);
    CHECK(zibo_layer("Line06_X").row == 11);
    CHECK(zibo_layer("Line06_G").row == 12);
    CHECK(zibo_layer("Line_entry").row == 13);
    CHECK(zibo_layer("Line_entry_I").row == 13);

    CHECK(zibo_layer("Line00_S").small);
    CHECK(zibo_layer("Line03_X").small);
    CHECK_FALSE(zibo_layer("Line03_LX").small);
    CHECK(zibo_layer("Line04_SI").small);
}

TEST_CASE("Zibo layers carry the colour of their font", "[text_screen]")
{
    CHECK(zibo_layer("Line02_L").color == 'w');
    CHECK(zibo_layer("Line02_G").color == 'g');
    CHECK(zibo_layer("Line02_GX").color == 'g');
    CHECK(zibo_layer("Line02_M").color == 'm');
    CHECK(zibo_layer("Line00_C").color == 'b');
    CHECK(zibo_layer("Line02_I").color == 'i');
    CHECK(zibo_layer("Line_entry_I").color == 'i');
}

TEST_CASE("a Zibo page combines its label and data layers", "[text_screen]")
{
    // N1 LIMIT, line 1 as the 737 shows it: label above, small OAT inside.
    TextScreen screen(zibo_fmc_format());
    screen.apply(zibo_layer("Line01_X"), std::string_view(" SEL/OAT          26K N1\0", 25));
    screen.apply(zibo_layer("Line01_L"), std::string_view("----/         99.2/ 99.2\0", 25));
    screen.apply(zibo_layer("Line01_S"), std::string_view("      +12`C\0", 12));

    CHECK(row(screen, 1)["text"] == " SEL/OAT          26K N1");
    CHECK(row(screen, 1)["sizes"] == "LsssssssLLLLLLLLLLsssLss");

    const nlohmann::json line = row(screen, 2);
    CHECK(line["text"] == "----/ +12\xc2\xb0" "C   99.2/ 99.2");
    CHECK(line["sizes"].get<std::string>().substr(0, 11) == "LLLLLLsssss");
}

TEST_CASE("a Zibo asterisk is a box, but a lit blank when inverse", "[text_screen]")
{
    TextScreen screen(zibo_fmc_format());
    screen.apply(zibo_layer("Line01_L"), "----                ****");
    screen.apply(zibo_layer("Line06_I"), "                DES*NOW");

    CHECK(row(screen, 2)["text"] == "----                \xe2\x98\x90\xe2\x98\x90\xe2\x98\x90\xe2\x98\x90");

    const nlohmann::json line = row(screen, 12);
    CHECK(line["text"] == "                DES NOW ");
    CHECK(line["colors"] == "wwwwwwwwwwwwwwwwiiiiiiiw");
}

TEST_CASE("an asterisk stays an asterisk on the ToLiss MCDU", "[text_screen]")
{
    TextScreen screen(toliss_mcdu_format());
    screen.apply(layer("cont1b"), "<SELECT*");

    CHECK(row(screen, 2)["text"].get<std::string>().substr(0, 8) == "<SELECT*");
}

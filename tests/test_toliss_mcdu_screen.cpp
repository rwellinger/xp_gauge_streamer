/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "toliss_mcdu_screen.hpp"

#include <catch_amalgamated.hpp>
#include <json.hpp>

#include <algorithm>
#include <set>

using namespace xp_gauge_streamer;

namespace
{

const McduLayer &layer(const std::string &suffix)
{
    const std::vector<McduLayer> &layers = toliss_mcdu_layers();
    const auto match = std::find_if(layers.begin(), layers.end(),
                                    [&suffix](const McduLayer &candidate) { return candidate.suffix == suffix; });
    REQUIRE(match != layers.end());
    return *match;
}

nlohmann::json row(const McduScreen &screen, size_t index)
{
    return nlohmann::json::parse(screen.to_json())["rows"][index];
}

} // namespace

TEST_CASE("the layers match the datarefs of one ToLiss unit", "[toliss_mcdu_screen]")
{
    // Counted in the A319: 149 screen datarefs per unit.
    const std::vector<McduLayer> &layers = toliss_mcdu_layers();
    CHECK(layers.size() == 149);

    std::set<std::string> suffixes;
    for (const McduLayer &entry : layers)
    {
        suffixes.insert(entry.suffix);
        CHECK(entry.row >= 0);
        CHECK(entry.row < MCDU_ROWS);
    }
    CHECK(suffixes.size() == layers.size());
}

TEST_CASE("lines land on alternating label and data rows", "[toliss_mcdu_screen]")
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

TEST_CASE("an empty screen is 14 blank rows of 24 columns", "[toliss_mcdu_screen]")
{
    const McduScreen     screen;
    const nlohmann::json rows = nlohmann::json::parse(screen.to_json())["rows"];

    REQUIRE(rows.size() == MCDU_ROWS);
    for (const nlohmann::json &entry : rows)
    {
        CHECK(entry["text"] == std::string(MCDU_COLUMNS, ' '));
        CHECK(entry["colors"].get<std::string>().size() == MCDU_COLUMNS);
        CHECK(entry["sizes"].get<std::string>().size() == MCDU_COLUMNS);
    }
}

TEST_CASE("colour layers overlay each other column by column", "[toliss_mcdu_screen]")
{
    // IDENT page, line 2 of the A319: database id in green, validity in cyan.
    McduScreen screen;
    screen.apply(layer("cont2g"), std::string_view("              ABV2609001\0", 25));
    screen.apply(layer("cont2b"), std::string_view("03SEP-30SEP\0", 12));

    const nlohmann::json line = row(screen, 4);
    CHECK(line["text"] == "03SEP-30SEP   ABV2609001");
    CHECK(line["colors"] == "bbbbbbbbbbbwwwgggggggggg");
    CHECK(line["sizes"] == std::string(MCDU_COLUMNS, 'L'));
}

TEST_CASE("small text keeps its size on a data row", "[toliss_mcdu_screen]")
{
    McduScreen screen;
    screen.apply(layer("cont6w"), "<TO DATA");
    screen.apply(layer("scont6g"), "          +0.0");

    const std::string sizes = row(screen, 12)["sizes"];
    CHECK(sizes.substr(0, 8) == "LLLLLLLL");
    CHECK(sizes.substr(10, 4) == "ssss");
}

TEST_CASE("the symbol layer draws glyphs in their own colours", "[toliss_mcdu_screen]")
{
    McduScreen screen;
    screen.apply(layer("cont1s"), "EE A B 0 1");

    const nlohmann::json line = row(screen, 2);
    CHECK(line["text"] == "\xe2\x98\x90\xe2\x98\x90 [ ] \xe2\x86\x90 \xe2\x86\x92              ");
    CHECK(line["colors"].get<std::string>().substr(0, 10) == "aawbwbwbwb");
}

TEST_CASE("the title's slew arrows are white", "[toliss_mcdu_screen]")
{
    McduScreen screen;
    screen.apply(layer("titles"), "                      23");

    CHECK(row(screen, 0)["colors"].get<std::string>().substr(22) == "ww");
}

TEST_CASE("a backtick is the degree sign", "[toliss_mcdu_screen]")
{
    McduScreen screen;
    screen.apply(layer("cont6w"), "-----/---`");

    CHECK(row(screen, 12)["text"].get<std::string>().substr(0, 11) == "-----/---\xc2\xb0");
}

TEST_CASE("text past the NUL, the last column or ASCII is ignored", "[toliss_mcdu_screen]")
{
    McduScreen screen;
    screen.apply(layer("cont3w"), std::string_view("AB\0CD", 5));
    screen.apply(layer("cont4w"), std::string(30, 'X'));
    screen.apply(layer("cont5w"), "A\x01\xff");

    CHECK(row(screen, 6)["text"] == "AB                      ");
    CHECK(row(screen, 8)["text"] == std::string(MCDU_COLUMNS, 'X'));
    CHECK(row(screen, 10)["text"] == "A                       ");
}

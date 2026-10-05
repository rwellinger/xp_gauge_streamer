/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "text_screen.hpp"

#include <json.hpp>

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

constexpr char SYMBOL_LAYER  = 's';
constexpr char UNKNOWN_COLOR = 'w';

bool is_printable_ascii(char character) { return character > ' ' && character < 0x7f; }

std::string substitute(const std::vector<Substitution> &substitutions, char code, char layer_color)
{
    const auto match = std::find_if(substitutions.begin(), substitutions.end(),
                                    [code, layer_color](const Substitution &substitution)
                                    {
                                        return substitution.code == code && (substitution.layer_color == 0 ||
                                                                             substitution.layer_color == layer_color);
                                    });
    return match != substitutions.end() ? match->glyph : std::string(1, code);
}

const Symbol *find_symbol(const std::vector<Symbol> &symbols, char code)
{
    const auto match =
        std::find_if(symbols.begin(), symbols.end(), [code](const Symbol &symbol) { return symbol.code == code; });
    return match != symbols.end() ? &*match : nullptr;
}

} // namespace

TextScreen::TextScreen(const ScreenFormat &format) : format(&format) {}

void TextScreen::apply(const ScreenLayer &layer, std::string_view text)
{
    if (layer.row < 0 || layer.row >= SCREEN_ROWS)
        return;

    const size_t columns = std::min(text.find('\0'), std::min(text.size(), static_cast<size_t>(SCREEN_COLUMNS)));
    for (size_t column = 0; column < columns; ++column)
    {
        const char code = text[column];
        if (!is_printable_ascii(code))
            continue;

        Cell &cell = cells[static_cast<size_t>(layer.row)][column];
        cell.small = layer.small;

        if (layer.color != SYMBOL_LAYER)
        {
            cell.glyph = substitute(format->substitutions, code, layer.color);
            cell.color = layer.color;
            continue;
        }

        const Symbol *symbol = find_symbol(format->symbols, code);
        cell.glyph           = symbol != nullptr ? symbol->glyph : std::string(1, code);
        cell.color           = symbol != nullptr ? symbol->color : UNKNOWN_COLOR;
    }
}

std::string TextScreen::to_json() const
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

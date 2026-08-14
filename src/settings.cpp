#include "settings.hpp"

#include <fstream>
#include <sstream>

namespace xp_gauge_streamer
{

namespace
{

Settings    settings_in_use;
std::string settings_file;

constexpr int LOWEST_PORT  = 1;
constexpr int HIGHEST_PORT = 65535;

std::string trimmed(const std::string &text)
{
    const std::string whitespace = " \t\r\n";
    const size_t      first      = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return {};

    const size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

bool parse_port(const std::string &value, int &into)
{
    try
    {
        size_t    consumed = 0;
        const int port     = std::stoi(value, &consumed);
        if (consumed != value.size() || port < LOWEST_PORT || port > HIGHEST_PORT)
            return false;

        into = port;
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

void apply_entry(const std::string &key, const std::string &value, Settings &into)
{
    if (value.empty())
        return;

    if (key == "bind_address")
        into.bind_address = value;
    else if (key == "port")
        parse_port(value, into.port);
}

} // namespace

Settings parse_settings(const std::string &text)
{
    Settings           settings;
    std::istringstream lines(text);
    std::string        line;

    while (std::getline(lines, line))
    {
        const std::string entry = trimmed(line);
        if (entry.empty() || entry.front() == '#')
            continue;

        const size_t separator = entry.find('=');
        if (separator == std::string::npos)
            continue;

        apply_entry(trimmed(entry.substr(0, separator)), trimmed(entry.substr(separator + 1)), settings);
    }

    return settings;
}

std::string serialize_settings(const Settings &settings)
{
    std::ostringstream text;
    text << "# xp_gauge_streamer settings\n"
         << "# The HTTP server has no authentication — anyone on this network\n"
         << "# can watch the stream. Bind to 127.0.0.1 to restrict it to this machine.\n"
         << "bind_address = " << settings.bind_address << "\n"
         << "port = " << settings.port << "\n";
    return text.str();
}

Settings load_settings(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return {};

    std::ostringstream text;
    text << file.rdbuf();
    return parse_settings(text.str());
}

void open_settings(const std::string &path)
{
    settings_file   = path;
    settings_in_use = load_settings(path);
    persist_settings();
}

Settings &current_settings() { return settings_in_use; }

bool persist_settings() { return !settings_file.empty() && save_settings(settings_file, settings_in_use); }

bool save_settings(const std::string &path, const Settings &settings)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
        return false;

    file << serialize_settings(settings);
    return file.good();
}

} // namespace xp_gauge_streamer

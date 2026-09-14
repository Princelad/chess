#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace chess::client {

namespace {

std::string trim(std::string s)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    const auto first = std::find_if_not(s.begin(), s.end(), isSpace);
    s.erase(s.begin(), first);
    if (!s.empty()) {
        const auto last = std::find_if_not(s.rbegin(), s.rend(), isSpace).base();
        s.erase(last, s.end());
    }
    return s;
}

// Strip everything from the first unquoted `#` or `;` to the end of line.
std::string stripComment(std::string s)
{
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '#' || s[i] == ';')
            return s.substr(0, i);
    }
    return s;
}

std::string sectionKey(const std::string& section, const std::string& key)
{
    return section + "." + key;
}

std::string sectionOf(const std::string& key)
{
    const std::size_t dot = key.find('.');
    if (dot == std::string::npos) return {};
    return key.substr(0, dot);
}

std::string bareKey(const std::string& key)
{
    const std::size_t dot = key.find('.');
    if (dot == std::string::npos) return key;
    return key.substr(dot + 1);
}

} // namespace

std::string Config::defaultPath()
{
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return std::string(home) + "/.config/chess/config.ini";
}

bool Config::load()
{
    std::ifstream in(path_);
    if (!in.is_open()) return false;

    entries_.clear();
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = stripComment(line);
        line = trim(line);
        if (line.empty()) continue;

        if (line[0] == '[') {
            const std::size_t close = line.find(']');
            section = (close == std::string::npos) ? "" : trim(line.substr(1, close - 1));
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue; // not key=value

        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty()) continue;
        entries_[sectionKey(section, key)] = value;
    }
    return true;
}

bool Config::save() const
{
    std::filesystem::path file(path_);
    if (file.empty()) return false;
    const std::filesystem::path dir = file.parent_path();
    std::error_code ec;
    if (!dir.empty() && !std::filesystem::exists(dir, ec))
        std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    std::ofstream out(path_, std::ios::trunc);
    if (!out.is_open()) return false;

    std::string currentSection;
    for (const auto& [key, value] : entries_) {
        const std::string section = sectionOf(key);
        if (section.empty()) continue;
        if (section != currentSection) {
            out << "\n[" << section << "]\n";
            currentSection = section;
        }
        out << bareKey(key) << " = " << value << "\n";
    }
    return static_cast<bool>(out);
}

std::string Config::get(const std::string& key, const std::string& def) const
{
    const auto it = entries_.find(key);
    return it == entries_.end() ? def : it->second;
}

bool Config::getBool(const std::string& key, bool def) const
{
    const auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    const std::string& v = it->second;
    if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
    if (v == "false" || v == "0" || v == "no" || v == "off") return false;
    return def;
}

int Config::getInt(const std::string& key, int def) const
{
    const auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return def;
    }
}

double Config::getFloat(const std::string& key, double def) const
{
    const auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    try {
        return std::stod(it->second);
    } catch (...) {
        return def;
    }
}

void Config::set(const std::string& key, const std::string& value)
{
    if (!key.empty() && key.find('.') != std::string::npos)
        entries_[key] = value;
}

void Config::remove(const std::string& key)
{
    entries_.erase(key);
}

} // namespace chess::client
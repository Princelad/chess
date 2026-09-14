#pragma once

#include <map>
#include <string>

namespace chess::client {

// Minimal INI-style config persisted to a single file. Keys are stored
// flattened as "section.key". `load()` parses `[section]` headers and
// `key = value` lines (ignoring `#`/`;` comments and malformed lines);
// `save()` groups entries by section and writes them sorted.
class Config {
public:
    explicit Config(std::string path) : path_(std::move(path)) {}

    // $HOME/.config/chess/config.ini (empty string if $HOME is unset).
    static std::string defaultPath();

    bool load();
    bool save() const;

    std::string get(const std::string& key, const std::string& def = {}) const;
    bool getBool(const std::string& key, bool def) const;
    int getInt(const std::string& key, int def) const;

    void set(const std::string& key, const std::string& value);
    void setBool(const std::string& key, bool value) { set(key, value ? "true" : "false"); }
    void setInt(const std::string& key, int value) { set(key, std::to_string(value)); }
    void remove(const std::string& key);

    bool has(const std::string& key) const { return entries_.find(key) != entries_.end(); }

    const std::map<std::string, std::string>& entries() const { return entries_; }

private:
    std::string path_;
    std::map<std::string, std::string> entries_; // "section.key" -> value
};

} // namespace chess::client
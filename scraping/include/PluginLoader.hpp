#pragma once
#include "Plugin.hpp"
#include <string>
#include <memory>
#include <vector>

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    // Load a plugin shared object from `path` and initialize it with `cfg`.
    // Returns a shared_ptr to the plugin instance (custom deleter will call destroy and dlclose).
    std::shared_ptr<IPlugin> load(const std::string& path, const PluginConfig& cfg);
    // Load plugins described in a simple YAML-like config file.
    // Returns vector of plugin instances (successful loads).
    std::vector<std::shared_ptr<IPlugin>> load_from_config(const std::string& cfg_path);

    // list of loaded plugin paths
    std::vector<std::string> loaded_paths() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

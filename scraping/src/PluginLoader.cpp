#include "PluginLoader.hpp"
#include <dlfcn.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <toml++/toml.h>
#include <filesystem>

struct PluginLoader::Impl {
    struct Entry { void* handle = nullptr; std::string path; };
    std::vector<Entry> entries;
};

PluginLoader::PluginLoader() : impl_(new Impl()) {}

PluginLoader::~PluginLoader()
{
    // ensure all shared objects are closed
    for (auto &e : impl_->entries) {
        if (e.handle) dlclose(e.handle);
    }
}

std::shared_ptr<IPlugin> PluginLoader::load(const std::string& path, const PluginConfig& cfg)
{
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return nullptr;
    }
    // lookup factory symbols
    dlerror();
    auto create = (create_t)dlsym(handle, "create_plugin");
    const char* dlsym_err = dlerror();
    if (dlsym_err || !create) {
        std::cerr << "missing create_plugin in " << path << " : " << (dlsym_err?dlsym_err:"") << std::endl;
        dlclose(handle);
        return nullptr;
    }
    auto destroy = (destroy_t)dlsym(handle, "destroy_plugin");
    dlsym_err = dlerror();
    if (dlsym_err || !destroy) {
        std::cerr << "missing destroy_plugin in " << path << " : " << (dlsym_err?dlsym_err:"") << std::endl;
        dlclose(handle);
        return nullptr;
    }

    IPlugin* raw = nullptr;
    try {
        raw = create();
    } catch (const std::exception& ex) {
        std::cerr << "plugin create threw: " << ex.what() << std::endl;
        dlclose(handle);
        return nullptr;
    }

    if (!raw) {
        std::cerr << "create_plugin returned null for " << path << std::endl;
        dlclose(handle);
        return nullptr;
    }

    if (!raw->init(cfg)) {
        std::cerr << "plugin init failed for " << path << std::endl;
        destroy(raw);
        dlclose(handle);
        return nullptr;
    }

    // custom deleter calls shutdown, destroy and dlclose
    auto deleter = [handle, destroy](IPlugin* p){
        try { p->shutdown(); } catch(...) {}
        try { destroy(p); } catch(...) {}
        if (handle) dlclose(handle);
    };

    std::shared_ptr<IPlugin> sp(raw, deleter);
    impl_->entries.push_back({handle, path});
    return sp;
}

std::vector<std::string> PluginLoader::loaded_paths() const
{
    std::vector<std::string> out;
    for (auto &e : impl_->entries) out.push_back(e.path);
    return out;
}

// Very small YAML-like parser for a limited plugin config format.
// Supported subset:
// plugins:
//  - path: /abs/path/plugin.so
//    config:
//      key: value
//      k2: v2
// Returns vector of (path, PluginConfig)
static std::vector<std::pair<std::string, PluginConfig>> parse_toml_plugins_cfg(const std::string& cfg_path)
{
    std::vector<std::pair<std::string, PluginConfig>> out;
    try {
        const auto tbl = toml::parse_file(cfg_path);
        if (!tbl.contains("plugin")) return out;
        auto node = tbl["plugin"];
        if (!node.is_array()) return out;
        for (const auto &elem : *node.as_array()) {
            if (!elem.is_table()) continue;
            const auto &t = *elem.as_table();
            if (!t.contains("path")) continue;
            std::string path;
            if (t["path"].is_string()) path = t["path"].value<std::string>().value_or("");
            if (path.empty()) continue;
            PluginConfig cfg;
            if (t.contains("config") && t["config"].is_table()) {
                for (const auto &kv : *t["config"].as_table()) {
                    std::string key = std::string(kv.first.str());
                    const auto &valnode = kv.second;
                    std::string val;
                    if (valnode.is_string()) val = valnode.value<std::string>().value_or("");
                    else if (valnode.is_integer()) val = std::to_string(valnode.value<int64_t>().value_or(0));
                    else if (valnode.is_floating_point()) val = std::to_string(valnode.value<double>().value_or(0.0));
                    else if (valnode.is_boolean()) val = valnode.value<bool>().value_or(false) ? "true" : "false";
                    else val = "";
                    cfg.kv[key] = val;
                }
            }
            out.emplace_back(path, cfg);
        }
    } catch (const toml::parse_error &err) {
        std::cerr << "TOML parse error: " << err.description() << std::endl;
    } catch (const std::exception &ex) {
        std::cerr << "TOML unknown error: " << ex.what() << std::endl;
    }
    return out;
}

std::vector<std::shared_ptr<IPlugin>> PluginLoader::load_from_config(const std::string& cfg_path)
{
    std::vector<std::shared_ptr<IPlugin>> out;
    auto entries = parse_toml_plugins_cfg(cfg_path);
    std::filesystem::path cfg_dir = std::filesystem::path(cfg_path).parent_path();
    int idx = 0;
    for (auto &e : entries) {
        ++idx;
        std::string raw_path = e.first;
        std::filesystem::path p(raw_path);
        if (p.is_relative()) p = cfg_dir / p;
        std::string resolved = p.lexically_normal().string();
        if (!std::filesystem::exists(p)) {
            std::cerr << "[PluginLoader] Plugin #" << idx << " path not found: " << resolved << " (skipping)\n";
            continue;
        }
        std::cout << "[PluginLoader] Loading plugin #" << idx << " : " << resolved << "\n";
        auto sp = load(resolved, e.second);
        if (!sp) {
            std::cerr << "[PluginLoader] Failed to initialize plugin: " << resolved << "\n";
        } else {
            out.push_back(sp);
            std::cout << "[PluginLoader] Plugin loaded: " << resolved << " (name=" << sp->name() << ", version=" << sp->version() << ")\n";
        }
    }
    return out;
}

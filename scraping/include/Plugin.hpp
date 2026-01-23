#pragma once
#include <map>
#include <string>
// #include <memory>
#include <functional>

struct PluginConfig {
    std::map<std::string,std::string> kv;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    // initialize plugin with configuration
    virtual bool init(const PluginConfig& cfg) = 0;
    // graceful shutdown
    virtual void shutdown() = 0;
    // human-readable name and version
    virtual const char* name() const = 0;
    virtual const char* version() const = 0;
};

// Example small plugin interfaces (extend as needed)
class StoragePlugin : public IPlugin {
public:
    // called to store a single JSON record (stringified)
    virtual void store(const std::string& json_record) = 0;
};

class FetcherPlugin : public IPlugin {
public:
    // simple async fetch API; plugin is responsible for invoking cb
    virtual void fetch(const std::string& url, std::function<void(const std::string&)> cb) = 0;
};

// Factory function signatures plugins must export
extern "C" {
    using create_t = IPlugin* (*)();
    using destroy_t = void (*)(IPlugin*);
}

#include "Plugin.hpp"
#include <fstream>
#include <mutex>
#include <iostream>

class JsonlStoragePlugin : public StoragePlugin {
public:
    JsonlStoragePlugin() : file_("plugin_out.jsonl") {}
    virtual ~JsonlStoragePlugin() {}
    bool init(const PluginConfig& cfg) override {
        auto it = cfg.kv.find("file");
        if (it != cfg.kv.end() && !it->second.empty()) file_ = it->second;
        ofs_.open(file_, std::ios::app);
        if (!ofs_.is_open()) {
            std::cerr << "JsonlStoragePlugin: failed to open file: " << file_ << std::endl;
            return false;
        }
        return true;
    }
    void shutdown() override {
        std::lock_guard<std::mutex> g(mtx_);
        if (ofs_.is_open()) ofs_.close();
    }
    const char* name() const override { return "jsonl_storage_plugin"; }
    const char* version() const override { return "0.1"; }

    std::expected<void,std::string>store(const std::string& json_record) override {
        std::lock_guard<std::mutex> g(mtx_);
        if (!ofs_.is_open()) return std::unexpected("Output file is not open");
        ofs_ << json_record << "\n";
        ofs_.flush();
        return {};
    }

private:
    std::string file_;
    std::ofstream ofs_;
    std::mutex mtx_;
};

extern "C" IPlugin* create_plugin() { return new JsonlStoragePlugin(); }
extern "C" void destroy_plugin(IPlugin* p) { delete p; }

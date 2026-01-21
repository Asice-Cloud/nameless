#pragma once
#include <string>
#include <memory>
#include "Fetcher.hpp"
#include "Plugin.hpp"

class Storage {
public:
    explicit Storage(const std::string& path);
    // store structured record: accept url + FetchResult and extracted title+excerpt
    void store(const std::string& url, const FetchResult& fr, const std::string& title, const std::string& excerpt);
    // attach an optional StoragePlugin to mirror writes to plugin
    void set_plugin(std::shared_ptr<StoragePlugin> plugin);
private:
    std::string path_;
    std::shared_ptr<StoragePlugin> plugin_;
};

#include "Storage.hpp"
#include <fstream>
#include <mutex>
#include <iostream>

static std::mutex storage_mutex;

Storage::Storage(const std::string& path) : path_(path) {}

void Storage::set_plugin(std::shared_ptr<StoragePlugin> plugin)
{
    std::lock_guard<std::mutex> g(storage_mutex);
    plugin_ = plugin;
}

void Storage::store(const std::string& url, const FetchResult& fr, const std::string& title, const std::string& excerpt)
{
    std::lock_guard<std::mutex> g(storage_mutex);
    std::ofstream ofs(path_, std::ios::app);
    if (!ofs) { std::cerr << "Storage: failed to open " << path_ << std::endl; return; }
    std::cerr << "Storage: writing to " << path_ << " url=" << url << std::endl;

    // build JSON record into string
    std::ostringstream oss;
    oss << "{";
    oss << "\"url\":\"" << url << "\",";
    oss << "\"status\":" << fr.status << ",";
    oss << "\"content_type\":\"" << fr.content_type << "\",";
    oss << "\"content_length\":" << fr.body.size() << ",";
    oss << "\"fetch_time\":\"" << fr.fetch_time << "\",";
    oss << "\"title\":\"";
    for (char c : title) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else oss << c;
    }
    oss << "\",";
    oss << "\"excerpt\":\"";
    for (char c : excerpt) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else oss << c;
    }
    oss << "\",";
    // headers as object
    oss << "\"headers\":{";
    bool first = true;
    for (auto const& kv : fr.headers) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << kv.first << "\":\"";
        for (char c : kv.second) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
        }
        oss << "\"";
    }
    oss << "},";
    // store truncated body (limit to 8192)
    size_t limit = 8192;
    std::string body = fr.body.size() > limit ? fr.body.substr(0, limit) : fr.body;
    oss << "\"body\":\"";
    for (char c : body) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else oss << c;
    }
    if (fr.body.size() > limit) oss << "...(truncated)";
    oss << "\"}";

    std::string record = oss.str();
    ofs << record << std::endl;

    if (plugin_) {
        try { plugin_->store(record); } catch(...) { /* ignore plugin errors */ }
    }
}

#include "Storage.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
#include <utility>
#include <print>

static std::mutex storage_mutex;

Storage::Storage(const std::string& path) : path_(path) {}

void Storage::set_plugin(std::shared_ptr<StoragePlugin> plugin)
{
    std::lock_guard<std::mutex> g(storage_mutex);
    plugin_ = std::move(plugin);
}

std::expected<void,std::string> Storage::store(const std::string& url, const FetchResult& fr, const std::string& title, const std::string& excerpt) const
{
    std::lock_guard<std::mutex> g(storage_mutex);
    std::ofstream ofs(path_, std::ios::app);
    if (!ofs) { 
        std::cerr << "Storage: failed to open " << path_ << " for writing" << std::endl;
        return std::unexpected("Failed to open storage file");
    }
    std::print("Storage: writing to {} url = {}", path_, url);

    // build JSON record into string
    std::ostringstream oss;
    oss << "{";
    oss << R"("url":")" << url << "\",";
    oss << "\"status\":" << fr.status << ",";
    oss << R"("content_type":")" << fr.content_type << "\",";
    oss << "\"content_length\":" << fr.body.size() << ",";
    oss << R"("fetch_time":")" << fr.fetch_time << "\",";
    oss << R"("title":")";
    for (char c : title) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else oss << c;
    }
    oss << "\",";
    oss << R"("excerpt":")";
    for (char c : excerpt) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else oss << c;
    }
    oss << "\",";
    // headers as object
    oss << "\"headers\":{";
    bool first = true;
    for (const auto& [fst, snd] : fr.headers) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << fst << "\":\"";
        for (char c : snd) {
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
    oss << R"("body":")";
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
        // try { plugin_->store(record); } catch(...) { /* ignore plugin errors */ }
        auto rev =plugin_->store(record);
        if(rev.has_value()) {
            std::print("Storage: plugin store succeeded\n");
        } else {
            std::print("Storage: plugin store failed: {}\n", rev.error());
        }
    }

    return {};
}

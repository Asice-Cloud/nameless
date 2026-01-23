#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Fetcher;
namespace boost::asio
{ class io_context; }

class Robots {
public:
    Robots(boost::asio::io_context& ioc, Fetcher& fetcher, const std::string& user_agent = "*");
    // Ensure rules are available for the URL's host, then invoke cb(allowed)
    void ensure_rules_for(const std::string& url, std::function<void(bool)> cb);
    // Return cached allow/deny if available (true = allowed). If no cached rules, returns true.
    bool is_allowed_cached(const std::string& url);
private:
    struct Rules {
        std::vector<std::string> allow;
        std::vector<std::string> disallow;
        double crawl_delay = -1.0;
    };

    std::mutex mtx_;
    std::unordered_map<std::string, Rules> rules_; // key: host
    boost::asio::io_context& ioc_;
    Fetcher& fetcher_;
    std::string user_agent_;

    void parse_and_store(const std::string& host, const std::string& body);
    static std::string host_from_url(const std::string& url);
    static std::string path_from_url(const std::string& url);
    static bool path_matches(const std::string& rule, const std::string& path);
};

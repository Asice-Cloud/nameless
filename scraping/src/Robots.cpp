#include "Robots.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <libxml/uri.h>
#include "Fetcher.hpp"

static inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b-a);
}

Robots::Robots(boost::asio::io_context& ioc, Fetcher& fetcher, const std::string& user_agent)
: ioc_(ioc), fetcher_(fetcher), user_agent_(user_agent)
{
}

std::string Robots::host_from_url(const std::string& url)
{
    const xmlURIPtr u = xmlParseURI(url.c_str());
    std::string host;
    if (u && u->server) host = reinterpret_cast<const char*>(u->server);
    if (u) xmlFreeURI(u);
    return host;
}

std::string Robots::path_from_url(const std::string& url)
{
    const xmlURIPtr u = xmlParseURI(url.c_str());
    std::string path = "/";
    if (u) {
        if (u->path) path = reinterpret_cast<const char*>(u->path);
        if (u->query) {
            path += std::string("?") + reinterpret_cast<const char*>(u->query);
        }
        xmlFreeURI(u);
    }
    return path;
}

bool Robots::path_matches(const std::string& rule, const std::string& path)
{
    if (rule.empty()) return false;
    // simple prefix match
    if (rule == "/") return true;
    if (path.size() < rule.size()) return false;
    return std::equal(rule.begin(), rule.end(), path.begin());
}

void Robots::parse_and_store(const std::string& host, const std::string& body)
{
    Rules effective;
    std::istringstream in(body);
    std::string line;
    std::vector<std::string> cur_uas;
    std::vector<std::pair<std::string,std::string>> cur_rules; // directive, value

    auto flush_group = [&]() {
        bool matches = false;
        for (const auto& ua : cur_uas) {
            std::string l = ua;
            std::ranges::transform(l, l.begin(), [](const unsigned char c){ return std::tolower(c); });
            std::string target = user_agent_;
            std::ranges::transform(target, target.begin(), [](const unsigned char c){ return std::tolower(c); });
            if (l == "*" || target.find(l) != std::string::npos || l.find(target) != std::string::npos) { matches = true; break; }
        }
        if (matches) {
            for (auto & [fst, snd] : cur_rules) {
                std::string d = fst;
                std::string v = snd;
                if (d == "allow") effective.allow.push_back(v);
                else if (d == "disallow") effective.disallow.push_back(v);
                else if (d == "crawl-delay") {
                    try {
                        effective.crawl_delay = std::stod(v);
                    } catch(...) {}
                }
            }
        }
        cur_uas.clear(); cur_rules.clear();
    };

    while (std::getline(in, line)) {
        auto s = trim(line);
        if (s.empty()) continue;
        // remove comments after #
        if (auto pos = s.find('#'); pos != std::string::npos) s = trim(s.substr(0,pos));
        if (s.empty()) continue;
        // split at ':'
        auto p = s.find(':');
        if (p == std::string::npos) continue;
        std::string key = s.substr(0,p);
        std::string val = trim(s.substr(p+1));
        // normalize
        std::ranges::transform(key, key.begin(), [](const unsigned char c){ return std::tolower(c); });
        if (key == "user-agent") {
            // new group if we already had some directives
            if (!cur_uas.empty() && !cur_rules.empty()) flush_group();
            cur_uas.push_back(val);
        } else if (key == "disallow" || key == "allow" || key == "crawl-delay") {
            cur_rules.emplace_back(key, val);
        }
    }
    if (!cur_uas.empty() || !cur_rules.empty()) flush_group();

    std::lock_guard<std::mutex> g(mtx_);
    rules_[host] = std::move(effective);
}

void Robots::ensure_rules_for(const std::string& url, std::function<void(bool)> cb)
{
    std::string host = host_from_url(url);
    if (host.empty()) { cb(true); return; }

    {
        std::lock_guard<std::mutex> g(mtx_);
        if (const auto it = rules_.find(host); it != rules_.end()) {
            // use cached rules
            const Rules &r = it->second;
            const std::string path = path_from_url(url);
            // find longest matching rule among allow/disallow
            size_t best_len = 0; bool allowed = true; // default allow
            std::string best_type;
            for (const auto &a : r.allow) if (path_matches(a, path) && a.size() >= best_len) { best_len = a.size(); best_type = "allow"; allowed = true; }
            for (const auto &d : r.disallow) if (path_matches(d, path) && d.size() >= best_len) { best_len = d.size(); best_type = "disallow"; allowed = false; }
            cb(allowed);
            return;
        }
    }

    // not cached: fetch robots.txt from same scheme
    const xmlURIPtr u = xmlParseURI(url.c_str());
    std::string scheme = "http";
    if (u && u->scheme) scheme = reinterpret_cast<const char*>(u->scheme);
    if (u) xmlFreeURI(u);
    const std::string robots_url = scheme + std::string("://") + host + "/robots.txt";

    fetcher_.async_fetch(ioc_, robots_url, [this, host, url, cb](const FetchResult& res){
        if (res.status == 200 && !res.body.empty()) {
            parse_and_store(host, res.body);
        } else {
            // store empty rules (allow all)
            std::lock_guard<std::mutex> g(mtx_);
            rules_.emplace(host, Rules());
        }
        // now call back with allowed/denied based on newly cached rules
        bool allowed = true;
        {
            std::lock_guard<std::mutex> g(mtx_);
            const auto &r = rules_[host];
            const std::string path = path_from_url(url);
            size_t best_len = 0; bool a_allowed = true;
            for (const auto &a : r.allow) if (path_matches(a, path) && a.size() >= best_len) { best_len = a.size(); a_allowed = true; }
            for (const auto &d : r.disallow) if (path_matches(d, path) && d.size() >= best_len) { best_len = d.size(); a_allowed = false; }
            allowed = a_allowed;
        }
        cb(allowed);
    });
}

bool Robots::is_allowed_cached(const std::string& url)
{
    const std::string host = host_from_url(url);
    if (host.empty()) return true;
    std::lock_guard<std::mutex> g(mtx_);
    const auto it = rules_.find(host);
    if (it == rules_.end()) return true;
    const auto &r = it->second;
    const std::string path = path_from_url(url);
    size_t best_len = 0; bool a_allowed = true;
    for (const auto &a : r.allow) if (path_matches(a, path) && a.size() >= best_len) { best_len = a.size(); a_allowed = true; }
    for (const auto &d : r.disallow) if (path_matches(d, path) && d.size() >= best_len) { best_len = d.size(); a_allowed = false; }
    return a_allowed;
}

#include "Scheduler.hpp"
#include "Fetcher.hpp"
#include "Parser.hpp"
#include "Storage.hpp"
#include "Robots.hpp"
#include <mutex>
#include <unordered_set>
#include <boost/asio/post.hpp>
#include <iostream>
#include <libxml/uri.h>

Scheduler::Scheduler(boost::asio::io_context& ioc, Fetcher& fetcher, Parser& parser, Storage& storage)
: ioc_(ioc), fetcher_(fetcher), parser_(parser), storage_(storage)
{
    robots_ = std::make_unique<Robots>(ioc_, fetcher_);
}

void Scheduler::add_url(const std::string& url)
{
    // quick de-dup check before scheduling
    static std::mutex mtx;
    static std::unordered_set<std::string> seen;
    {
        std::lock_guard<std::mutex> g(mtx);
        if (seen.find(url) != seen.end()) return;
        seen.insert(url);
    }

    // post task to io_context: start async fetch; callback will handle parse+store
    boost::asio::post(ioc_, [this, url]() {
        fetcher_.async_fetch(ioc_, url, [this, url](const FetchResult& res){
            if (res.body.empty()) {
                std::cerr << "[Scheduler] empty body for " << url << std::endl;
                return;
            }
            auto pr = parser_.parse(url, res.content_type, res.body);
            std::string title = pr.title;
            std::string excerpt = pr.excerpt.empty() ? res.body.substr(0, 512) : pr.excerpt;
            storage_.store(url, res, title, excerpt);
            std::cout << "[Scheduler] fetched " << url << " title='" << title << "'\n";

            // Link filtering and scheduling:
            // - strip fragment
            // - allow only http/https schemes
            // - respect rel=nofollow
            // - optional same-host filtering (default: same-host only)
            const bool only_same_host = true;
            int scheduled = 0;
            const int max_per_page = 50;
            // compute base host
            xmlURIPtr base_uri = xmlParseURI(url.c_str());
            std::string base_host;
            if (base_uri && base_uri->server) base_host = reinterpret_cast<const char*>(base_uri->server);
            if (base_uri) xmlFreeURI(base_uri);
            // iterate links but consult robots.txt before scheduling each
            for (const auto& L : pr.links) {
                if (scheduled >= max_per_page) break;
                if (L.href.empty()) continue;
                if (L.nofollow) continue; // respect nofollow
                // strip fragment
                std::string href = L.href;
                auto posf = href.find('#');
                if (posf != std::string::npos) href = href.substr(0, posf);
                // parse URI
                xmlURIPtr uri = xmlParseURI(href.c_str());
                if (!uri) continue;
                bool ok_scheme = false;
                if (uri->scheme) {
                    std::string scheme = reinterpret_cast<const char*>(uri->scheme);
                    if (scheme == "http" || scheme == "https") ok_scheme = true;
                }
                if (!ok_scheme) { xmlFreeURI(uri); continue; }
                // same-host filter
                if (only_same_host && uri->server) {
                    std::string host = reinterpret_cast<const char*>(uri->server);
                    if (!base_host.empty() && host != base_host) { xmlFreeURI(uri); continue; }
                }
                xmlFreeURI(uri);

                // consult robots (async): if cached allows, schedule immediately; otherwise async fetch will call back
                if (robots_->is_allowed_cached(href)) {
                    this->add_url(href);
                    ++scheduled;
                } else {
                    // ensure_rules_for will call back with allowed flag
                    robots_->ensure_rules_for(href, [this, href, &scheduled, max_per_page](bool allowed){
                        if (!allowed) return;
                        if (scheduled >= max_per_page) return;
                        this->add_url(href);
                        ++scheduled;
                    });
                }
            }
        });
    });
}

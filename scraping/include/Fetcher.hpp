#pragma once
#include <string>
#include <functional>
#include <map>
#include <boost/asio.hpp>

struct FetchResult {
    int status = 0;
    std::string content_type;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string fetch_time; // ISO-like string
};

class Fetcher {
public:
    // callback receives a structured FetchResult; empty body => error
    using Callback = std::function<void(const FetchResult&)>;
    void async_fetch(boost::asio::io_context& ioc, const std::string& url, Callback cb);
};

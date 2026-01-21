#pragma once
#include <string>
#include <memory>
#include <boost/asio.hpp>

class Fetcher;
class Parser;
class Storage;
#include "Robots.hpp"

class Scheduler {
public:
    Scheduler(boost::asio::io_context& ioc, Fetcher& fetcher, Parser& parser, Storage& storage);
    void add_url(const std::string& url);
private:
    boost::asio::io_context& ioc_;
    Fetcher& fetcher_;
    Parser& parser_;
    Storage& storage_;
    std::unique_ptr<Robots> robots_;
};

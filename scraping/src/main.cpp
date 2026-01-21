#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <iostream>

#include "Fetcher.hpp"
#include "Parser.hpp"
#include "Storage.hpp"
#include "Scheduler.hpp"

int main()
{
    boost::asio::io_context ioc;

    Fetcher fetcher;
    Parser parser;
    Storage storage("results.jsonl");
    Scheduler scheduler(ioc, fetcher, parser, storage);

    std::vector<std::string> start_urls = {"http://example.com/", "http://example.com/"};

    for (const auto& u : start_urls) scheduler.add_url(u);

    int concurrency = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (int i = 0; i < concurrency; ++i) {
        workers.emplace_back([&ioc]{ ioc.run(); });
    }

    for (auto& t : workers) if (t.joinable()) t.join();

    std::cout << "All done. Results in results.jsonl" << std::endl;
    return 0;
}

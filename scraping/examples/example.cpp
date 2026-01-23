#include <iostream>
#include <thread>
#include <vector>
#include <boost/asio.hpp>

#include "Fetcher.hpp"
#include "Parser.hpp"
#include "PluginLoader.hpp"
#include "Scheduler.hpp"
#include "Storage.hpp"

int main()
{
    boost::asio::io_context ioc;

    // load plugins from plugins.toml if present
    PluginLoader& pl = PluginLoader::init();
    const std::vector<std::shared_ptr<IPlugin>> plugins = pl.closure();

    Fetcher fetcher;
    Parser parser;
    Storage storage("example_results.jsonl");

    // if any StoragePlugin was loaded, attach the first one
    for (auto &p : plugins) {
        if (const auto sp = std::dynamic_pointer_cast<StoragePlugin>(p)) {
            storage.set_plugin(sp);
            std::cout << "Attached StoragePlugin: " << sp->name() << "\n";
            break;
        }
    }

    Scheduler scheduler(ioc, fetcher, parser, storage);

    for (const std::vector<std::string> start_urls = {"https://example.com/"}; const auto& u : start_urls) scheduler.add_url(u);

    const int concurrency = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    workers.reserve(concurrency);
for (int i = 0; i < concurrency; ++i) {
        workers.emplace_back([&ioc]{ ioc.run(); });
    }

    for (auto& t : workers) if (t.joinable()) t.join();

    std::cout << "Example done. Results in example_results.jsonl" << std::endl;
    return 0;
}

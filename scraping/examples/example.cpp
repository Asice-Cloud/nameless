#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <iostream>

#include "Fetcher.hpp"
#include "Parser.hpp"
#include "Storage.hpp"
#include "Scheduler.hpp"
#include "PluginLoader.hpp"
#include <filesystem>

int main()
{
    boost::asio::io_context ioc;

    // load plugins from plugins.toml if present
    PluginLoader pl;
    std::vector<std::shared_ptr<IPlugin>> plugins;
    if (std::filesystem::exists("../plugins.toml")) {
        plugins = pl.load_from_config("../plugins.toml");
        std::cout << "Loaded " << plugins.size() << " plugins from ../plugins.toml\n";
    } else if (std::filesystem::exists("plugins.toml")) {
        plugins = pl.load_from_config("plugins.toml");
        std::cout << "Loaded " << plugins.size() << " plugins from plugins.toml\n";
    }

    Fetcher fetcher;
    Parser parser;
    Storage storage("example_results.jsonl");

    // if any StoragePlugin was loaded, attach the first one
    for (auto &p : plugins) {
        auto sp = std::dynamic_pointer_cast<StoragePlugin>(p);
        if (sp) {
            storage.set_plugin(sp);
            std::cout << "Attached StoragePlugin: " << sp->name() << "\n";
            break;
        }
    }

    Scheduler scheduler(ioc, fetcher, parser, storage);

    std::vector<std::string> start_urls = {"https://example.com/"};

    for (const auto& u : start_urls) scheduler.add_url(u);

    int concurrency = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (int i = 0; i < concurrency; ++i) {
        workers.emplace_back([&ioc]{ ioc.run(); });
    }

    for (auto& t : workers) if (t.joinable()) t.join();

    std::cout << "Example done. Results in example_results.jsonl" << std::endl;
    return 0;
}

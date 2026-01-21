# Simple Crawler (prototype)

Lightweight asynchronous C++ crawler prototype using Boost.Asio/Beast, OpenSSL and libxml2.

Status
------
- Implemented modules: `Fetcher`, `Parser`, `Storage`, `Scheduler`, `Robots`.
- Async HTTP/HTTPS fetcher with TLS/SNI, libxml2 HTML parsing, charset conversion (iconv), sanitizer and Readability-like extraction.
- Example `examples/example.cpp` produces structured JSONL at `build/example_results.jsonl`.

Build
-----
Requirements: C++23, CMake, Boost (asio/beast), OpenSSL, libxml2, iconv.

Quick build and run:

```bash
mkdir -p build && cd build
cmake ..
make -j
./example_fetch
```

Public API (headers)
---------------------
Notes: the code exposes small, focused components. See the `include/` headers for full types.

- `include/Fetcher.hpp`
  - `struct FetchResult { int status; std::string content_type; std::string body; std::map<std::string,std::string> headers; std::string fetch_time; }`
  - `void Fetcher::async_fetch(boost::asio::io_context& ioc, const std::string& url, Callback cb)` where `Callback` is `std::function<void(const FetchResult&)>`.

- `include/Parser.hpp`
  - `struct Link { std::string href; std::string text; bool nofollow; }`
  - `struct ParseResult { std::string title; std::string meta_description; std::string canonical; std::vector<Link> links; std::string text; std::string excerpt; std::string charset; std::string safe_html; std::vector<std::string> errors; }`
  - `ParseResult Parser::parse(const std::string& url, const std::string& content_type, const std::string& body)` — handles charset detection/conversion, sanitization, main-content extraction and link resolution.

- `include/Storage.hpp`
  - `Storage::store(const std::string& url, const FetchResult& fr, const std::string& title, const std::string& excerpt)` — appends a JSONL record containing `url,status,content_type,content_length,fetch_time,title,excerpt,headers,body` (body is truncated).

- `include/Scheduler.hpp`
  - `Scheduler(boost::asio::io_context& ioc, Fetcher& fetcher, Parser& parser, Storage& storage)` — coordinates fetch→parse→store and link scheduling.
  - `void Scheduler::add_url(const std::string& url)` — de-duplicates, schedules fetches, and enqueues discovered links (applies link filtering and consults `Robots`).

-- `include/Robots.hpp`
  - `Robots::ensure_rules_for(const std::string& url, std::function<void(bool)> cb)` — ensures `robots.txt` rules for the host are cached; invokes callback with `allowed` flag.
  - `Robots::is_allowed_cached(const std::string& url)` — returns cached allow/deny when available.

Plugin configuration
--------------------
Plugins are declared in `plugins.toml` as an array of tables. Example:

```toml
[[plugin]]
path = "build/jsonl_storage_plugin.so"
[plugin.config]
file = "plugin_out.jsonl"
```

`PluginLoader::load_from_config("plugins.toml")` will parse the file and load each `plugin.path` using `dlopen`, passing the `plugin.config` key/value pairs to the plugin's `init()`.

Building and placing plugins
----------------------------
- Example plugins live in the `plugins/` folder. The build will automatically add plugin subdirectories that contain a `CMakeLists.txt` (see `plugins/jsonl_storage_plugin/`).
- By default the sample plugin (`jsonl_storage_plugin`) is built into the `build/` directory as `build/jsonl_storage_plugin.so`.
- `plugins.toml` paths are resolved relative to the `plugins.toml` file location; when running the example from `build/` the loader looks for `../plugins.toml` first.
- After loading, the sample plugin writes configured output (e.g. `plugin_out.jsonl`) into the current working directory; in the example run this is observed at `build/plugin_out.jsonl`.

Quick plugin build & run
------------------------
From the repository root:

```bash
mkdir -p build && cd build
cmake ..
make -j
./example_fetch   # will load build/jsonl_storage_plugin.so per plugins.toml
```

After running, check the plugin output (example):

```bash
sed -n '1,120p' build/plugin_out.jsonl
```

Plugin troubleshooting
----------------------
- If a plugin file cannot be found, `PluginLoader` will print: `[PluginLoader] Plugin #N path not found: /abs/path (skipping)`.
- If `dlopen` or symbol lookup fails you will see messages like `dlopen failed: ...` or `missing create_plugin in ...` on stderr.
- If the plugin `create_plugin()` or `init()` fails, you will see `plugin create threw:` or `plugin init failed for ...` and the loader will skip that plugin.
- Use absolute paths in `plugins.toml` or place the `.so` relative to the `plugins.toml` location; relative paths are resolved against the `plugins.toml` directory.
- Recommended workflow:
  1. Build the plugin shared object next to `plugins.toml` (or use absolute path).
  2. Run `./example_fetch` and inspect stdout/stderr for `PluginLoader` messages.
  3. If you need more structured validation, I can add an explicit `--validate-plugins plugins.toml` CLI that reports issues without starting the crawler.

Example usage
-------------
`examples/example.cpp` demonstrates standard wiring:

- create `boost::asio::io_context` and a small thread pool
- instantiate `Fetcher`, `Parser`, `Storage` and `Scheduler`
- call `scheduler.add_url("https://example.com/")`
- run `io_context.run()` across threads

Output
------
`Storage` writes one JSON object per line to `build/example_results.jsonl`. Each record contains fields like:

- `url`, `status`, `content_type`, `content_length`, `fetch_time` (ISO8601)
- `title`, `excerpt`, `headers` (object), `body` (truncated)

Design notes & next steps
-------------------------
- Current pipeline: fetch → charset conversion → libxml2 parse → sanitize → readability extraction → link filtering → robots check → schedule.
- Missing / planned features: per-host rate limits & `crawl-delay` enforcement, persistent frontier and dedupe, pluggable storage backends, proxy support, optional JS rendering, observability/metrics and distributed coordination.

Contributing
------------
- Build with CMake, edit `include/` and `src/`, run `./example_fetch` to validate.

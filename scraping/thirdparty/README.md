Place third-party vendored libraries here.

To vendor `tomlplusplus` for offline builds or to freeze a version, clone the tomlplusplus repository into `thirdparty/tomlplusplus`:

```bash
mkdir -p thirdparty
cd thirdparty
git clone https://github.com/marzer/tomlplusplus.git tomlplusplus
cd tomlplusplus
# optionally checkout a specific tag
git checkout v3.4.0
```

When `thirdparty/tomlplusplus/CMakeLists.txt` exists, the top-level `CMakeLists.txt` will prefer the vendored copy instead of fetching via `FetchContent`.

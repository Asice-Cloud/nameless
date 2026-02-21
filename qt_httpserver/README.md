# Qt Minimal HTTP Server

This repository is a small example HTTP server built with Qt (Core + Network).

Layout
- include/: public headers (`httpserver.h`, `router.h`, `logger.h`)
- src/: implementation and `main.cpp`
- www/: static files served by the server

Build
```bash
cmake -S . -B build
cmake --build build -j
```

Run
```bash
./build/qtl
# then in another terminal:
curl http://localhost:8080/hello
```

Features
- Basic routing with path parameters (e.g. `/user/:id`)
- Method-specific routes (`GET`, `POST`, `*`)
- Thread-pool execution for handlers
- Static file serving from `www/`

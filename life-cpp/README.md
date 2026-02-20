Qt C++ Conway's Game of Life

Build (requires Qt6 and CMake):

```bash
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="$(qmake -query QT_INSTALL_PREFIX)" ..
cmake --build .
./qt_life_cpp
```

Keys:
- Space: start/stop
- S: single step
- C: clear
- R: randomize

Click grid cells to toggle. Hold left mouse button and drag to draw (set alive); hold right mouse button and drag to erase.
Use the `Load` button to import a simple pattern text file. Pattern format: one `x y` coordinate per line (comments start with `#`). Example patterns are in the `patterns/` folder (`glider.txt`, `lwss.txt`).

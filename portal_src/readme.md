# Portal Gomoku Engine

Portal Gomoku Engine is a specialized, high-performance C++ game engine for Gomoku and Renju, forked and extended from the [Rapfi project](https://github.com/hzyhhzy/Rapfi). 

While it retains Rapfi's state-of-the-art Alpha-Beta and MCTS search frameworks, Portal Gomoku Engine introduces novel rule mechanics, allowing Custom Board Topologies via unplayable **Wall** elements and topological **Portals**.

---

## 🌟 Key Features

1. **Wall Cells:** Immovable, unplayable grid cells that block lines. Similar to playing near the edge of the board, but they can be placed anywhere.
2. **Teleportation Portals (A, B pairs):** Zero-width cells that connect two points on the board. A straight continuous string of stones passing into Portal A will magically exit out of Portal B (and vice versa) seamlessly, continuing in the exact same direction.
3. **High-Performance Pattern Evaluator:** Bitboard routines and pattern matching logic deeply extended and optimized to handle zero-width portal skipping natively without breaking original AVX2/BMI2 vectorizations.
4. **Gomocup Protocol Extended Support:** Full support for standard graphical UIs such as Yixin-Board, along with specific custom commands (`INFO WALL`, `INFO YXPORTAL`) to define the board topologies dynamically.

---

## 📁 Source Layout

The primary modified source rests inside `/portal_src/`. Key changes from original Rapfi reside primarily in:

- `game/board.h` & `game/board.cpp`: Introduces the `Portal` structs and modified `getKeyAt()` methods. Employs a dual-path design for ultra-fast bitkeys when unaffected by portals, and a line-walking path when stepping through teleportations.
- `command/gomocup.cpp`: Connects the UI to the Engine through `stdin`/`stdout`. Custom handlers implemented for configuring board topography before match iterations.
- `config.h` & `main.cpp`: Altered to load new portal defaults and entry logic.

For a detailed explanation of how UI developers can pipe board setup commands (Walls, Portals, starting matches) to the engine, please read:
👉 **[protocol.md](./protocol.md)**

---

## 🛠️ Building the Engine

The engine uses CMake and requires a modern C++ compiler (C++17 compliant).

### Prerequisites
- CMake >= 3.18.2
- A C++17 compiler (GCC, Clang, or MSVC)
- Hardware supporting SIMD (SSE/AVX2/BMI2) is heavily recommended for extreme performance.

### Compilation Steps

1. Navigate to your build directory (for instance, creating a `build` folder next to or inside `portal_src`).
2. Run CMake and build:
```bash
mkdir build && cd build
cmake ../portal_src -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 4
```

This will produce the executable `pbrain-MINT-P`, which is universally ready to plug straight into GUI software that adopts Gomocup protocol implementations.

To execute tests and ensure the portal mechanics operate correctly:
```bash
./portal-test
./portal-pattern-test
```

---

## 📜 Acknowledgements & License

Portal Gomoku Engine builds upon the open-source architecture of the Rapfi Engine. Please respect original GNU General Public License terms provided in the respective source files.

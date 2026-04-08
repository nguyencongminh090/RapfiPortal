# Portal Gomoku Analysis Tool — GTK4 C++ Architecture

## Goal

Design the core architecture for a **professional Gomoku analysis tool** built with **GTK4/gtkmm-4.0** in C++20, supporting the full MINT-P engine protocol including Portal/WALL board setup, N-best analysis, database integration, and game recording.

> [!IMPORTANT]
> This document refines and expands the initial design from `PortalUI_FullReport.md`. Key improvements:
> - Uses `Glib::Dispatcher` instead of custom `ThreadQueue` (idiomatic GTK4)
> - Uses `Gio::Subprocess` + async `read_line_async` instead of raw `popen`
> - Adds proper RAII ownership model with `std::unique_ptr`
> - Adds State Machine diagram for engine lifecycle
> - Adds missing commands (`YXSEARCHDEFEND`, `BENCH`)

---

## 1. Folder Structure

```
gomoku-portal-ui/
│
├── meson.build                          ← Build system (Meson, native GTK4 support)
├── meson_options.txt                    ← Build options (debug, sanitizers, etc.)
│
├── src/
│   ├── main.cpp                         ← Entry point: Gtk::Application creation
│   │
│   ├── app/                             ← APPLICATION SHELL
│   │   ├── Application.hpp/.cpp         ← Gtk::Application subclass, window management
│   │   ├── AppActions.hpp/.cpp          ← Gio::SimpleAction bindings (menu/shortcuts)
│   │   └── AppSettings.hpp/.cpp         ← Gio::Settings wrapper (persistent prefs)
│   │
│   ├── engine/                          ← ENGINE COMMUNICATION LAYER (no GTK widget dependency)
│   │   ├── EngineProcess.hpp/.cpp       ← Gio::Subprocess wrapper, async pipe I/O
│   │   ├── EngineProtocol.hpp/.cpp      ← Stateless command builder + output parser
│   │   ├── EngineController.hpp/.cpp    ← High-level async API, state machine, signal emission
│   │   ├── EngineState.hpp              ← Enum class: Disconnected, Idle, Thinking, Stopping
│   │   └── EngineConfig.hpp/.cpp        ← Value object: engine path, rule, threads, memory, time
│   │
│   ├── model/                           ← DOMAIN MODEL LAYER (pure data, zero UI dependency)
│   │   ├── Board.hpp/.cpp               ← Board state: cells, move history, ply counter
│   │   ├── Cell.hpp                     ← Enum: Empty, Black, White, Wall, PortalA, PortalB
│   │   ├── Move.hpp                     ← Value object: {x, y, color, ply}
│   │   ├── PortalTopology.hpp/.cpp      ← Wall positions + portal pairs (zero-width metadata)
│   │   ├── GameRecord.hpp/.cpp          ← Full game: metadata + move list + topology (serializable)
│   │   ├── AnalysisInfo.hpp             ← Search info: depth, score, nodes, nps, pv line
│   │   ├── AnalysisMove.hpp             ← NBest entry: {move, score, rank, pv}
│   │   └── DatabaseRecord.hpp           ← DB query result value object
│   │
│   ├── controller/                      ← APPLICATION LOGIC LAYER (mediates model ↔ engine ↔ view)
│   │   ├── GameController.hpp/.cpp      ← Central orchestrator: all user actions route through here
│   │   ├── SetupController.hpp/.cpp     ← WALL/Portal placement mode FSM
│   │   ├── AnalysisController.hpp/.cpp  ← NBest, balance, trace, multiPV management
│   │   └── DatabaseController.hpp/.cpp  ← DB query/edit/import/export abstraction
│   │
│   ├── ui/                              ← VIEW LAYER (GTK4 widgets only)
│   │   ├── MainWindow.hpp/.cpp          ← Gtk::ApplicationWindow, top-level layout
│   │   ├── HeaderBar.hpp/.cpp           ← Gtk::HeaderBar with menu/toolbar buttons
│   │   │
│   │   ├── board/                       ← Board rendering subsystem
│   │   │   ├── BoardCanvas.hpp/.cpp     ← Gtk::DrawingArea: renders board via BoardRenderer
│   │   │   ├── BoardRenderer.hpp/.cpp   ← Cairo drawing: grid, stones, walls, portals, overlays
│   │   │   ├── BoardGestures.hpp/.cpp   ← Gtk::GestureClick + Gtk::EventControllerMotion
│   │   │   └── BoardOverlay.hpp/.cpp    ← NBest arrows, last-move indicator, coordinates
│   │   │
│   │   ├── panels/                      ← Side panels
│   │   │   ├── EnginePanel.hpp/.cpp     ← Engine status, connect/disconnect, state indicator
│   │   │   ├── AnalysisPanel.hpp/.cpp   ← NBest list (Gtk::ListView), eval bar, search stats
│   │   │   ├── MoveListPanel.hpp/.cpp   ← Move history (Gtk::ListView), click-to-navigate
│   │   │   ├── LogPanel.hpp/.cpp        ← Scrolling MESSAGE/ERROR log (Gtk::TextView)
│   │   │   └── DatabasePanel.hpp/.cpp   ← DB browser: query results, edit, labels
│   │   │
│   │   ├── dialogs/                     ← Modal dialogs
│   │   │   ├── NewGameDialog.hpp/.cpp   ← Board size, rule, time control
│   │   │   ├── BoardSetupDialog.hpp/.cpp ← Visual WALL/Portal editor (embedded BoardCanvas)
│   │   │   ├── EngineSettingsDialog.hpp/.cpp ← Engine executable path, threads, memory
│   │   │   ├── AboutDialog.hpp/.cpp     ← Version info + engine ABOUT response
│   │   │   └── ExportDialog.hpp/.cpp    ← Export: SGF, PGN, lib, CSV
│   │   │
│   │   └── widgets/                     ← Reusable custom widgets
│   │       ├── EvalBar.hpp/.cpp         ← Win-rate bar (black/white gradient)
│   │       ├── ClockWidget.hpp/.cpp     ← Per-side chess-style timer
│   │       ├── CoordOverlay.hpp/.cpp    ← A-O / 1-15 labels around board edge
│   │       └── StatusIndicator.hpp/.cpp ← Colored dot: green=idle, yellow=thinking, red=dead
│   │
│   └── util/                            ← UTILITIES (no GTK, no engine dependency)
│       ├── Coord.hpp                    ← Coordinate conversion: board ↔ pixel ↔ protocol
│       ├── Logger.hpp/.cpp              ← Simple file/console logger
│       ├── FileUtils.hpp/.cpp           ← Path resolution, save/load helpers
│       └── StringUtils.hpp              ← trim, split, upper, parseInt
│
├── resources/
│   ├── resources.gresource.xml          ← GResource manifest
│   ├── ui/                              ← Gtk::Builder XML templates
│   │   ├── main_window.ui
│   │   ├── new_game_dialog.ui
│   │   ├── board_setup_dialog.ui
│   │   └── engine_settings_dialog.ui
│   ├── icons/                           ← SVG: wall.svg, portal.svg, stone_black.svg, etc.
│   ├── css/                             ← GTK4 CSS themes
│   │   ├── light.css
│   │   └── dark.css
│   └── shortcuts.ui                     ← Keyboard shortcut window definition
│
├── data/
│   └── org.portal.gomoku.gschema.xml    ← GSettings schema (persistent preferences)
│
└── tests/
    ├── meson.build
    ├── test_engine_protocol.cpp          ← Unit: command building + output parsing
    ├── test_board.cpp                    ← Unit: board model logic
    ├── test_portal_topology.cpp          ← Unit: wall/portal management
    └── test_game_record.cpp             ← Unit: serialization round-trip
```

---

## 2. Object-Oriented Design

### 2.1 Layered Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        UI LAYER (GTK4)                          │
│  BoardCanvas, MainWindow, Panels, Dialogs, Widgets              │
│  Rule: Only talks to Controllers. Never touches Engine directly. │
└──────────────────────────┬──────────────────────────────────────┘
                           │ signals (sigc++)
┌──────────────────────────▼──────────────────────────────────────┐
│                    CONTROLLER LAYER                              │
│  GameController, SetupController, AnalysisController             │
│  Rule: Owns Model objects. Talks to EngineController + UI.       │
└──────────────────────────┬──────────────────────────────────────┘
                           │ method calls + signals
┌────────────────┬─────────▼─────────┐
│  MODEL LAYER   │  ENGINE LAYER     │
│  Board, Move,  │  EngineProcess,   │
│  GameRecord,   │  EngineProtocol,  │
│  PortalTopology│  EngineController │
│                │  EngineState      │
│  Rule: Pure    │  Rule: No GTK     │
│  data. No I/O. │  widget deps.     │
└────────────────┴───────────────────┘
```

> [!IMPORTANT]
> **Dependency Rule:** Each layer only depends on layers below it. UI → Controller → Model/Engine. Never upward.

---

### 2.2 Engine Layer — State Machine

```
                    connect()
    ┌──────────┐  ───────────►  ┌──────────┐
    │DISCONNECT│                │   IDLE   │◄─────────────────┐
    │   ED     │  ◄───────────  │          │                  │
    └──────────┘  disconnect()  └────┬─────┘                  │
                                     │ BEGIN/TURN/BOARD        │
                                     │ requestNBest()          │
                                     ▼                        │
                                ┌──────────┐    engine move   │
                                │ THINKING │ ─────────────────┘
                                │          │    or OK
                                └────┬─────┘
                                     │ STOP/YXSTOP
                                     ▼
                                ┌──────────┐    best-so-far
                                │ STOPPING │ ─────────► IDLE
                                └──────────┘
```

**State transition rules enforced by `EngineController`:**
- `makeMove()` / `requestEngineMove()` only allowed in `IDLE`
- `stopThinking()` only allowed in `THINKING`
- `addWall()` / `addPortal()` / `clearPortals()` allowed in `IDLE` (they trigger internal reinit)
- `disconnect()` force-kills process from any state

---

### 2.3 Core Class Designs

#### 2.3.1 EngineProcess — Subprocess Management (RAII)

```cpp
/// Manages the engine subprocess lifecycle using Gio::Subprocess.
/// Provides async line-by-line reading from stdout and blocking writes to stdin.
/// Uses Glib::Dispatcher to safely notify the main thread of new output.
class EngineProcess {
public:
    EngineProcess();
    ~EngineProcess();  // kills process if alive

    // Lifecycle
    bool launch(const std::filesystem::path& execPath,
                const std::filesystem::path& workDir);
    void kill();
    [[nodiscard]] bool isAlive() const;

    // I/O
    void sendLine(std::string_view command);   // write to stdin + '\n'

    // Signal: emitted on GTK main thread when a new line arrives from stdout
    sigc::signal<void(const std::string&)>& signalLineReceived();

private:
    Glib::RefPtr<Gio::Subprocess>    process_;
    Glib::RefPtr<Gio::DataInputStream> stdoutStream_;
    Glib::RefPtr<Gio::OutputStream>    stdinStream_;

    Glib::Dispatcher                 dispatcher_;      // thread → main thread
    std::mutex                       bufferMutex_;
    std::vector<std::string>         lineBuffer_;       // accumulated lines

    void startAsyncRead();                              // chains read_line_async
    void onLineReady(Glib::RefPtr<Gio::AsyncResult>& result);
    void onDispatch();                                  // main thread: drain buffer
};
```

#### 2.3.2 EngineProtocol — Stateless Parser/Builder

```cpp
/// Pure-function command builder and output parser.
/// No state, no I/O, no GTK dependency. Fully unit-testable.
class EngineProtocol {
public:
    // --- Command Builders (return string ready to send) ---

    // Session
    static std::string start(int boardSize);        // "START 15"
    static std::string restart();                    // "RESTART"
    static std::string end();                        // "END"
    static std::string about();                      // "ABOUT"
    static std::string yxShowInfo();                 // "YXSHOWINFO"

    // Gameplay
    static std::string begin();                      // "BEGIN"
    static std::string turn(int x, int y);           // "TURN 7,7"
    static std::string takeBack(int x, int y);       // "TAKEBACK 7,7"
    static std::string stop();                       // "STOP"
    static std::string board(const std::vector<std::tuple<int,int,int>>& entries);
    static std::string yxBoard(const std::vector<std::tuple<int,int,int>>& entries);

    // Portal Extensions
    static std::string infoWall(int x, int y);                       // "INFO WALL 3,3"
    static std::string infoPortal(int ax, int ay, int bx, int by);   // "INFO YXPORTAL 5,5 10,10"
    static std::string infoClearPortals();                            // "INFO CLEARPORTALS"

    // Configuration
    static std::string infoRule(int rule);           // "INFO RULE 0"
    static std::string infoTimeoutTurn(int ms);
    static std::string infoTimeoutMatch(int ms);
    static std::string infoMaxMemory(size_t bytes);  // NOTE: unit is bytes
    static std::string infoThreadNum(int n);
    static std::string infoStrength(int level);
    static std::string infoMaxDepth(int depth);
    static std::string infoPondering(bool enable);
    static std::string infoShowDetail(int level);

    // Analysis
    static std::string yxNBest(int n);
    static std::string traceBoard();
    static std::string traceSearch();

    // Hash
    static std::string yxHashClear();
    static std::string yxShowHashUsage();
    static std::string yxHashDump(const std::string& path);
    static std::string yxHashLoad(const std::string& path);

    // Database
    static std::string yxSetDatabase(const std::string& path);
    static std::string yxSaveDatabase();
    static std::string yxQueryDatabaseAll();
    static std::string yxQueryDatabaseOne();
    static std::string yxQueryDatabaseText();
    static std::string yxDeleteDatabaseOne();

    // --- Output Parser ---
    struct ParsedLine {
        enum class Type {
            Move,           // "7,7"
            MoveDouble,     // "7,7 8,8" (Swap2/Balance)
            Swap,           // "SWAP"
            Ok,             // "OK"
            Message,        // "MESSAGE ..."
            Error,          // "ERROR ..."
            Forbid,         // "FORBID 0506..."
            DatabaseLine,   // "DATABASE ..."
            DatabaseDone,   // "DATABASE DONE"
            EngineInfo,     // ABOUT response: name="...", version="..."
            Unknown
        };

        Type        type;
        int         x1 = -1, y1 = -1;  // first move
        int         x2 = -1, y2 = -1;  // second move (MoveDouble only)
        std::string text;               // raw text content for Message/Error/Database/Info
        std::vector<std::pair<int,int>> forbidPoints;  // for Forbid type
    };

    static ParsedLine parse(const std::string& line);
};
```

#### 2.3.3 EngineController — Async Orchestrator

```cpp
/// High-level engine interaction API with state machine enforcement.
/// All methods are called from the GTK main thread.
/// Engine responses are delivered via sigc++ signals (also on main thread).
class EngineController {
public:
    EngineController();
    ~EngineController();

    // --- Lifecycle ---
    bool connect(const EngineConfig& config);
    void disconnect();
    [[nodiscard]] EngineState state() const { return state_; }

    // --- Game Commands (only valid in IDLE state) ---
    void startGame(int boardSize, int rule);
    void makeMove(int x, int y);
    void requestEngineMove();      // BEGIN
    void takeBack(int x, int y);
    void loadPosition(const GameRecord& record);  // BOARD or YXBOARD

    // --- Portal Setup (only valid in IDLE state) ---
    void addWall(int x, int y);
    void addPortal(int ax, int ay, int bx, int by);
    void clearPortals();

    // --- Analysis ---
    void requestNBest(int n);
    void stopThinking();           // STOP (valid in THINKING)
    void traceBoard();
    void traceSearch();

    // --- Configuration ---
    void applyConfig(const EngineConfig& config);  // sends all INFO params

    // --- Signals (all emitted on GTK main thread) ---
    sigc::signal<void(int, int)>&          onEngineMove();      // engine played x,y
    sigc::signal<void(int,int,int,int)>&   onEngineMove2();     // double move
    sigc::signal<void()>&                  onSwap();
    sigc::signal<void()>&                  onOk();              // RESTART/TAKEBACK response
    sigc::signal<void(const std::string&)>& onMessage();        // MESSAGE lines
    sigc::signal<void(const std::string&)>& onError();          // ERROR lines
    sigc::signal<void(const AnalysisInfo&)>& onAnalysisUpdate();
    sigc::signal<void(EngineState)>&       onStateChanged();
    sigc::signal<void(const std::string&)>& onAboutInfo();

private:
    EngineProcess  process_;
    EngineState    state_ = EngineState::Disconnected;

    void setState(EngineState newState);
    void onLineReceived(const std::string& line);  // connected to EngineProcess signal
    void dispatchParsed(const EngineProtocol::ParsedLine& parsed);

    // State guard: throws if not in expected state
    void requireState(EngineState expected, const char* action) const;
};
```

#### 2.3.4 Board (Model) — Lightweight Mirror

```cpp
/// Lightweight client-side board mirror.
/// Does NOT duplicate the engine's pattern detection — only tracks cell state
/// for rendering and game record purposes.
class Board {
public:
    explicit Board(int size = 15);

    // --- Mutators ---
    void reset(int size);
    void placeStone(int x, int y, Cell color);  // Black or White
    void undoLast();
    void setTopology(const PortalTopology& topo);
    void clearTopology();

    // --- Queries ---
    [[nodiscard]] int      size() const;
    [[nodiscard]] int      ply() const;
    [[nodiscard]] Cell     cellAt(int x, int y) const;
    [[nodiscard]] bool     isEmpty(int x, int y) const;
    [[nodiscard]] bool     isWall(int x, int y) const;
    [[nodiscard]] bool     isPortal(int x, int y) const;
    [[nodiscard]] std::optional<std::pair<int,int>> portalPartner(int x, int y) const;
    [[nodiscard]] const Move& lastMove() const;
    [[nodiscard]] const std::vector<Move>& history() const;
    [[nodiscard]] const PortalTopology& topology() const;

    // --- Serialization ---
    [[nodiscard]] GameRecord toRecord(const std::string& metadata = "") const;
    void fromRecord(const GameRecord& record);

private:
    int                    size_;
    std::vector<Cell>      cells_;       // size_ * size_, row-major
    std::vector<Move>      history_;
    PortalTopology         topology_;
};
```

#### 2.3.5 GameController — Central Orchestrator (Façade)

```cpp
/// The single point of contact for all UI actions.
/// Routes user interactions to the appropriate subsystem.
/// Owns the Board model and coordinates EngineController ↔ UI signals.
class GameController {
public:
    explicit GameController(EngineController& engine);

    // --- Game Lifecycle ---
    void newGame(int boardSize, int rule, const PortalTopology& topology);
    void restartGame();

    // --- User Actions ---
    void humanMove(int x, int y);       // validates → board.place → engine.turn
    void requestEngineMove();           // engine.begin
    void undoMove();                    // engine.takeback → board.undo (x2 if vs engine)
    void stopEngine();

    // --- Board Setup ---
    void addWall(int x, int y);
    void addPortal(int ax, int ay, int bx, int by);
    void clearTopology();

    // --- Analysis ---
    void startAnalysis(int multiPV);
    void stopAnalysis();

    // --- Accessors ---
    [[nodiscard]] const Board& board() const;
    [[nodiscard]] bool isHumanTurn() const;

    // --- Signals for UI binding ---
    sigc::signal<void()>&                   onBoardChanged();   // triggers redraw
    sigc::signal<void(const Move&)>&        onMoveMade();       // for move list
    sigc::signal<void()>&                   onGameReset();      // clear panels
    sigc::signal<void(const AnalysisInfo&)>& onAnalysisUpdate();

private:
    EngineController& engine_;
    Board             board_;
    bool              humanIsBlack_ = true;

    void onEngineMove(int x, int y);    // handler for engine signal
    void onOk();                        // handler for RESTART/TAKEBACK
};
```

---

### 2.4 Design Patterns Summary

| Pattern | Component | Purpose |
|---------|-----------|---------|
| **Façade** | `GameController` | Single API for all UI actions; hides engine protocol + model coordination |
| **State Machine** | `EngineController` | Enforces valid command sequences; prevents race conditions |
| **Observer (Signal/Slot)** | `sigc++` throughout | Decouples async engine output from UI; type-safe, GTK-native |
| **Strategy** | `BoardRenderer` | Swappable rendering backend (Cairo today, GL/Vulkan later) |
| **Command** | `EngineProtocol` static methods | Each protocol command is a pure function → trivially testable |
| **RAII** | `EngineProcess` | Subprocess killed in destructor; no resource leaks |
| **MVC** | `Board`(M) + `BoardCanvas`(V) + `GameController`(C) | Classic separation |
| **Mediator** | `GameController` | Coordinates interactions between engine, model, and multiple UI panels |

---

### 2.5 Threading Model

```
┌──────────────────────────────────────────────────────┐
│  GTK MAIN THREAD                                     │
│                                                      │
│  ┌─────────────┐   signals    ┌──────────────────┐   │
│  │ BoardCanvas  │◄────────────│ GameController   │   │
│  │ Panels       │             │                  │   │
│  │ Dialogs      │────────────►│ humanMove()      │   │
│  └─────────────┘  user events │ undoMove()       │   │
│                               └────────┬─────────┘   │
│                                        │              │
│                               ┌────────▼─────────┐   │
│  Glib::Dispatcher ──────────► │EngineController  │   │
│  (thread-safe wakeup)         │ onLineReceived() │   │
│                               └────────┬─────────┘   │
└────────────────────────────────────────┼──────────────┘
                                         │
┌────────────────────────────────────────▼──────────────┐
│  ENGINE I/O (internal to EngineProcess)               │
│                                                       │
│  read_line_async() callback chain                     │
│  (GLib main context integration — NOT a separate      │
│   thread if using Gio async I/O correctly)            │
│                                                       │
│  Alternatively: std::thread + Glib::Dispatcher        │
│  if using raw pipe reads                              │
└───────────────────────────────────────────────────────┘
                          │ stdin/stdout pipes
┌─────────────────────────▼─────────────────────────────┐
│  ENGINE SUBPROCESS (pbrain-MINT-P)                    │
│  gomocupLoop() running                                │
└───────────────────────────────────────────────────────┘
```

> [!TIP]
> **Preferred approach:** Use `Gio::DataInputStream::read_line_async()` which integrates natively with the GLib main loop. The callback fires on the main thread — no `Dispatcher` needed. Only fall back to `std::thread` + `Dispatcher` if you need raw pipe control.

---

### 2.6 Build System (Meson)

```meson
project('gomoku-portal-ui', 'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++20', 'warning_level=2'])

gtkmm_dep = dependency('gtkmm-4.0', version: '>= 4.10')

gnome = import('gnome')
resources = gnome.compile_resources('app-resources',
    'resources/resources.gresource.xml',
    source_dir: 'resources')

sources = files(
    'src/main.cpp',
    'src/app/Application.cpp',
    'src/app/AppActions.cpp',
    'src/app/AppSettings.cpp',
    'src/engine/EngineProcess.cpp',
    'src/engine/EngineProtocol.cpp',
    'src/engine/EngineController.cpp',
    'src/engine/EngineConfig.cpp',
    'src/model/Board.cpp',
    'src/model/PortalTopology.cpp',
    'src/model/GameRecord.cpp',
    'src/controller/GameController.cpp',
    'src/controller/SetupController.cpp',
    'src/controller/AnalysisController.cpp',
    'src/controller/DatabaseController.cpp',
    'src/ui/MainWindow.cpp',
    'src/ui/HeaderBar.cpp',
    'src/ui/board/BoardCanvas.cpp',
    'src/ui/board/BoardRenderer.cpp',
    'src/ui/board/BoardGestures.cpp',
    'src/ui/board/BoardOverlay.cpp',
    'src/ui/panels/EnginePanel.cpp',
    'src/ui/panels/AnalysisPanel.cpp',
    'src/ui/panels/MoveListPanel.cpp',
    'src/ui/panels/LogPanel.cpp',
    'src/ui/panels/DatabasePanel.cpp',
    'src/ui/dialogs/NewGameDialog.cpp',
    'src/ui/dialogs/BoardSetupDialog.cpp',
    'src/ui/dialogs/EngineSettingsDialog.cpp',
    'src/ui/dialogs/AboutDialog.cpp',
    'src/ui/dialogs/ExportDialog.cpp',
    'src/ui/widgets/EvalBar.cpp',
    'src/ui/widgets/ClockWidget.cpp',
    'src/ui/widgets/StatusIndicator.cpp',
    'src/util/Logger.cpp',
    'src/util/FileUtils.cpp',
)

executable('gomoku-portal-ui',
    sources: sources + resources,
    dependencies: [gtkmm_dep],
    install: true)

# Tests
gtest_dep = dependency('gtest', required: false)
if gtest_dep.found()
    subdir('tests')
endif
```

---

### 2.7 Phase Roadmap

| Phase | Scope | Deliverable |
|-------|-------|-------------|
| **P1: Engine Comms** | `EngineProcess` + `EngineProtocol` + `EngineController` + unit tests | Can spawn engine, send commands, parse responses. No UI. |
| **P2: Domain Model** | `Board` + `PortalTopology` + `Move` + `GameRecord` + unit tests | Full board model with serialization. |
| **P3: Skeleton UI** | `Application` + `MainWindow` + `BoardCanvas` (basic Cairo) + `GameController` | Can play a game against engine visually. |
| **P4: Portal Setup** | `SetupController` + `BoardSetupDialog` + portal/wall rendering in `BoardRenderer` | Full portal board creation UX. |
| **P5: Analysis** | `AnalysisController` + `AnalysisPanel` + `EvalBar` + NBest overlay on board | Professional analysis display. |
| **P6: Database** | `DatabaseController` + `DatabasePanel` + import/export | Full Yixin-style database integration. |
| **P7: Polish** | Dark theme, keyboard shortcuts, game record save/load, settings persistence | Release-ready quality. |

> [!IMPORTANT]
> **Start with Phase 1.** The engine layer is fully testable without any GTK dependency. Get this right first — everything else builds on top.

---

## Open Questions

1. **Game Record Format:** Use standard SGF (Smart Game Format) for interoperability, or a custom JSON/binary format for portal metadata?
2. **Multi-Engine:** Should the architecture support connecting to 2 engines simultaneously (e.g., engine-vs-engine analysis)?
3. **Board Size Limit:** Support boards > 20? (Engine supports up to `MAX_BOARD_SIZE` which is 22 in Rapfi.)
4. **NNUE Weight Loading:** Should the UI expose `LOADMODEL` / `EXPORTMODEL` in a dialog, or is `config.toml` sufficient?

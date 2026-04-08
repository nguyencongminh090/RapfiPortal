# Portal Gomoku Engine — Protocol Analysis & GTK4 UI Architecture Report

> Source: `portal_src/command/gomocup.cpp`, `portal_src/protocol.md`
> Repo: https://github.com/nguyencongminh090/RapfiPortal

---

## PART 1 — PROTOCOL ANALYSIS

### 1.1 Protocol Architecture Overview

Engine giao tiếp qua `stdin`/`stdout` theo **Gomocup/Piskvork protocol** có mở rộng (Yixin-Board
extensions + Portal custom extensions). Toàn bộ command dispatch nằm trong `runProtocol()` —
một loop blocking đọc từng lệnh từ `stdin`.

```
UI Process  ──stdin──►  Engine (gomocupLoop → runProtocol)
            ◄─stdout──  Engine replies (moves, MESSAGE, ERROR, OK)
```

---

### 1.2 Full Command Reference (extracted from gomocup.cpp)

#### Group A — Session Lifecycle

| Command | Handler | Engine Reply | Notes |
|---------|---------|--------------|-------|
| `START [size]` | `start()` | `OK` (via `restart()`) | Creates board. If size changes, clears portals. |
| `RESTART` | `restart()` | `OK` | Resets stones, keeps portal topology. |
| `END` | inline | *(exits)* | Stops thinking, exits process. |
| `STOP` / `YXSTOP` | inline | *(none)* | Halts search, engine outputs best move so far. |
| `ABOUT` | inline | engine info string | Version/name info. |
| `RECTSTART` | `rectStart()` | `ERROR` | Not supported. |

#### Group B — Gameplay

| Command | Handler | Engine Reply | Notes |
|---------|---------|--------------|-------|
| `BEGIN` | `begin()` | `x,y` | Engine plays first. On portal boards: picks cell adjacent to obstacle (not center). |
| `TURN x,y` | `turn()` | `x,y` | Places opponent stone, engine thinks and replies. |
| `TAKEBACK x,y` | `takeBack()` | `OK` | Undoes ONE ply. |
| `BOARD` | `getPosition(true)` | `x,y` | Load position + start thinking. Parses `x,y,color` list terminated by `DONE`. color: 1=SELF, 2=OPPO, 3=WALL |
| `YXBOARD` | `getPosition(false)` | *(none)* | Load position silently, no thinking. |
| `SWAP2BOARD` | `swap2board()` | `x,y x,y x,y` or move | Swap2 opening protocol. |

#### Group C — Portal/WALL Custom Extensions (NEW)

| Command | Handler | Engine Reply | Notes |
|---------|---------|--------------|-------|
| `INFO WALL x,y` | `getOption()` → WALL branch | *(none)* | Adds WALL. Immediately calls `applyAndReinit()`. No RESTART needed. |
| `INFO YXPORTAL Ax,Ay Bx,By` | `getOption()` → YXPORTAL branch | *(none)* | Adds portal pair. Immediately calls `applyAndReinit()`. Note: space separator between A and B, not comma. |
| `INFO CLEARPORTALS` | `getOption()` → CLEARPORTALS | *(none)* | Clears all WALLs and portals. Calls `applyAndReinit()`. |

#### Group D — INFO Configuration

| Command | Handler | Notes |
|---------|---------|-------|
| `INFO TIMEOUT_TURN ms` | `getOption()` | Per-move time limit in ms. |
| `INFO TIMEOUT_MATCH ms` | `getOption()` | Match total time in ms. |
| `INFO TIME_LEFT ms` | `getOption()` | Remaining match time. Pass `2147483647` for unlimited. |
| `INFO MAX_MEMORY kb` | `getOption()` | Engine memory limit in KB. 0 = default 350MB. |
| `INFO RULE [0/1/4/5/6]` | `getOption()` | 0=Freestyle, 1=Standard, 4=Renju, 5=Swap1, 6=Swap2. Rule change re-applies portals. |
| `INFO THREAD_NUM n` | `getOption()` | Number of search threads. |
| `INFO PONDERING 0/1` | `getOption()` | Enable/disable pondering. |
| `INFO STRENGTH 0-100` | `getOption()` | Skill level. |
| `INFO MAX_DEPTH n` | `getOption()` | Max search depth. |
| `INFO MAX_NODE n` | `getOption()` | Max node count. `ULLONG_MAX` = unlimited. |
| `INFO SHOW_DETAIL 0-3` | `getOption()` | 0=none, 1=realtime, 2=detail, 3=both. |
| `INFO CAUTION_FACTOR 0-5` | `getOption()` | Candidate range expansion. Rebuilds board object entirely. Re-applies portals. |
| `INFO HASH_SIZE kb` | `getOption()` | Yixin alias for MAX_MEMORY. |
| `INFO SEARCH_TYPE str` | `getOption()` | Switch searcher (AB / MCTS). |
| `INFO MAX_MOVES n` | `getOption()` | Max move count for game. |
| `INFO DRAW_RESULT 0/1/2` | `getOption()` | 0=draw, 1=black_win, 2=white_win. |

#### Group E — GUI & Debug

| Command | Handler | Notes |
|---------|---------|-------|
| `YXSHOWINFO` | `setGUIMode()` | **Must be first command!** Enables GUI mode. Engine prints version + `INFO MAX_THREAD_NUM` + `INFO MAX_HASH_SIZE`. |
| `YXSHOWFORBID` | `showForbid()` | Prints forbidden points (Renju). Format: `FORBID xxyy...` |
| `YXNBEST n` | `nbest()` | Multi-PV: engine returns n best moves. |
| `YXBALANCEONE bias` | `balance(ONE)` | Balance mode 1 (swap1). |
| `YXBALANCETWO bias` | `balance(TWO)` | Balance mode 2. |
| `YXBLOCK` / `YXBLOCKUNDO` | `getBlock()` | Add/remove blocked moves for engine. |
| `YXBLOCKRESET` | inline | Clear all blocked moves. |
| `TRACEBOARD` | `traceBoard()` | Dump board state (patterns, scores) as MESSAGE lines. |
| `TRACESEARCH` | `traceSearch()` | Dump search state, TT entry, static eval. |

#### Group F — Hash Table

| Command | Notes |
|---------|-------|
| `YXHASHCLEAR` | Clear transposition table. |
| `YXSHOWHASHUSAGE` | Print TT usage %. |
| `YXHASHDUMP [path]` | Dump TT to file. |
| `YXHASHLOAD [path]` | Load TT from file. |

#### Group G — Database

| Command | Notes |
|---------|-------|
| `YXSETDATABASE [path]` | Load opening database. |
| `YXSAVEDATABASE` | Flush/save database to disk. |
| `YXDBTOTXT` / `YXDBTOTXTALL` | Export DB to CSV. |
| `YXLIBTODB` | Import .lib file to DB. |
| `YXDBTOLIB` | Export DB to .lib file. |
| `YXQUERYDATABASEALL` | Query all child positions in DB for current board. |
| `YXQUERYDATABASEONE` | Query current position in DB. |
| `YXQUERYDATABASETEXT` | Query text/comment for current position. |
| `YXQUERYDATABASEALLT` | Query all + text. |
| `YXEDITTVDDATABASE mask label value depth` | Edit DB record (type/value/depth). |
| `YXEDITTEXTDATABASE "text"` | Edit DB text/comment. |
| `YXEDITLABELDATABASE x,y "text"` | Edit board label for child position. |
| `YXDELETEDATABASEONE` | Delete current position record. |
| `YXDELETEDATABASEALL [type]` | Delete child records with optional filter. |
| `YXDBSPLIT [path]` | Split database. |
| `YXDBMERGE [path]` | Merge external database. |

#### Group H — Model / Config

| Command | Notes |
|---------|-------|
| `RELOADCONFIG [path]` | Reload TOML config (leave blank for internal). |
| `LOADMODEL [path]` | Load NNUE weights. |
| `EXPORTMODEL [path]` | Export NNUE weights. |

---

### 1.3 Engine Output Formats

```
x,y                     ← Best move (single)
x,y x,y                 ← Two-move response (Swap2 / BALANCE_TWO)
SWAP                    ← Swap response
OK                      ← Command acknowledgement (RESTART, TAKEBACK)
MESSAGE ...             ← Log/info line → display in log panel, NOT as game data
ERROR ...               ← Protocol error → log, possibly show warning in UI
FORBID xxyy...          ← Forbidden points (zero-padded 2-digit each coord)
DATABASE ...            ← Database query result (multiple lines)
DATABASE DONE           ← End of DATABASE query block
```

---

### 1.4 protocol.md Review

**Overall quality:** Tốt. Document cover được các use case chính cho UI developer. Dưới đây là các điểm cần bổ sung hoặc sửa:

#### Issues cần fix trong protocol.md

**[MISSING] — `STOP` command không được document**
```
Section 5 nói "send STOP to interrupt" nhưng không có entry trong bảng lệnh Section 2.
Fix: Thêm STOP vào Group B table.
```

**[MISSING] — Output format của engine chưa được document rõ**
```
Thiếu section riêng về engine output formats:
  - Format của move reply: "x,y\n"
  - Format của MESSAGE: "MESSAGE text\n"
  - Format của FORBID: "FORBID xxyy...\n"
  - Format của DATABASE block (nhiều dòng, kết thúc bằng "DATABASE DONE")
Fix: Thêm Section 6 — Engine Output Formats.
```

**[MISLEADING] — Section 3.2 Example Pipeline thiếu `YXSHOWINFO`**
```
Protocol nói YXSHOWINFO "must" be sent first, nhưng example pipeline ở 3.2 bỏ qua bước này.
Fix: Thêm YXSHOWINFO vào đầu example pipeline.
```

**[MISSING] — Không document `BOARD` color=3 là WALL, conflict với `INFO WALL`**
```
Section 3.3 đề cập inline WALL qua BOARD color=3, nhưng không giải thích rằng:
- Inline WALLs qua BOARD được ADD vào pendingPortals.walls (không thay thế)
- Sau BOARD + inline WALLs, board sẽ có CỘNG các WALL trước đó + WALL mới
Fix: Thêm warning rõ ràng về accumulation behavior.
```

**[MISSING] — Group E, F, G, H commands hoàn toàn không được document**
```
Các lệnh như TRACEBOARD, YXHASHCLEAR, YXQUERYDATABASEALL, LOADMODEL, v.v.
hoàn toàn vắng mặt trong protocol.md.
Recommendation: Thêm Section 6 — Advanced Commands (với note "for analysis tools only").
```

**[INCOMPLETE] — YXSHOWFORBID output format chưa được giải thích**
```
Format "FORBID xxyy..." với zero-padded 2-digit coordinates khá đặc biệt.
Fix: Thêm ví dụ cụ thể.
```

---

## PART 2 — GTK4 UI ARCHITECTURE DESIGN

### 2.1 Requirements Analysis

Từ protocol analysis, UI cần hỗ trợ:

**Core gameplay:** Start/Restart, Turn, Begin, TakeBack, Board/YxBoard
**Portal board setup:** WALL placement, Portal pair placement, ClearPortals
**Analysis features:** N-best moves, Balance modes, Search tracing, TT management
**Database integration:** Query, Edit, Delete, Import/Export
**Engine management:** Multiple engines, config reload, model loading
**Real-time info:** MESSAGE log, search info (depth/score/nodes), forbidden points

---

### 2.2 Folder Structure

```
gomoku-portal-ui/
│
├── CMakeLists.txt
├── meson.build                    ← Recommended for GTK4 C++ projects
│
├── src/
│   ├── main.cpp                   ← Entry point, Application init
│   │
│   ├── app/
│   │   ├── Application.hpp/cpp    ← Gio::Application subclass, top-level lifecycle
│   │   └── AppSettings.hpp/cpp    ← GSettings wrapper (persistent user preferences)
│   │
│   ├── engine/                    ← ENGINE LAYER (no GTK dependency)
│   │   ├── EngineProcess.hpp/cpp  ← Process management: spawn, stdin/stdout pipe
│   │   ├── EngineProtocol.hpp/cpp ← Protocol parser/serializer (all commands)
│   │   ├── EngineController.hpp/cpp ← High-level API: startGame(), makeMove(), think()
│   │   ├── EngineState.hpp        ← Enum: IDLE, THINKING, WAITING, DEAD
│   │   └── EngineConfig.hpp/cpp   ← Engine config (path, rule, threads, time, etc.)
│   │
│   ├── model/                     ← DOMAIN MODEL LAYER (pure data, no UI)
│   │   ├── GameBoard.hpp/cpp      ← Board state: pieces, WALLs, portals, move history
│   │   ├── Move.hpp               ← Move value object (Pos + color + ply)
│   │   ├── GameRecord.hpp/cpp     ← Full game: metadata + move list (save/load)
│   │   ├── PortalConfig.hpp/cpp   ← Wall positions + portal pairs topology
│   │   ├── AnalysisResult.hpp     ← N-best moves, eval scores, search info
│   │   └── DatabaseRecord.hpp     ← DB query result value objects
│   │
│   ├── controller/                ← APPLICATION CONTROLLER LAYER
│   │   ├── GameController.hpp/cpp ← Orchestrates: UI events → engine commands → model update
│   │   ├── BoardSetupController.hpp/cpp ← WALL/Portal placement mode logic
│   │   ├── AnalysisController.hpp/cpp   ← N-best, balance, trace, DB interactions
│   │   └── SessionController.hpp/cpp    ← Save/load, game history, undo stack
│   │
│   ├── ui/                        ← VIEW LAYER (GTK4 widgets)
│   │   ├── MainWindow.hpp/cpp     ← Gtk::ApplicationWindow, layout container
│   │   │
│   │   ├── board/
│   │   │   ├── BoardWidget.hpp/cpp      ← Gtk::DrawingArea: renders board, stones, walls, portals
│   │   │   ├── BoardRenderer.hpp/cpp    ← Cairo/GL rendering logic (separate from widget)
│   │   │   └── BoardInputHandler.hpp/cpp ← Mouse/touch events → BoardWidget signals
│   │   │
│   │   ├── panels/
│   │   │   ├── EnginePanel.hpp/cpp      ← Engine status, config, connect/disconnect
│   │   │   ├── AnalysisPanel.hpp/cpp    ← N-best list, eval bar, search stats
│   │   │   ├── MoveListPanel.hpp/cpp    ← Scrollable move history (click to jump)
│   │   │   ├── LogPanel.hpp/cpp         ← Raw MESSAGE/ERROR log from engine
│   │   │   └── DatabasePanel.hpp/cpp    ← DB records browser/editor
│   │   │
│   │   ├── dialogs/
│   │   │   ├── NewGameDialog.hpp/cpp    ← Board size, rule, time control setup
│   │   │   ├── PortalSetupDialog.hpp/cpp ← Visual WALL/Portal placement UI
│   │   │   ├── EngineSettingsDialog.hpp/cpp ← Engine path, threads, memory
│   │   │   └── ExportDialog.hpp/cpp     ← Export game (SGF, CSV, lib)
│   │   │
│   │   └── widgets/
│   │       ├── EvalBar.hpp/cpp          ← Horizontal win-rate progress bar
│   │       ├── TimerWidget.hpp/cpp      ← Per-side clock display
│   │       └── CoordLabel.hpp/cpp       ← Board coordinate overlay
│   │
│   └── utils/
│       ├── Signal.hpp             ← Type-safe signal/slot (wraps sigc++ or custom)
│       ├── ThreadQueue.hpp        ← Thread-safe queue for engine→UI communication
│       ├── CoordConvert.hpp       ← x,y ↔ screen pixel conversion
│       └── FileUtils.hpp/cpp      ← Save/load game records, config files
│
├── resources/
│   ├── ui/                        ← Gtk::Builder .ui XML files
│   │   ├── main_window.ui
│   │   ├── new_game_dialog.ui
│   │   └── portal_setup_dialog.ui
│   ├── icons/                     ← SVG icons
│   ├── themes/                    ← CSS themes for GTK4
│   │   ├── default.css
│   │   └── dark.css
│   └── resources.gresource.xml    ← GResource manifest
│
└── tests/
    ├── test_engine_protocol.cpp   ← Unit test: parse engine output
    ├── test_game_board.cpp        ← Unit test: board logic
    └── test_portal_config.cpp     ← Unit test: portal topology
```

---

### 2.3 Object-Oriented Design

#### 2.3.1 Engine Layer — Key Classes

```cpp
// EngineProcess: manages subprocess lifetime
class EngineProcess {
public:
    bool     launch(const std::filesystem::path& execPath);
    void     send(const std::string& command);       // write to stdin
    void     setOutputCallback(std::function<void(std::string)>);  // stdout line callback
    void     terminate();
    bool     isAlive() const;
private:
    // Platform: popen / CreateProcess + async read thread
    std::thread readThread_;
    // ...
};

// EngineProtocol: STATELESS command builder + output parser
// No GTK dependency. Pure string logic.
class EngineProtocol {
public:
    // Command builders (return string to send)
    static std::string cmdStart(int boardSize);
    static std::string cmdRestart();
    static std::string cmdTurn(int x, int y);
    static std::string cmdBegin();
    static std::string cmdTakeBack(int x, int y);
    static std::string cmdEnd();
    static std::string cmdStop();
    static std::string cmdSetWall(int x, int y);
    static std::string cmdSetPortal(int ax, int ay, int bx, int by);
    static std::string cmdClearPortals();
    static std::string cmdSetRule(int rule);
    static std::string cmdSetTimeout(int turnMs, int matchMs);
    static std::string cmdSetThreads(int n);
    static std::string cmdNBest(int n);
    static std::string cmdYxShowInfo();
    static std::string cmdTraceBoard();
    // ... etc for all commands

    // Output parsers (parse one line of engine output)
    struct ParseResult {
        enum class Type { Move, Ok, Message, Error, Forbid, Database, DatabaseDone, Unknown };
        Type        type;
        int         x = -1, y = -1;   // for Move
        std::string text;              // for Message/Error/Database
    };
    static ParseResult parseLine(const std::string& line);
};

// EngineController: high-level async controller
// Emits signals that the UI layer connects to (via sigc++ or custom signal).
class EngineController {
public:
    // Lifecycle
    bool connect(const EngineConfig& config);
    void disconnect();

    // Game commands (async — reply comes via signal)
    void startGame(int boardSize, Rule rule);
    void makeMove(int x, int y);        // TURN x,y
    void requestEngineMove();           // BEGIN
    void takeBack(int x, int y);
    void setPosition(const GameRecord& record);  // BOARD

    // Portal setup
    void addWall(int x, int y);
    void addPortal(int ax, int ay, int bx, int by);
    void clearPortals();

    // Analysis
    void requestNBest(int n);
    void stopThinking();
    void traceBoard();

    // Signals (UI connects to these)
    sigc::signal<void(int x, int y)>          signalEngineMove;
    sigc::signal<void(std::string)>            signalMessage;
    sigc::signal<void(std::string)>            signalError;
    sigc::signal<void(AnalysisResult)>         signalAnalysisUpdate;
    sigc::signal<void(EngineState)>            signalStateChanged;
    sigc::signal<void()>                       signalOk;

private:
    EngineProcess    process_;
    EngineState      state_ = EngineState::DISCONNECTED;
    ThreadQueue<std::string> pendingReplies_;  // thread-safe
    void onEngineOutput(const std::string& line);  // called from read thread
};
```

#### 2.3.2 Model Layer — GameBoard

```cpp
class GameBoard {
public:
    enum class CellState { Empty, Black, White, Wall, Portal };

    void         reset(int size);
    bool         placeStone(int x, int y, Color color);
    void         undoLastMove();
    void         addWall(int x, int y);
    void         addPortal(int ax, int ay, int bx, int by);
    void         clearPortals();

    CellState    getCell(int x, int y) const;
    int          boardSize() const;
    int          ply() const;
    const Move&  getLastMove() const;
    bool         isPortal(int x, int y) const;
    std::pair<int,int> getPortalPartner(int x, int y) const;

    // Serialization
    GameRecord   toRecord() const;
    void         fromRecord(const GameRecord&);

private:
    int                          size_;
    std::vector<CellState>       cells_;
    std::vector<Move>            history_;
    PortalConfig                 portalConfig_;
};
```

#### 2.3.3 UI Layer — BoardWidget

```cpp
class BoardWidget : public Gtk::DrawingArea {
public:
    BoardWidget();

    void setModel(const GameBoard* board);
    void setHighlightMoves(const std::vector<AnalysisMove>& moves);  // N-best overlay
    void setMode(BoardMode mode);  // PLAY, WALL_PLACEMENT, PORTAL_PLACEMENT, VIEW

    // Signals emitted by widget (connected by GameController)
    sigc::signal<void(int x, int y)> signalCellClicked;
    sigc::signal<void(int x, int y)> signalCellHovered;

protected:
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) override;

private:
    const GameBoard*         model_ = nullptr;
    BoardRenderer            renderer_;
    BoardInputHandler        inputHandler_;
    std::vector<AnalysisMove> highlights_;
    BoardMode                mode_ = BoardMode::PLAY;
};
```

---

### 2.4 Design Patterns Applied

| Pattern | Where | Rationale |
|---------|-------|-----------|
| **Observer / Signal-Slot** | `EngineController` → UI panels | Decouples engine async output from UI rendering. Uses `sigc++` (native GTK). |
| **Command Pattern** | `EngineProtocol` static builders | Encapsulates each protocol command as a string-producing function. Easily testable. |
| **MVC** | `GameBoard` (M) + `BoardWidget` (V) + `GameController` (C) | Classic separation. Controller mediates all interactions. |
| **Strategy** | `BoardRenderer` as separate class | Swap rendering backends (Cairo → OpenGL) without touching BoardWidget. |
| **State Machine** | `EngineState` enum in `EngineController` | Prevents invalid commands during THINKING state (mirrors engine's own blocking). |
| **Facade** | `GameController` | Single API for UI actions → hides engine protocol + board model coordination. |
| **Repository** | `DatabasePanel` / `AnalysisController` | Abstracts DB query/edit behind clean interface. |
| **Thread-Safe Queue** | `ThreadQueue` in `EngineController` | Engine read thread pushes output; GTK main thread processes via `Glib::idle_add`. |

---

### 2.5 Threading Model

```
┌─────────────────────────────────────────────────────┐
│  GTK Main Thread (UI)                               │
│  - All widget rendering                             │
│  - Signal emission to UI                            │
│  - User interaction events                          │
│  - Glib::idle_add() → drains ThreadQueue            │
└────────────────────┬────────────────────────────────┘
                     │ ThreadQueue<string> (lock-free)
┌────────────────────▼────────────────────────────────┐
│  Engine Read Thread                                 │
│  - Blocking read from engine stdout                 │
│  - Pushes raw lines to ThreadQueue                  │
│  - NO GTK calls here (GTK is not thread-safe)       │
└─────────────────────────────────────────────────────┘
                     │ stdin (pipe)
┌────────────────────▼────────────────────────────────┐
│  Engine Process (subprocess)                        │
│  - gomocupLoop() running                            │
└─────────────────────────────────────────────────────┘
```

**Key rule:** Tuyệt đối không gọi GTK widget methods từ Engine Read Thread. Dùng `Glib::signal_idle().connect_once()` hoặc `Glib::MainContext::get_default()->invoke()` để dispatch sang main thread.

---

### 2.6 Portal Setup UX Flow

Đây là flow quan trọng nhất khác với standard Gomoku UI:

```
User clicks "Setup Board" button
    → BoardSetupController sets mode = WALL_PLACEMENT
    → BoardWidget renders cursor as "W" on hover
    → User clicks cell (x,y)
        → GameController.addWall(x,y)
            → EngineController.addWall(x,y)
                → sends "INFO WALL x,y" to engine
                → engine replies nothing (immediate applyAndReinit internally)
            → GameBoard.addWall(x,y)
            → BoardWidget.redraw()

User clicks "Add Portal" button
    → mode = PORTAL_PLACEMENT, waiting for 2 clicks
    → First click: store as portal_a
    → Second click: store as portal_b
        → GameController.addPortal(portal_a, portal_b)
            → EngineController.addPortal(...)
                → sends "INFO YXPORTAL ax,ay bx,by"
            → GameBoard.addPortal(...)
            → BoardWidget renders portal pair with matching color/icon
```

---

### 2.7 Build System Recommendation

GTK4 với C++ — dùng **Meson** thay CMake vì:
- Native support cho `gnome.compile_resources()` (GResource)
- Tích hợp tốt với `pkg-config` cho GTK4 dependencies
- Cleaner syntax cho GTK projects

```meson
project('gomoku-portal-ui', 'cpp',
  version: '1.0',
  default_options: ['cpp_std=c++20'])

gtkmm = dependency('gtkmm-4.0')
sigcpp = dependency('sigc++-3.0')

gnome = import('gnome')
resources = gnome.compile_resources('resources',
    'resources/resources.gresource.xml',
    source_dir: 'resources')

executable('gomoku-portal-ui',
    sources: ['src/main.cpp', ...] + resources,
    dependencies: [gtkmm, sigcpp])
```

---

### 2.8 Priority Build Roadmap

| Phase | Components | Goal |
|-------|-----------|------|
| **Phase 1** | `EngineProcess` + `EngineProtocol` + unit tests | Engine communication layer, fully testable without any UI |
| **Phase 2** | `GameBoard` + `PortalConfig` + `GameController` | Domain model complete |
| **Phase 3** | `BoardWidget` (basic Cairo rendering) + `MainWindow` | Can play a game visually |
| **Phase 4** | `PortalSetupDialog` + `BoardWidget` portal rendering | Full portal board setup |
| **Phase 5** | `AnalysisPanel` + N-best overlay + `LogPanel` | Analysis tool features |
| **Phase 6** | `DatabasePanel` + save/load `GameRecord` | Professional analysis persistence |

project('gomoku-portal-ui', 'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++20', 'warning_level=2'])

# Dependencies
gtkmm_dep = dependency('gtkmm-4.0', version: '>= 4.10')

# Engine layer sources (no GTK dependency)
engine_sources = files(
    'src/engine/EngineProtocol.cpp',
    'src/engine/EngineProcess.cpp',
    'src/engine/EngineController.cpp',
)

# Model layer sources (no GTK dependency)
model_sources = files(
    'src/model/Board.cpp',
    'src/model/GameRecord.cpp',
)

# Main application
app_sources = files(
    'src/main.cpp',
    'src/ui/Application.cpp',
    'src/ui/MainWindow.cpp',
    'src/ui/widgets/StatusIndicator.cpp',
    'src/ui/widgets/EvalBar.cpp',
    'src/ui/widgets/ClockWidget.cpp',
    'src/ui/widgets/CoordOverlay.cpp',
    'src/ui/panels/LogPanel.cpp',
    'src/ui/board/BoardRenderer.cpp',
    'src/ui/board/BoardCanvas.cpp',
    'src/controller/GameController.cpp',
    'src/controller/SetupController.cpp',
    'src/controller/AnalysisController.cpp',
    'src/controller/DatabaseController.cpp',
    'src/ui/panels/DatabasePanel.cpp',
    'src/ui/panels/AnalysisPanel.cpp',
    'src/ui/panels/EngineSettingsPanel.cpp',
    'src/ui/panels/UISettingsPanel.cpp',
    'src/util/SettingsManager.cpp',
)

executable('gomoku-portal-ui',
    sources: app_sources + engine_sources + model_sources,
    dependencies: [gtkmm_dep],
    install: true)

# ============================================================================
# Tests (no GTK dependency needed)
# ============================================================================

test_protocol_exe = executable('test-engine-protocol',
    sources: ['tests/test_engine_protocol.cpp', 'src/engine/EngineProtocol.cpp'],
    include_directories: include_directories('src'),
    install: false)

test('engine-protocol', test_protocol_exe)

test_board_exe = executable('test-board',
    sources: ['tests/test_board.cpp'] + model_sources,
    include_directories: include_directories('src'),
    install: false)

test('board-model', test_board_exe)

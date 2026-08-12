cmake_minimum_required(VERSION 3.20)
project(gameshub VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# AUTORCC compiles sounds.qrc into whichever target lists it as a source.
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup()


# Every game's rules live in a Qt-free core plus a QWidget view. The core files
# are listed separately so the headless self-test can link them without Qt
# Widgets.
set(GAME_CORE_SOURCES
    src/reversi/board.cpp
    src/reversi/ai.cpp
    src/minesweeper/minefield.cpp
    src/draughts/draughtsboard.cpp
    src/chess/chessboard.cpp
    src/chess/chessai.cpp
    src/sudoku/sudokugrid.cpp
    src/cards/card.cpp
    src/cards/cardcodec.cpp
    src/hearts/heartsengine.cpp
    src/canasta/canastaengine.cpp
    src/canasta/canastaai.cpp
    src/pinball/pinballtable.cpp
)

set(GAME_VIEW_SOURCES
    src/gameview.cpp
    src/hubwindow.cpp
    src/reversi/reversiview.cpp
    src/minesweeper/minesweeperview.cpp
    src/theme.cpp
    src/sound.cpp
    src/scores.cpp
    src/cards/cardart.cpp
    src/chess/chessart.cpp
    src/chess/chessview.cpp
    src/klondike/klondikeview.cpp
    src/freecell/freecellview.cpp
    src/pyramid/pyramidview.cpp
    src/draughts/draughtsview.cpp
    src/sudoku/sudokuview.cpp
    src/snake/snakeview.cpp
    src/twenty48/twenty48view.cpp
    src/spider/spiderview.cpp
    src/hearts/heartsview.cpp
    src/canasta/canastaview.cpp
    src/pinball/pinballview.cpp
)

qt_add_executable(gameshub
    src/main.cpp
    sounds.qrc
    ${GAME_CORE_SOURCES}
    ${GAME_VIEW_SOURCES}
)
target_include_directories(gameshub PRIVATE src)
target_compile_definitions(gameshub PRIVATE GAMESHUB_VERSION="${PROJECT_VERSION}")
target_link_libraries(gameshub PRIVATE Qt6::Widgets Qt6::Multimedia)

# Console harness for every game's rules. No UI dependency, so it needs no
# display and runs in milliseconds.
add_executable(gameshub_selftest
    tests/selftest.cpp
    ${GAME_CORE_SOURCES}
)
target_include_directories(gameshub_selftest PRIVATE src)
target_link_libraries(gameshub_selftest PRIVATE Qt6::Core)

# Drives real game widgets offscreen to check they build, paint and respond.
qt_add_executable(gameshub_uitest
    tests/uitest.cpp
    sounds.qrc
    ${GAME_CORE_SOURCES}
    ${GAME_VIEW_SOURCES}
)
target_include_directories(gameshub_uitest PRIVATE src)
target_compile_definitions(gameshub_uitest PRIVATE GAMESHUB_VERSION="${PROJECT_VERSION}")
target_link_libraries(gameshub_uitest PRIVATE Qt6::Widgets Qt6::Multimedia)

enable_testing()
add_test(NAME selftest COMMAND gameshub_selftest)
add_test(NAME uitest COMMAND gameshub_uitest)
set_tests_properties(uitest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ---------------------------------------------------------------------------
# Desktop integration. `cmake --install build` puts the hub in the application
# menu; the .desktop file needs the absolute Exec path because ~/.local/bin is
# not always on the desktop session's PATH.
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

configure_file(packaging/gameshub.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/gameshub.desktop @ONLY)

install(TARGETS gameshub RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gameshub.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES packaging/gameshub.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

# Licence texts travel with the binary. A source checkout has LICENSE at its
# root, but a downloaded AppImage or zip carries no repository to read it
# from, and bundling Qt makes the LGPL's copy-of-the-licence condition apply.
# The licenses/ subdirectory is asserted by the release workflow (GHUB-0025
# INV-8) — installing these flat here would fail that check.
install(FILES LICENSE packaging/THIRD-PARTY.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES packaging/licenses/LGPL-3.0.txt packaging/licenses/GPL-3.0.txt
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/licenses)

cmake_minimum_required(VERSION 3.20)
project(gameshub VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# AUTORCC compiles sounds.qrc into whichever target lists it as a source.
set(CMAKE_AUTORCC ON)

# Build accelerators, used when the machine has them and silently skipped when
# it does not -- so a CI runner, a Windows box and a fresh clone all configure
# exactly as before. Neither changes a byte of output: ccache replays a
# compilation it has already done, and mold emits the same program GNU ld does.
#
# ccache earns most of its keep here for a reason peculiar to this project:
# every core and view source is compiled once PER TARGET, and there are three
# targets. Measured 2026-09-04 on this tree: 118 objects for about 60 distinct
# sources, so roughly half of a full build is the same work done twice. The
# second and third compilations are cache hits.
#
# Turn either off with -DGAMESHUB_FAST_BUILD=OFF, which is the switch to reach
# for when a build behaves oddly and you want the plainest possible toolchain.
option(GAMESHUB_FAST_BUILD "Use ccache and mold when they are installed" ON)
if(GAMESHUB_FAST_BUILD)
    find_program(GAMESHUB_CCACHE ccache)
    if(GAMESHUB_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${GAMESHUB_CCACHE}")
        message(STATUS "gameshub: compiling through ccache (${GAMESHUB_CCACHE})")
    endif()

    # -fuse-ld= rather than CMAKE_LINKER_TYPE, which arrived in CMake 3.29 and
    # this project asks for 3.20. GCC and Clang only: MSVC has no such flag and
    # would fail obscurely rather than ignoring it.
    if(NOT MSVC)
        find_program(GAMESHUB_MOLD mold)
        if(GAMESHUB_MOLD)
            add_link_options(-fuse-ld=mold)
            message(STATUS "gameshub: linking with mold (${GAMESHUB_MOLD})")
        endif()
    endif()
endif()

# GHUB-0052. The saved-game fuzzer proves very little without these: an
# out-of-bounds read that happens to land in owned memory returns a wrong
# answer quietly and the harness scores it a pass, where ASan aborts on the
# spot. Off by default because it makes the binary several times slower and is
# not what a player runs; a separate build directory is the intended use.
#
#   cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
#         -DGAMESHUB_SANITIZE=ON
#   cmake --build build-asan
#   QT_QPA_PLATFORM=offscreen ./build-asan/gameshub_uitest --fuzz 2000
#
# MSVC has no UBSan and its ASan does not accept these flags, so this is a
# GCC/Clang option and says so rather than failing obscurely on Windows.
option(GAMESHUB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(GAMESHUB_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "GAMESHUB_SANITIZE is a GCC/Clang option; MSVC ships no UBSan.")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia Concurrent)
qt_standard_project_setup()


# The donate links live in .github/FUNDING.yml and nowhere else. That file is
# already GitHub's sponsor button, so it is the copy that is guaranteed to be
# maintained; a list of URLs in a .cpp would be a second copy that drifts the
# day one of them changes, with nothing to catch it. So the header is generated
# from the YAML at configure time, and a key this loop has no rule for is a
# hard error rather than a silently dropped link — which is the whole point,
# since a dropped link looks exactly like a working build.
set(FUNDING_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.github/FUNDING.yml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FUNDING_FILE}")
file(STRINGS "${FUNDING_FILE}" funding_lines)
set(funding_entries "")
foreach(funding_line IN LISTS funding_lines)
    string(STRIP "${funding_line}" funding_line)
    if(funding_line STREQUAL "" OR funding_line MATCHES "^#")
        continue()
    endif()
    # The handle-to-URL mapping is GitHub's, not this project's: FUNDING.yml
    # stores account names for the platforms it knows and a full URL only for
    # `custom`. That mapping has to live somewhere, and here is the one place
    # it is not a copy of a link.
    if(funding_line MATCHES "^github:[ \t]*\\[?([A-Za-z0-9._-]+)\\]?$")
        list(APPEND funding_entries
             "    { \"GitHub Sponsors\", \"https://github.com/sponsors/${CMAKE_MATCH_1}\" },")
    elseif(funding_line MATCHES "^patreon:[ \t]*\\[?([A-Za-z0-9._-]+)\\]?$")
        list(APPEND funding_entries
             "    { \"Patreon\", \"https://www.patreon.com/${CMAKE_MATCH_1}\" },")
    elseif(funding_line MATCHES "^custom:[ \t]*\\[?\"?(https://[^\"]+)\"?\\]?$")
        list(APPEND funding_entries
             "    { \"Tip page\", \"${CMAKE_MATCH_1}\" },")
    else()
        message(FATAL_ERROR
                "CMakeLists.txt cannot turn this line of .github/FUNDING.yml into a "
                "donate entry, so the app would quietly offer fewer links than the "
                "repository declares: \"${funding_line}\". Add a rule for it here.")
    endif()
endforeach()
if(funding_entries STREQUAL "")
    message(FATAL_ERROR ".github/FUNDING.yml declares no funding links this build can use.")
endif()
string(JOIN "\n" GAMESHUB_FUNDING_ENTRIES ${funding_entries})
configure_file(src/funding.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/funding.h" @ONLY)


# Every game's rules live in a Qt-free core plus a QWidget view. The core files
# are listed separately so the headless self-test can link them without Qt
# Widgets.
set(GAME_CORE_SOURCES
    # Where a deal, a board or a computer seat gets its randomness, so --seed
    # can pin the lot from one place (GHUB-0093). QtCore-only and it reads no
    # stored state, which is what keeps it on this side of the split.
    src/dealseed.cpp
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
    src/snake/snakeboard.cpp
    src/twenty48/twenty48board.cpp
    src/pyramid/pyramidtable.cpp
    src/freecell/freecelltable.cpp
    src/klondike/klondiketable.cpp
    src/spider/spidertable.cpp
)

set(GAME_VIEW_SOURCES
    src/gameview.cpp
    src/hubwindow.cpp
    src/reversi/reversiview.cpp
    src/minesweeper/minesweeperview.cpp
    src/theme.cpp
    src/sound.cpp
    # QtCore-only today, but a rules core has no business reading a display
    # preference — so it links into the view half, not the self-test's.
    src/legibility.cpp
    src/donate.cpp
    src/donatedialog.cpp
    src/scores.cpp
    src/cards/cardart.cpp
    src/cards/cardflight.cpp
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
target_include_directories(gameshub PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(gameshub PRIVATE GAMESHUB_VERSION="${PROJECT_VERSION}")
target_link_libraries(gameshub PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Concurrent)

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
target_include_directories(gameshub_uitest PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
# The UI test reads .github/FUNDING.yml back and counts it against the links the
# build generated, so it needs to know where the source tree is. TO_CMAKE_PATH
# keeps the separators forward-slashed, which a C string literal survives on
# Windows and a native path does not.
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" GAMESHUB_SOURCE_DIR_FWD)
target_compile_definitions(gameshub_uitest PRIVATE GAMESHUB_VERSION="${PROJECT_VERSION}"
                                                   GAMESHUB_SOURCE_DIR="${GAMESHUB_SOURCE_DIR_FWD}")
target_link_libraries(gameshub_uitest PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Concurrent)

enable_testing()
add_test(NAME selftest COMMAND gameshub_selftest)
add_test(NAME uitest COMMAND gameshub_uitest)
set_tests_properties(uitest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --shot is how a layout question gets answered by looking rather than by
# arithmetic, so it has to keep working. Run against the real binary because
# that is the whole point of it: no display, no compositor, no injection tool.
# A malformed size must be refused rather than quietly photographed at some
# other size, so the second run asserts a NON-zero status.
add_test(NAME shot COMMAND gameshub --shot ${CMAKE_CURRENT_BINARY_DIR}/shot-test.png
                                    --game hearts --size 900x600 --legible)
add_test(NAME shot_refuses_a_bad_size
         COMMAND gameshub --shot ${CMAKE_CURRENT_BINARY_DIR}/shot-never.png
                          --game hearts --size not-a-size)
# --turns is what reaches a layout that only exists once a hand has been played,
# and --seed is what makes two shots of one card game comparable (GHUB-0093,
# GHUB-0166). The pair is run together because that is how they are used. A turn
# count aimed at a game that cannot play itself must be REFUSED rather than
# quietly photographed at the deal, which is the same failure --size guards
# against: the picture still gets written and still looks like an answer.
add_test(NAME shot_plays_forward
         COMMAND gameshub --shot ${CMAKE_CURRENT_BINARY_DIR}/shot-played.png
                          --game canasta --size 1200x700 --seed 3 --turns 30)
add_test(NAME shot_refuses_turns_it_cannot_play
         COMMAND gameshub --shot ${CMAKE_CURRENT_BINARY_DIR}/shot-never.png
                          --game sudoku --turns 5)
set_tests_properties(shot shot_refuses_a_bad_size shot_plays_forward
                     shot_refuses_turns_it_cannot_play
                     PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(shot_refuses_a_bad_size shot_refuses_turns_it_cannot_play
                     PROPERTIES WILL_FAIL TRUE)

# The pre-push hook decides between the linters and the full pipeline, and a
# wrong decision is silent — the push succeeds either way. Unix only: the hook
# is bash, and nothing on Windows installs it.
#
# An absent checker registers a FAILING test rather than no test at all. A
# configure without bash or python used to drop the test silently, and ctest
# then reported 100% passed over a smaller suite -- which is exactly the
# "silently skipped step" scripts/local-ci.sh refuses by name. A red test says
# which tool is missing; a green run over a shorter suite says nothing at all.
# Deliberately no figures here: the count moves every time a case is added, and
# a stale one sends somebody hunting for a case that was never registered on
# their platform. `ctest -N` is how you count them.
if(UNIX)
    find_program(BASH_PROGRAM bash)
    if(BASH_PROGRAM)
        add_test(NAME prepush
                 COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tests/pre-push-test.sh)
    else()
        add_test(NAME prepush_needs_bash COMMAND ${CMAKE_COMMAND} -E false)
    endif()
endif()

# The score book in scorepad/ is a phone replacement for the paper book kept at
# a real table, so it does NOT reimplement the scoring — the players work each
# hand out themselves. It does have to SHOW the opening minimum each side
# needs, and those are the game's own numbers, which puts them in two places.
# That is the drift this project refuses elsewhere (the donate URLs are
# generated from FUNDING.yml for the same reason), so a script reads both and
# fails when they disagree. Not guarded on UNIX: the check is pure Python and
# the numbers can drift on either platform.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME scorepad
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/scripts/scorepad-check.py)

    # GHUB-0017 names --thresholds as INV-4's test, and nothing ran it: it
    # passed only when somebody thought to run it by hand. It asserts that
    # CardArt::kFaceMinWidth is the only statement of the 46-pixel threshold,
    # which is the sort of thing a second copy breaks quietly.
    add_test(NAME legibility
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/scripts/legibility-check.py --thresholds)
elseif(UNIX)
    # Only on Linux. Both of these are pure Python and platform-independent, so
    # they have to run SOMEWHERE rather than everywhere, and Linux is where the
    # pipeline runs them. A Linux configure with no python has lost a check and
    # should say so; a Windows one has not, and reddening the Windows leg over a
    # check the Linux leg already ran would be an alarm with nothing behind it.
    add_test(NAME scorepad_needs_python COMMAND ${CMAKE_COMMAND} -E false)
    add_test(NAME legibility_needs_python COMMAND ${CMAKE_COMMAND} -E false)
else()
    # Windows with no interpreter CMake will use. Said out loud, because the
    # alternative is a silent one: the owner's Windows box has only the Store
    # stub python.exe, so it registers strictly fewer cases than the GitHub
    # runner -- and a green run there was read as full coverage while the runner
    # was failing on one of the two checks this branch drops.
    message(STATUS
            "gameshub: no Python3 interpreter -- the scorepad and legibility "
            "checks are NOT registered; ctest here runs fewer tests than CI")
endif()

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

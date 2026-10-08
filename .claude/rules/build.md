---
paths:
  - "CMakeLists.txt"
  - "cmake/**"
  - ".github/workflows/**"
---

## Building

Moved from `CLAUDE.md` § Commands.

```bash
# Warnings are on for every build (-Wall -Wextra, /W4 on MSVC) and are fatal
# only under -DGAMESHUB_WERROR=ON, which CI sets on the LINUX leg alone --
# MSVC's /W4 is a different set and GHUB-0185 owns measuring it. Turn it on
# locally before pushing code, or the Linux leg is where you find out.
# Release builds also harden: stack protector, FORTIFY_SOURCE, full RELRO,
# non-executable stack and PIE. Neither switch changes what the code does.
# `readelf -h build/gameshub` saying DYN is the only thing that proves PIE
# landed -- CMAKE_POSITION_INDEPENDENT_CODE alone compiles -fPIE and links no
# -pie, and every other hardening check still passes while it is missing.
# GAMESHUB_SANITIZE turns the hardening off, deliberately: ASan instruments the
# same paths and the two then report each other.

# Every source is compiled ONCE, into gameshub_core or gameshub_views, and the
# objects are linked into all three executables. Until GHUB-0187 the three
# targets listed GAME_CORE_SOURCES and GAME_VIEW_SOURCES directly, so a core
# file went to the compiler three times and a view file twice. Measured on this
# machine with ccache and mold off, which is how CI builds: a cold build fell
# from about 58 s to about 29 s, and 126 build steps to 65.
#
# The object libraries are not a faster compiler. They are the same work done
# fewer times, so nothing about the output moves -- and the hardening still
# lands, which is checked on the binary rather than assumed: readelf says DYN,
# BIND_NOW and a non-executable stack.
#
# They also turn the core/view split from a convention into something the
# compiler enforces. gameshub_core links Qt6::Core alone, so a rules core that
# includes a widget no longer builds -- verified by adding one and watching it
# fail. Before, only gameshub_selftest's link line said so, about its own copy
# of the file.
#
# The configure step picks up ccache and mold when they are installed and says
# so; -DGAMESHUB_FAST_BUILD=OFF turns both off. Neither changes what is built.
# ccache replays compilations it has already done, so a repeated cold build is
# a couple of seconds whatever the object count; the figures above are what a
# machine that has never built this pays, which is every CI run.
#
# Two things worth knowing. CI has neither ccache nor mold, so a runner builds
# the plain way and the shipped artifacts are linked by GNU ld -- if a local
# build is green and CI is not, the toolchain is one of the differences. And
# the peak is a single compiler process at roughly 660 MB, unchanged by the
# above, so -j on a machine short of memory is worth setting by hand: ninja
# defaults to cores plus two.
```

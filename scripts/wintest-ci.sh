#!/usr/bin/env bash
# Build and test the WINDOWS leg of ci.yml on the wintest box, over SSH.
#
# Why this exists. scripts/local-ci.sh says on every run that it cannot drive
# MSVC, so the Windows half of a push is verified by GitHub and nowhere else --
# three minutes after the fact, and this project has already spent three red
# Windows runs learning that. GHUB-0091 put a real toolchain on the owner's
# spare Windows box; this is the command that uses it.
#
# WHAT IT DOES NOT DO, and assuming otherwise is the expensive mistake.
# wintest is a real desktop with Segoe UI installed. ci.yml runs the same
# binaries under QT_QPA_PLATFORM=offscreen on windows-2022, where
# QFontDatabase::families() is EMPTY and the default face measures digits at
# 0.997 of an em -- the full em box, a headless stub rather than any typeface.
# Anything derived from font metrics can therefore pass here and fail there.
# This buys "what a Windows player sees", which is a real question for a game
# read by its pip patterns. It does not buy "what the runner does", and a green
# run here is not a reason to skip watching CI.
#
# It tests a COMMIT, not your working tree: git archive takes $REF (default
# HEAD), which is what a push would send. Uncommitted edits are not included,
# deliberately -- the question this answers is "is what I am about to push
# going to survive the Windows leg?".
set -euo pipefail

HOST=${WINTEST_HOST:-wintest}
DEST=${WINTEST_DIR:-C:/gameshub}

# Validated, because the remote prep below does a RECURSIVE DELETE inside it.
# Unset is safe by construction; a mistyped or hostile value was not checked at
# all, and a drive ROOT here would take the whole disk with it. The pattern
# insists on a drive letter plus at least one directory, and refuses quotes and
# shell metacharacters, which would otherwise close the PowerShell string this
# is pasted into.
case "$DEST" in
  *\'*|*\"*|*'$'*|*'`'*|*';'*|*'&'*|*'|'*)
      echo "WINTEST_DIR must not contain quotes or shell metacharacters: '$DEST'" >&2
      exit 2 ;;
esac
if ! printf '%s' "$DEST" | grep -qE '^[A-Za-z]:/[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)*$'; then
    echo "WINTEST_DIR must look like C:/some/directory, and never a drive root: '$DEST'" >&2
    exit 2
fi
REF=${1:-HEAD}

say() { printf '\033[1m%s\033[0m\n' "$*"; }

cd "$(git rev-parse --show-toplevel)"

if ! git rev-parse --verify --quiet "$REF^{commit}" >/dev/null; then
    echo "not a commit: $REF" >&2
    exit 1
fi
sha=$(git rev-parse --short "$REF")

say "== Reaching $HOST"
# Probe with PowerShell rather than `cmd /c ver`: it is what the build script
# is run by, so this proves the thing actually needed. And `cmd /c ver` does
# not survive the trip — Windows sshd hands the command to cmd.exe with its own
# quoting, which turns the last word into `ver"` and fails on a box that is up.
if ! ssh -o BatchMode=yes -o ConnectTimeout=15 "$HOST" 'powershell -NoProfile -Command exit 0' >/dev/null 2>&1; then
    cat >&2 <<EOF
cannot reach '$HOST' over ssh.

This script needs the Windows box GHUB-0091 set up. Set WINTEST_HOST if it
answers to another name. It is a pre-push convenience, not a gate: if the box
is off, push and let CI run the Windows leg.
EOF
    exit 1
fi

say "== Shipping $REF ($sha) to $HOST:$DEST"
# Everything except build/ is replaced, so a file deleted in this commit does
# not linger from the last run and quietly keep compiling. build/ survives on
# purpose -- a from-scratch Windows build is minutes, an incremental one seconds.
# One line, deliberately. The remote command line is handed to cmd.exe, which
# cannot take a multi-line command — split this across lines and the directory
# is silently not created, leaving tar to fail with "could not chdir".
prep="New-Item -ItemType Directory -Force -Path '$DEST' | Out-Null; Get-ChildItem -LiteralPath '$DEST' -Force | Where-Object { \$_.Name -ne 'build' } | Remove-Item -Recurse -Force"
ssh -o BatchMode=yes "$HOST" "powershell -NoProfile -Command \"$prep\""
git archive --format=tar "$REF" | ssh -o BatchMode=yes "$HOST" "tar -xf - -C $DEST"

say "== Building and testing on $HOST"
# The .ps1 hands -Src straight to cd /d, which wants backslashes.
win_dest=${DEST//\//\\}
# DOUBLE quotes around the path, not single. The remote command line is parsed
# by cmd.exe, which strips double quotes and leaves single ones alone -- and
# powershell -File then takes its arguments literally, so a single-quoted path
# arrives complete with the quotes and every Test-Path on it fails.
# -tt so ctest's progress arrives as it happens rather than in one lump at the
# end; a five-minute silence reads as a hang.
ssh -tt -o BatchMode=yes -o ServerAliveInterval=30 "$HOST" \
    "powershell -NoProfile -ExecutionPolicy Bypass -File $DEST/scripts/wintest-build.ps1 -Src \"$win_dest\"" |
    tr -d '\r'

#!/usr/bin/env bash
# Run once in every clone: scripts/setup-hooks.sh
#
# Points git at .githooks/, whose pre-push hands every push to the machine-wide
# gate (~/.claude/githooks/pre-push). git keeps this setting in .git/config and
# no clone inherits it, so without this run no hook in .githooks/ fires.
#
# The gate's own settings are committed in .ants/gate.conf and need nothing
# here.
set -Eeuo pipefail
cd "$(dirname "$0")/.."

git config core.hooksPath .githooks
echo "core.hooksPath = $(git config core.hooksPath)"

#!/usr/bin/env bash
set -euo pipefail

# Ensure we are inside a git repository
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: Not a git repository." >&2
    exit 1
fi

echo "==> Fetching and rebasing current branch..."
git pull --rebase

echo "==> Initializing, fetching, and updating submodules recursively..."
git submodule update --init --recursive

echo "==> Done."

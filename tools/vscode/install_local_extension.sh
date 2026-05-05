#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXT_SRC="$ROOT_DIR/tools/vscode"

resolve_extension_root() {
    local candidates=(
        "$HOME/.vscode-server/extensions"
        "$HOME/.vscode-server-insiders/extensions"
        "$HOME/.cursor-server/extensions"
        "$HOME/.windsurf-server/extensions"
    )

    local candidate
    for candidate in "${candidates[@]}"; do
        if [[ -d "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    printf '%s\n' "$HOME/.vscode-server/extensions"
}

if [[ ! -f "$EXT_SRC/package.json" ]]; then
    echo "Cannot find extension source at $EXT_SRC" >&2
    exit 1
fi

EXT_ROOT="$(resolve_extension_root)"
TARGET_LINK="$EXT_ROOT/local.v6pp-debugger-ui-dev"

mkdir -p "$EXT_ROOT"
rm -rf "$TARGET_LINK"
ln -s "$EXT_SRC" "$TARGET_LINK"

cat <<EOF
Installed V6++ debugger frontend into:
  $TARGET_LINK

Next steps:
  1. In VS Code, run: Developer: Reload Window
  2. In the repo root terminal, run:
       make deploy-full-debug
       make qemug-boot-no-rebuild
  3. Press F5 and choose "V6++ Lifecycle Attach"

The extension is linked to the repository source, so future edits only need a window reload.
EOF

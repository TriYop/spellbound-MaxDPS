#!/usr/bin/env bash
# Remove Bastos from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling Bastos..."

remove "${HOME}/.vst3/Bastos.vst3"
remove "${HOME}/.clap/Bastos.clap"
remove "${HOME}/.lv2/Bastos.lv2"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Bastos.vst3"
    remove "/usr/lib/clap/Bastos.clap"
    remove "/usr/lib/lv2/Bastos.lv2"
else
    for path in "/usr/lib/vst3/Bastos.vst3" \
                "/usr/lib/clap/Bastos.clap" \
                "/usr/lib/lv2/Bastos.lv2"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi

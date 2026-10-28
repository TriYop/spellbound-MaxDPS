#!/usr/bin/env bash
# Install Havoc plugins (VST3/CLAP/LV2). Havoc is an effect -- no standalone app.
# Usage:
#   ./install.sh           — install to user directories (no root needed)
#   ./install.sh --system  — install system-wide to /usr/lib (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    LV2_DIR="/usr/lib/lv2"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    LV2_DIR="${HOME}/.lv2"
fi

echo "Installing Havoc..."

# Legacy (pre-rename) Bastos: remove old copies (no backups) and migrate presets.
rm -rf "${VST3_DIR}/Bastos.vst3" "${CLAP_DIR}/Bastos.clap" "${LV2_DIR}/Bastos.lv2"
OLD_CFG="${HOME}/.config/Bastos"
NEW_CFG="${HOME}/.config/Havoc"
if [[ -d "${OLD_CFG}" && ! -e "${NEW_CFG}" ]]; then
    mv "${OLD_CFG}" "${NEW_CFG}"
    echo "  Migrated ${OLD_CFG} -> ${NEW_CFG}"
fi

mkdir -p "${VST3_DIR}"
rm -rf   "${VST3_DIR}/Havoc.vst3"
cp -r    "${SCRIPT_DIR}/VST3/Havoc.vst3" "${VST3_DIR}/"
echo "  VST3  → ${VST3_DIR}/Havoc.vst3"

mkdir -p "${CLAP_DIR}"
cp       "${SCRIPT_DIR}/CLAP/Havoc.clap" "${CLAP_DIR}/"
chmod    755 "${CLAP_DIR}/Havoc.clap"
echo "  CLAP  → ${CLAP_DIR}/Havoc.clap"

mkdir -p "${LV2_DIR}"
rm -rf   "${LV2_DIR}/Havoc.lv2"
cp -r    "${SCRIPT_DIR}/LV2/Havoc.lv2" "${LV2_DIR}/"
echo "  LV2   → ${LV2_DIR}/Havoc.lv2"

echo "Done."

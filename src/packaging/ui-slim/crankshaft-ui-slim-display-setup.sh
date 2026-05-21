#!/bin/sh
# Project: Crankshaft
# This file is part of Crankshaft project.
# Copyright (C) 2025 OpenCarDev Team
#
#  Crankshaft is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  Crankshaft is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Crankshaft. If not, see <http://www.gnu.org/licenses/>.

set -eu

RUN_DIR="/run/crankshaft"
ENV_FILE="${RUN_DIR}/ui-slim-display.env"
PREFERENCES_ROOT="/var/lib/crankshaft/slim-ui/.local/share"

read_rotation_preference() {
    preference_file=$(find "${PREFERENCES_ROOT}" \( -name 'slim-ui-preferences.db' -o -name 'slim-ui-preferences.ini' \) -print -quit 2>/dev/null || true)

    if [ -z "${preference_file}" ] || [ ! -f "${preference_file}" ]; then
        echo 0
        return
    fi

    rotation_value=$(awk -F= '$1 == "slim_ui.display.rotation" { print $2; exit }' "${preference_file}" 2>/dev/null || true)

    case "${rotation_value}" in
        0|90|180|270)
            echo "${rotation_value}"
            ;;
        *)
            echo 0
            ;;
    esac
}

mkdir -p "${RUN_DIR}"

has_connected_display=0
for status_file in /sys/class/drm/card*-*/status; do
    if [ ! -f "${status_file}" ]; then
        continue
    fi

    connector_dir=$(dirname "${status_file}")
    connector_name=$(basename "${connector_dir}")

    case "${connector_name}" in
        *Writeback*|*writeback*)
            continue
            ;;
    esac

    status_value=$(cat "${status_file}" 2>/dev/null || true)
    if [ "${status_value}" = "connected" ]; then
        has_connected_display=1
        break
    fi
done

if [ "${has_connected_display}" -eq 1 ]; then
    display_rotation=$(read_rotation_preference)

    cat > "${ENV_FILE}" << 'EOF'
QT_QPA_PLATFORM=eglfs
QT_QPA_EGLFS_INTEGRATION=eglfs_kms
QT_QPA_EGLFS_ALWAYS_SET_MODE=1
SLIM_UI_DISPLAY_MODE=physical
EOF
    printf 'QT_QPA_EGLFS_ROTATION=%s\n' "${display_rotation}" >> "${ENV_FILE}"
    echo "[crankshaft-ui-slim-display-setup] display detected, selecting physical mode (eglfs)"
else
    cat > "${ENV_FILE}" << 'EOF'
QT_QPA_PLATFORM=vnc:size=1280x720:port=5900
SLIM_UI_DISPLAY_MODE=vnc
EOF
    echo "[crankshaft-ui-slim-display-setup] no display detected, selecting VNC mode"
fi

chmod 0644 "${ENV_FILE}"

exit 0

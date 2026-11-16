#!/usr/bin/env bash
# Reproducible compilation only. No devices, flash, or monitor commands.
set -euo pipefail
profile="${1:-headless}"
case "$profile" in headless|rev12) ;; *) echo 'Usage: build_idf.sh [headless|rev12]' >&2; exit 2 ;; esac
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
image='espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a'
defaults='/project/firmware/esp32-hmi/sdkconfig.defaults'
if [[ "$profile" == rev12 ]]; then defaults+=';/project/firmware/esp32-hmi/sdkconfig.rev12'; fi
docker run --rm --cap-drop=ALL --security-opt=no-new-privileges \
  -v "$project_root:/project" -w /project/firmware/esp32-hmi "$image" \
  idf.py -B "/project/build-idf-$profile" \
  "-DSDKCONFIG=/project/build-idf-$profile/sdkconfig" \
  "-DSDKCONFIG_DEFAULTS=$defaults" build

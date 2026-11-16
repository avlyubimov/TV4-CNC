#!/usr/bin/env bash
# Compilation only: no devices, flash, monitor or serial access.
set -euo pipefail
profile="${1:-headless}"
version="${2:-6.1.1}"
mode="${3:-docker}"
case "$profile" in headless|rev12) ;; *) echo 'Profile: headless or rev12' >&2; exit 2 ;; esac
case "$version" in
  5.3.2) image='espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a' ;;
  6.1.1) image='espressif/idf@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91' ;;
  *) echo 'Validated SDKs: 5.3.2 or 6.1.1' >&2; exit 2 ;;
esac
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "$mode" == docker ]]; then
  # Re-enter in the official immutable SDK container. No device passthrough.
  exec docker run --rm --cap-drop=ALL --security-opt=no-new-privileges \
    -v "$project_root:/project" -w /project "$image" \
    bash /project/tools/build_idf.sh "$profile" "$version" local
elif [[ "$mode" != local ]]; then
  echo 'Mode: docker or local' >&2; exit 2
fi
sdk_path="${TV4_IDF_PATH:-${IDF_PATH:-$HOME/.espressif/v$version/esp-idf}}"
if [[ ! -f "$sdk_path/export.sh" ]]; then echo "Missing ESP-IDF: $sdk_path" >&2; exit 2; fi
# Espressif's activation script does not promise nounset compatibility.
set +u
source "$sdk_path/export.sh"
set -u
actual="$(idf.py --version)"
if [[ "$actual" != "ESP-IDF v$version" ]]; then echo "Wrong SDK: $actual (expected v$version)" >&2; exit 2; fi
build="$project_root/build-idf-$version-$profile"
defaults="$project_root/firmware/esp32-hmi/sdkconfig.defaults"
if [[ "$profile" == rev12 ]]; then defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.rev12"; fi
idf.py -C "$project_root/firmware/esp32-hmi" -B "$build" \
  -DIDF_TARGET=esp32s3 "-DSDKCONFIG=$build/sdkconfig" "-DSDKCONFIG_DEFAULTS=$defaults" build
python "$project_root/tools/check_idf_build.py" "$build" "$profile" "$version"

#!/usr/bin/env bash
# Compilation only: no devices, flash, monitor or serial access.
set -euo pipefail
profile="${1:-headless}"
version="${2:-6.1.1}"
mode="${3:-docker}"
memory_diagnostics="${TV4_OTA_MEMORY_DIAGNOSTICS:-0}"
worker_stack_bench="${TV4_OTA_WORKER_STACK_BENCH:-0}"
worker_tls_bench="${TV4_OTA_WORKER_TLS_BENCH:-0}"
signed_check_bench="${TV4_OTA_SIGNED_CHECK_BENCH:-0}"
reproducible="${TV4_REPRODUCIBLE_BUILD:-0}"
case "$reproducible" in 0|1) ;; *) echo 'TV4_REPRODUCIBLE_BUILD must be 0 or 1' >&2; exit 2 ;; esac
if [[ "$reproducible" == 1 && "$profile" != rev12-ota ]]; then
  echo 'Reproducible release builds require explicit rev12-ota' >&2; exit 2
fi
case "$memory_diagnostics" in 0|1) ;; *) echo 'TV4_OTA_MEMORY_DIAGNOSTICS must be 0 or 1' >&2; exit 2 ;; esac
case "$worker_stack_bench" in 0|1) ;; *) echo 'TV4_OTA_WORKER_STACK_BENCH must be 0 or 1' >&2; exit 2 ;; esac
case "$worker_tls_bench" in 0|1) ;; *) echo 'TV4_OTA_WORKER_TLS_BENCH must be 0 or 1' >&2; exit 2 ;; esac
case "$signed_check_bench" in 0|1) ;; *) echo 'TV4_OTA_SIGNED_CHECK_BENCH must be 0 or 1' >&2; exit 2 ;; esac
if [[ "$signed_check_bench" == 1 && ( "$profile" != rev12-ota || "$mode" != local || "$memory_diagnostics" != 1 || "$worker_stack_bench" != 0 || "$worker_tls_bench" != 0 || -z "${TV4_OTA_CONTEXT:-}" ) ]]; then
  echo 'Signed CHECK bench requires configured local rev12-ota trace exclusively' >&2; exit 2
fi
if [[ "$worker_tls_bench" == 1 && ( "$profile" != rev12-ota || "$memory_diagnostics" != 1 || "$worker_stack_bench" != 1 || -n "${TV4_OTA_CONTEXT:-}" ) ]]; then
  echo 'Worker TLS bench requires unprovisioned rev12-ota with memory diagnostics=1 and worker stack bench=1' >&2; exit 2
fi
if [[ "$worker_stack_bench" == 1 && ( "$profile" != rev12-ota || "$memory_diagnostics" != 1 || -n "${TV4_OTA_CONTEXT:-}" ) ]]; then
  echo 'Worker stack bench requires unprovisioned rev12-ota with memory diagnostics=1' >&2; exit 2
fi
if [[ "$memory_diagnostics" == 1 && "$profile" != rev12-ota ]]; then
  echo 'Memory phase tracing belongs only to explicit rev12-ota' >&2; exit 2
fi
case "$profile" in headless|rev12|rev12-memory-bench|rev12-tls-bench|rev12-ota) ;; *) echo 'Profile: headless, rev12, rev12-memory-bench, rev12-tls-bench or rev12-ota' >&2; exit 2 ;; esac
case "$version" in
  5.3.2) image='espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a' ;;
  6.1.1) image='espressif/idf@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91' ;;
  *) echo 'Validated SDKs: 5.3.2 or 6.1.1' >&2; exit 2 ;;
esac
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
# IDF cannot discover a linked worktree's external .git directory inside the
# container. Carry its actual display version in; the review tool also checks
# full clean commit/tree and the app descriptor. Never invent a release SemVer.
project_version="${TV4_BUILD_PROJECT_VERSION:-}"
if [[ -z "$project_version" ]]; then
  project_version="$(git -C "$project_root" describe --always --dirty 2>/dev/null || true)"
fi
if [[ -n "$project_version" && ( ${#project_version} -gt 31 || ! "$project_version" =~ ^[A-Za-z0-9._-]+$ ) ]]; then
  echo 'Unsupported project display version; expected <=31 ASCII version characters' >&2; exit 2
fi
if [[ -n "${TV4_OTA_CONTEXT:-}" && ( "$profile" != rev12-ota || "$mode" != local ) ]]; then
  echo 'Provisioned local OTA context requires explicit rev12-ota/local; ordinary/CI profiles never receive keys' >&2; exit 2
fi
if [[ "$profile" == rev12-ota && -n "${TV4_OTA_CONTEXT:-}" ]]; then
  python3 "$project_root/tools/prepare_ota_context.py" --context "$TV4_OTA_CONTEXT" --output "$project_root/build-ota-context" >/dev/null
  project_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["installed_version"])' "$project_root/build-ota-context/ota_local_context.json")"
fi
if [[ "$mode" == docker ]]; then
  # Re-enter in the official immutable SDK container. No device passthrough.
  # Pin the descriptor limit instead of inheriting the Docker daemon's setting.
  exec docker run --rm --cap-drop=ALL --security-opt=no-new-privileges \
    --ulimit nofile=4096:4096 \
    -e "TV4_BUILD_PROJECT_VERSION=$project_version" \
    -e "TV4_OTA_MEMORY_DIAGNOSTICS=$memory_diagnostics" \
    -e "TV4_OTA_WORKER_STACK_BENCH=$worker_stack_bench" \
    -e "TV4_OTA_WORKER_TLS_BENCH=$worker_tls_bench" \
    -e "TV4_OTA_SIGNED_CHECK_BENCH=$signed_check_bench" \
    -e "TV4_REPRODUCIBLE_BUILD=$reproducible" \
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
if [[ "$reproducible" == 1 ]]; then build+="-reproducible"; fi
if [[ "$memory_diagnostics" == 1 ]]; then build+="-memory-diagnostics"; fi
if [[ "$worker_stack_bench" == 1 ]]; then build+="-worker-stack-bench"; fi
if [[ "$worker_tls_bench" == 1 ]]; then build+="-worker-tls-bench"; fi
if [[ "$signed_check_bench" == 1 ]]; then build+="-signed-check-bench"; fi
version_args=()
if [[ -n "$project_version" ]]; then version_args+=("-DPROJECT_VER=$project_version"); fi
defaults="$project_root/firmware/esp32-hmi/sdkconfig.defaults"
if [[ "$profile" != headless ]]; then defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.rev12"; fi
if [[ "$profile" == rev12-ota ]]; then defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.ota"; fi
reproducible_args=()
if [[ "$reproducible" == 1 ]]; then
  defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.reproducible"
  reproducible_args+=(--reproducible)
fi
if [[ "$profile" == rev12-memory-bench ]]; then defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.memory-bench"; fi
if [[ "$profile" == rev12-tls-bench ]]; then defaults+=";$project_root/firmware/esp32-hmi/sdkconfig.tls-bench"; fi
memory_args=()
if [[ "$memory_diagnostics" == 1 ]]; then
  mkdir -p -- "$build"
  printf '%s\n' 'CONFIG_TV4_OTA_MEMORY_DIAGNOSTICS=y' > "$build/sdkconfig.memory-diagnostics.defaults"
  if [[ "$worker_stack_bench" == 1 ]]; then
    printf '%s\n' 'CONFIG_TV4_OTA_WORKER_STACK_BENCH=y' >> "$build/sdkconfig.memory-diagnostics.defaults"
    memory_args+=(--worker-stack-bench)
  fi
  defaults+=";$build/sdkconfig.memory-diagnostics.defaults"
  if [[ "$worker_tls_bench" == 1 ]]; then
    printf '%s\n' 'CONFIG_TV4_OTA_WORKER_TLS_BENCH=y' >> "$build/sdkconfig.memory-diagnostics.defaults"
    memory_args+=(--worker-tls-bench)
  fi
  memory_args+=(--memory-diagnostics)
  if [[ "$signed_check_bench" == 1 ]]; then
    printf '%s\n' 'CONFIG_TV4_OTA_SIGNED_CHECK_BENCH=y' >> "$build/sdkconfig.memory-diagnostics.defaults"
    memory_args+=(--signed-check-bench)
  fi
fi
ota_args=()
if [[ -n "${TV4_OTA_CONTEXT:-}" ]]; then ota_args+=("-DTV4_OTA_CONTEXT=$(realpath -- "$TV4_OTA_CONTEXT")"); else ota_args+=("-DTV4_OTA_CONTEXT="); fi
idf.py -C "$project_root/firmware/esp32-hmi" -B "$build" \
  -DIDF_TARGET=esp32s3 "-DSDKCONFIG=$build/sdkconfig" "-DSDKCONFIG_DEFAULTS=$defaults" "${version_args[@]}" "${ota_args[@]}" build
python "$project_root/tools/check_idf_build.py" "$build" "$profile" "$version" "${memory_args[@]}" "${reproducible_args[@]}"

#!/usr/bin/env bash
# Compile actual firmware branches against SDK boundaries; no device/network I/O.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$root"
output="${1:-build-ota-tls-analysis}"
mkdir -p -- "$output"
compiler="${CC:-gcc}"
"$compiler" --version
common=(-std=c11 -Wall -Wextra -Werror -fanalyzer -DESP_PLATFORM=1
  -Itests/ota_tls_sdk -Itests/ota_service_sdk -Itests
  -Ifirmware/esp32-hmi/ota -Ifirmware/esp32-hmi/network)
for sdk in 5 6; do
  for standalone in 0 1; do
    worker=$((1-standalone))
    flags=(-DTLS_TEST_SDK="$sdk" -DTLS_TEST_STANDALONE="$standalone"
      -DSERVICE_TEST_OTA="$worker" -DSERVICE_TEST_DIAGNOSTICS="$worker"
      -DSERVICE_TEST_BENCH="$worker" -DSERVICE_TEST_TLS_BENCH="$worker")
    "$compiler" "${common[@]}" "${flags[@]}" -c firmware/esp32-hmi/ota/utc_tls_bench.c \
      -o "$output/utc-$sdk-$standalone.o"
    printf 'native UTC/HEAD SDK%s standalone=%s: analyzer PASS\n' "$sdk" "$standalone"
    if [[ "$standalone" == 0 ]]; then
      "$compiler" "${common[@]}" "${flags[@]}" -c firmware/esp32-hmi/ota/dma_diagnostics.c \
        -o "$output/dma-$sdk.o"
      printf 'actual DMA failure callback SDK%s: analyzer PASS\n' "$sdk"
      "$compiler" "${common[@]}" "${flags[@]}" -c firmware/esp32-hmi/ota/memory_diagnostics.c \
        -o "$output/monitor-$sdk.o"
      printf 'actual OTA monitor SDK%s: analyzer PASS\n' "$sdk"
    fi
  done
done
"$compiler" "${common[@]}" -DSERVICE_TEST_BENCH=1 -DSERVICE_TEST_TLS_BENCH=1 \
  -c firmware/esp32-hmi/ota/service.c -o "$output/service-worker-tls.o"
printf 'actual OTA worker TLS service: analyzer PASS\n'

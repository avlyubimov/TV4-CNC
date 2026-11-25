# Reproducible ESP32 OTA build

Use the official `CONFIG_APP_REPRODUCIBLE_BUILD` option only in the explicit
release build. Ordinary profiles and their output paths remain unchanged.
The SDK removes compilation date/time and maps source/build/toolchain paths.
The repo's external cJSON source/header directory has an additional narrow map
because it is outside IDF's `PROJECT_DIR`. Optimization, CPU, PSRAM, LVGL/RGB,
GT911, DMA reserve, partition layout and TLS certificate-date checks remain.

```bash
TV4_REPRODUCIBLE_BUILD=1 TV4_BUILD_PROJECT_VERSION='<reviewed-display-version>' \
  ./tools/build_idf.sh rev12-ota 6.1.1 docker
```

Output: `build-idf-6.1.1-rev12-ota-reproducible/`. The flag must be `0` or `1`;
other profiles reject `1`. Local reviewed contexts still require local mode;
Docker/CI never receive owner contexts through this wrapper. If memory tracing
is explicitly selected, its existing suffix and checks follow `-reproducible`.
The app descriptor keeps project/version/SDK/ELF hash; only its 32 date/time
bytes become zero. TLS's `MBEDTLS_HAVE_TIME_DATE` remains mandatory.

For debugging, keep the SDK-generated `gdbinit/prefix_map`. For external cJSON
also substitute `/TV4_THIRD_PARTY` with the checkout's `third_party` directory.
No debugger or device is started by these build commands.

## A pair of builds, not a configuration claim

The CI workflow exports the exact checked-out commit twice using `git archive`,
at different source/build paths. It compiles sequentially with compiler caches
disabled, in the existing immutable SDK image. Each SDK has its own independent
pair. Dependency locks stay pinned. Neither SDK is required to match the other.

```bash
python3 tools/check_idf_reproducibility.py \
  --first '<first-clean-build-directory>' --second '<second-clean-build-directory>' \
  --sdk 6.1.1 --source-commit '<full-40-character-source-sha>'
```

Use `--memory-diagnostics` only for a pair explicitly built with that option.
The checker revalidates both actual configurations, app format, descriptor,
ELF linkage, security/memory/layout gates and compares **all bytes** of app BIN,
ELF, bootloader BIN and partition table. A mismatch is a failure; no stripping,
timestamp rewriting, path-byte replacement or normalization is allowed.

The source commit argument is an external claim. This file comparator does not
authenticate source or prove separate compilation; the exact CI checkout/archive
steps and recorded real build commands establish that part of the evidence.
SDK/toolchain relocation, different host operating systems and production
performance are not claimed by two project-path builds. Build timestamps,
directories and elapsed times may differ in logs and supporting metadata;
comparison concerns deployable artifacts and the effective SDK configuration.

CI uploads only numeric equality/build evidence, not app/ELF, keys, contexts or
dumps. Local release binaries can be signed and published only after their exact
source/version/assets pass the existing owner review. This option never grants
OTA eligibility, hardware access, publication or a merge.

# Reproducible HMI artifacts for the next P1 bench

Base main 5fb177b after owner-merged PR90. Keep existing OTA/crypto/HTTPS and
runtime resource settings. No device, signing, release or merge operations.

1. Add explicit opt-in rev12-ota reproducible builds using the official SDK
   configuration, separate output directory and actual descriptor/ELF checks.
2. Build the same clean source twice at different project/build paths and
   compilation times in each pinned SDK image. Disable compiler caching. Compare
   exact app BIN, ELF, bootloader and partition bytes; never strip or normalize
   artifacts to obtain equality. Inspect unchanged security/memory/layout gates.
3. Run negative comparison/selection tests and all existing host/sanitizers.
   CI keeps the previous profile matrix and independently repeats both SDK pairs;
   no signing keys, local owner contexts or device passthrough enter CI.
4. Preserve source, toolchain image, dependency locks, sizes, SHA-256 and the
   two real build commands/results. Equal outputs do not authenticate a release
   or establish hardware performance. SDK/toolchain relocation is not claimed.

The owner separately approved new local bench-only .4→.5 settings until
27 November 2026 08:00 Asia/Almaty, retaining the existing key and resource/time
rules. Old contexts stay unchanged. Publication and a finite hardware session
still require their exact reviewable packages; this software PR grants neither.

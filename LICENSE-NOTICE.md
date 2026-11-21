# License decision pending

Do not assume an open-source license. Repository owner must select a license before external redistribution. Vendor source, images, fonts and binaries retain their respective licenses; keep attribution and do not copy unlicensed material.

Display bench adapter board/rev12.c derives RGB setup from the official Waveshare
example carrying Espressif 2022 CC0-1.0 attribution. Fonts retain the supplied
DejaVu license. Managed LVGL and Espressif touch components retain their original
licenses and are resolved by the component manager, not vendored in source releases.

The independent STM32 admission profile compiles pinned Mbed TLS 3.6.7 under
the upstream-offered Apache-2.0 option. The complete upstream dual license is
retained in third_party/mbedtls/LICENSE, with archive/source integrity metadata
in origin.json. It is not linked into the ESP32 HMI. The application license
decision above remains pending; this addition does not authorize binary releases.

P0's STM32 vector layout derives from STMicroelectronics cmsis-device-f4
v2.6.11 (commit 0fa0e489e053fa1ca7790bb40b4d76458f64c55d), Apache-2.0.
The complete upstream license and attributed reference startup are retained in
third_party/stm32f4-reference with upstream/local hashes and a change notice.
The reference is not compiled; runtime startup/register code is project-authored.
This attribution does not select an application license or authorize publishing
the real bench ELF/BIN as CI artifacts or Releases. Local owner bench artifacts
and text-only CI evidence preserve the existing release gate.

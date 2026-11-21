# STM32F407 source reference, not a runtime dependency

STMicroelectronics `cmsis-device-f4` **v2.6.11**,
commit `0fa0e489e053fa1ca7790bb40b4d76458f64c55d`, Apache-2.0.
The [license](LICENSE.md) is retained. [Origin](origin.json) pins both upstream
file hashes and the exact local copies. Only the startup reference and license
are vendored. The startup copy adds a modification notice and normalizes trailing
whitespace; its instructions are **not compiled or linked**.

The board's full vector layout derives from this reference. Our integer-only
reset/fault handlers and register subset are separate, authored bench code.
No HAL, CMSIS core implementation, SystemInit external-memory setup, libc/heap,
FPU initialization, GPIO or CAN driver is added to the executable.

DS8626 Rev12, RM0090 Rev22 and PM0214 Rev10 were reviewed from ST. Exact PDF
hashes/URLs and reviewed non-vendored ST header/SystemInit hashes are recorded
for reproducibility of the source review; PDFs and the 1.2MB header are not
vendored. The build works offline and never fetches files or accesses hardware.
The application license decision remains in [LICENSE-NOTICE.md](../../LICENSE-NOTICE.md);
this reference does not authorize binary Releases.

# Pinned Mbed TLS for STM32 admission

Mbed TLS3.6.7, official release archive and SHA-256 in [origin.json](origin.json).
[Release](https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7).
Source is downloaded into the ignored build tree, not vendored or modified.
Only the listed crypto source files are compiled, with our separately named
configuration. Header/source/license aggregate integrity is checked every build.

Upstream offers Apache-2.0 OR GPL-2.0-or-later. This verification profile uses
the Apache-2.0 option; the complete unchanged upstream [LICENSE](LICENSE) is
retained. This does not choose a license for the TV4-CNC application.

The official archive also includes four public binary data fixtures. Their
exact names/hashes are recorded in origin.json so the no-generated-binary
check can distinguish those source inputs from unexpected firmware outputs.
They are never copied into Git or upload-artifact. Private keys are generated
only in host test memory; no production trust store/signing pipeline is added.

This dependency is independent of the ESP-IDF-managed mbedTLS and is not linked
into the ESP32 HMI. No SDK component versions/configuration/locks are changed.

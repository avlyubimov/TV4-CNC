#pragma once
#include "session.h"
/* Actual ESP-IDF backend; no writer available in ordinary profiles. One worker.
 * Approval is a local copied identity chosen by the operator, never a download
 * flag. Service forbids cancellation after entering activation. */
typedef struct {
    char release[65],version[65],url[513];
    uint8_t sha256[32];uint32_t size;
} tv4_install_approval;
typedef struct {
    const void *partition;uint32_t handle;uint32_t expected,written;
    bool opened,sealed;int native_error;
    const tv4_install_approval *approval;
    bool (*commit_allowed)(void *);void *owner;
} tv4_ota_sdk;
bool tv4_ota_sdk_layout_valid(void);
/* Boot worker supplies the bounded HMI/touch/PSRAM self-test result. A pending
 * image is accepted only on success; otherwise SDK rollback/reboot is invoked.
 * False requires a caller reset if SDK rollback could not reboot. */
bool tv4_ota_sdk_finish_boot(bool healthy);
bool tv4_ota_sdk_begin(void *,const tv4_ota_candidate *);
bool tv4_ota_sdk_write(void *,const uint8_t *,size_t);
bool tv4_ota_sdk_seal(void *);
bool tv4_ota_sdk_activate(void *);
void tv4_ota_sdk_abort(void *);

#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {TV4_UPDATE_OFF,TV4_UPDATE_SETUP,TV4_UPDATE_BOOT,TV4_UPDATE_IDLE,
 TV4_UPDATE_CHECKING,TV4_UPDATE_READY,TV4_UPDATE_WRITING,TV4_UPDATE_COMMIT,
 TV4_UPDATE_RESTART,TV4_UPDATE_ERROR,TV4_UPDATE_BOOT_FAILED} tv4_update_state;
typedef enum {TV4_UPDATE_CHECK,TV4_UPDATE_INSTALL,TV4_UPDATE_CANCEL,TV4_UPDATE_REBOOT} tv4_update_command;
typedef struct {
    bool enabled,stack_valid,install_allowed;tv4_update_state state;unsigned error;
    uint32_t received,total,stack_watermark;char version[65];uint8_t sha256[32];
} tv4_update_snapshot;
/* Internal boot-worker window, exposed for host fault tests. A PASS is only
 * permission to run PSRAM/layout checks, never a boot activation authority. */
typedef struct {uint32_t started,continuous,checked;bool observing;} tv4_ota_boot_window;
typedef enum {TV4_BOOT_WAIT,TV4_BOOT_CHECK_MEMORY,TV4_BOOT_TIMEOUT} tv4_ota_boot_result;
tv4_ota_boot_result tv4_ota_boot_observe(tv4_ota_boot_window *,uint32_t now,bool healthy,uint32_t last_health);
/* Start/health only from app_main; worker owns all network/Flash/rollback I/O.
 * Commands are nonblocking bounded posts. Snapshot is a short critical copy.
 * No caller-supplied success/keys/versions or machine authority. */
bool tv4_ota_service_start(void);
void tv4_ota_service_health(bool);
void tv4_ota_service_snapshot(tv4_update_snapshot *);
bool tv4_ota_service_request(tv4_update_command);

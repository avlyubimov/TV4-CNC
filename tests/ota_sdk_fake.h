#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
typedef uint32_t esp_ota_handle_t;
typedef struct {unsigned type,subtype;uint32_t address,size;const char *label;bool encrypted;} esp_partition_t;
/* Match the pinned ESP-IDF states, including the reset-before-confirmation path. */
typedef enum {
    ESP_OTA_IMG_NEW=0, ESP_OTA_IMG_PENDING_VERIFY=1, ESP_OTA_IMG_VALID=2,
    ESP_OTA_IMG_INVALID=3, ESP_OTA_IMG_ABORTED=4, ESP_OTA_IMG_UNDEFINED=UINT32_MAX
} esp_ota_img_states_t;
#define ESP_OK 0
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_FACTORY 0
esp_err_t esp_flash_get_size(void *,uint32_t *);
esp_err_t esp_flash_get_physical_size(void *,uint32_t *);
const esp_partition_t *esp_partition_find_first(unsigned,unsigned,const char *);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_boot_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);

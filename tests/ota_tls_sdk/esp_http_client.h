#pragma once
#include "ota_service_sdk_fake.h"
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
enum {HTTP_METHOD_HEAD=2,HTTP_TRANSPORT_OVER_SSL=2,HTTP_AUTH_TYPE_NONE=0,
      HTTP_EVENT_DISCONNECTED=6,HTTP_EVENT_ON_HEADER=3};
typedef struct fake_http_client *esp_http_client_handle_t;
typedef struct {int event_id;void *data,*user_data;const char *header_key,*header_value;} esp_http_client_event_t;
typedef struct {
    const char *url;int method,transport_type;
    esp_err_t (*crt_bundle_attach)(void *);
    bool skip_cert_common_name_check,disable_auto_redirect;
    int auth_type,max_authorization_retries,timeout_ms,buffer_size,buffer_size_tx;
    bool keep_alive_enable;esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data;const char *user_agent;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_open(esp_http_client_handle_t,int);
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t,int *,int *);

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "wifi_profiles.h"
#define WIFI_UI_NETWORKS 16
/* No credentials in snapshots or logs. All control is local, station-only. */
typedef enum { NET_OFF, NET_IDLE, NET_SCANNING, NET_CONNECTING, NET_ONLINE, NET_FAILED } net_state_t;
typedef struct {
    uint32_t connect_attempts, disconnect_events, ip_events, queue_drops, stack_watermark;
    net_state_t state; unsigned revision,count,saved_count; int error,storage_error;
    char ip[16],active_ssid[33],saved_ssids[WIFI_SAVED_MAX][33];
    char ssids[WIFI_UI_NETWORKS][33]; int8_t rssi[WIFI_UI_NETWORKS]; bool secured[WIFI_UI_NETWORKS];
} wifi_snapshot_t;
bool wifi_service_start(void);
bool wifi_service_scan(void);
bool wifi_service_connect(const char *ssid,const char *password);
bool wifi_service_disconnect(void);
bool wifi_service_snapshot(wifi_snapshot_t *out);
/* Nonblocking advisory NET_ONLINE check without copying the UI network list.
 * False if the service/lock is unavailable; not a guarantee for the next I/O. */
bool wifi_service_online(void);

bool wifi_credentials_valid(const char *ssid,const char *password);

bool wifi_service_connect_saved(const char *ssid);
bool wifi_service_forget(const char *ssid);

#ifndef ESP_PLATFORM
/* Host UI tests only: no network activity. NULL restores the offline backend. */
void wifi_service_host_fixture(const wifi_snapshot_t *value);
#endif

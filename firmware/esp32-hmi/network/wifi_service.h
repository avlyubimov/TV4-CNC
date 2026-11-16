#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WIFI_UI_NETWORKS 6
/* No credentials in snapshots or logs. All control is local, station-only. */
typedef enum { NET_OFF, NET_IDLE, NET_SCANNING, NET_CONNECTING, NET_ONLINE, NET_FAILED } net_state_t;
typedef struct { net_state_t state; unsigned revision,count; int error; char ip[16]; char ssids[WIFI_UI_NETWORKS][33]; int8_t rssi[WIFI_UI_NETWORKS]; } wifi_snapshot_t;
bool wifi_service_start(void);
bool wifi_service_scan(void);
bool wifi_service_connect(const char *ssid,const char *password);
bool wifi_service_disconnect(void);
bool wifi_service_snapshot(wifi_snapshot_t *out);

bool wifi_credentials_valid(const char *ssid,const char *password);

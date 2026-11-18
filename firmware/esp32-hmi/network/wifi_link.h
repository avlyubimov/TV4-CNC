#pragma once
#include <stdbool.h>
typedef struct {
    void *context;
    bool (*associated)(void *context);
    bool (*disconnect)(void *context);
    bool (*wait_disconnected)(void *context);
    bool (*clear_config)(void *context);
} wifi_link_ops_t;
/* Idle first connection must not wait for a disconnect event that may never occur. */
bool wifi_link_prepare(const wifi_link_ops_t *ops,bool connecting);
typedef enum { WIFI_PROBLEM_NONE, WIFI_PROBLEM_TIMEOUT, WIFI_PROBLEM_AUTH,
    WIFI_PROBLEM_NOT_FOUND, WIFI_PROBLEM_SIGNAL, WIFI_PROBLEM_SECURITY,
    WIFI_PROBLEM_REJECTED, WIFI_PROBLEM_OTHER } wifi_problem_t;
wifi_problem_t wifi_problem_from_code(int code);

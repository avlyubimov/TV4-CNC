#include "wifi_link.h"
bool wifi_link_prepare(const wifi_link_ops_t *ops,bool connecting) {
    if(ops->associated(ops->context) || connecting) {
        if(!ops->disconnect(ops->context))return false;
        if(!ops->wait_disconnected(ops->context) && ops->associated(ops->context))return false;
    }
    return ops->clear_config(ops->context);
}
wifi_problem_t wifi_problem_from_code(int code) {
    switch(code) {
    case 0:return WIFI_PROBLEM_NONE;
    case 263:return WIFI_PROBLEM_TIMEOUT; /* ESP_ERR_TIMEOUT, stable across supported SDKs. */
    case 2:case 15:case 23:case 202:case 204:return WIFI_PROBLEM_AUTH;
    case 201:return WIFI_PROBLEM_NOT_FOUND;
    case 200:return WIFI_PROBLEM_SIGNAL;
    case 210:case 211:return WIFI_PROBLEM_SECURITY;
    case 203:case 205:return WIFI_PROBLEM_REJECTED;
    default:return WIFI_PROBLEM_OTHER;
    }
}

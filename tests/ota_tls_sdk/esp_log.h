#pragma once
#include "ota_service_sdk_fake.h"
#define ESP_LOGW(tag,...) service_test_log(tag,__VA_ARGS__)
#define ESP_LOGE(tag,...) service_test_log(tag,__VA_ARGS__)

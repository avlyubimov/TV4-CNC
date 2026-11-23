#pragma once
#include "ota_service_sdk_fake.h"
void vTaskDelete(TaskHandle_t);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xPortInIsrContext(void);

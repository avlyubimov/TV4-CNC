#include "export.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
static QueueHandle_t queue;
static StaticQueue_t queue_state;
static uint8_t storage[sizeof(diag_record_t)];
static void worker(void *arg) {
    (void)arg;diag_record_t record;char json[1536];
    for(;;)if(xQueueReceive(queue,&record,portMAX_DELAY)==pdTRUE) {
        record.export_stack=uxTaskGetStackHighWaterMark(NULL);
        if(diagnostics_format(&record,json,sizeof(json)))printf("tv4_diag %s\n",json);
    }
}
bool diag_export_start(void) {
    if(queue)return true;
    queue=xQueueCreateStatic(1,sizeof(diag_record_t),storage,&queue_state);
    if(!queue)return false;
    if(xTaskCreate(worker,"tv4_export",4096,NULL,1,NULL)!=pdPASS) {vQueueDelete(queue);queue=NULL;return false;}
    return true;
}
bool diag_export_submit(const diag_record_t *record) {
    return record && queue && xQueueSend(queue,record,0)==pdTRUE;
}
